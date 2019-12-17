/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "Mona/FlashStream.h"
#include "Mona/FLVReader.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

FlashStream::FlashStream(UInt16 id, ServerAPI& api, Peer& peer, bool amf0) : amf0(amf0), id(id), api(api), peer(peer), _pPublication(NULL), _pSubscription(NULL), _bufferTime(0) {
	DEBUG("FlashStream ", id," created")
}

FlashStream::~FlashStream() {
	disengage(NULL);
	DEBUG("FlashStream ", name()," deleted")
}

void FlashStream::flush() {
	if (_pPublication)
		_pPublication->flush(peer.ping());
	if (_pSubscription && _pSubscription->ejected())
		disengage(&_pSubscription->target<FlashWriter>());
}

void FlashStream::disengage(FlashWriter* pWriter) {
	// Stop the current  job
	if(_pPublication) {
		if (pWriter)
			pWriter->writeAMFStatus("NetStream.Unpublish.Success", _pPublication->name() + " is now unpublished");
		 // do after writeAMFStatus because can delete the publication, so corrupt name reference
		api.unpublish(*_pPublication, peer);
		_pPublication = NULL;
	}
	if(_pSubscription) {
		const string& name(_pSubscription->name());
		if (pWriter) {
			switch (_pSubscription->ejected()) {
				case Subscription::EJECTED_BANDWITDH:
					pWriter->writeAMFStatus("NetStream.Play.InsufficientBW", "error", "Insufficient bandwidth to play " + name);
					break;
				case Subscription::EJECTED_ERROR:
					pWriter->writeAMFStatus("NetStream.Play.Failed", "error", "Unknown error to play " + name);
					break;
				default:;
			}
			pWriter->writeAMFStatus("NetStream.Play.Stop", "Stopped playing " + name);
			onStop(id, *pWriter); // stream end
		}
		 // do after writeAMFStatus because can delete the publication, so corrupt publication name reference
		api.unsubscribe(peer, *_pSubscription);
		delete _pSubscription;
		_pSubscription = NULL;
	}
}

bool FlashStream::process(AMF::Type type, UInt32 time, const Packet& packet, FlashWriter& writer, Net::Stats& netStats) {

	writer.amf0 = amf0;

	// if exception, it closes the connection, and print an ERROR message
	switch(type) {

		case AMF::TYPE_AUDIO:
			audioHandler(time,packet);
			break;
		case AMF::TYPE_VIDEO:
			videoHandler(time,packet);
			break;
		case AMF::TYPE_DATA_AMF3:
			dataHandler(time, packet+1);
			break;
		case AMF::TYPE_DATA:
			dataHandler(time, packet);
			break;
		case AMF::TYPE_INVOCATION_AMF3:
		case AMF::TYPE_INVOCATION: {
			string name;
			AMFReader reader(packet + (type & 1)); // packet+1 for TYPE_INVOCATION_AMF3
			reader.readString(name);
			double number(0);
			reader.readNumber(number);
			reader.readNull();
			writer.setCallbackHandle(number);
			messageHandler(name,reader,writer, netStats);
			writer.resetCallbackHandle();
			break;
		}
		case AMF::TYPE_RAW:
			rawHandler(BinaryReader(packet.data(), packet.size()).read16(), packet+2, writer);
			break;

		case AMF::TYPE_EMPTY:
			break;

		default:
			ERROR("Unpacking type '",String::Format<UInt8>("%02x",(UInt8)type),"' unknown");
	}

	return writer ? true : false;
}


UInt32 FlashStream::bufferTime(UInt32 ms) {
	_bufferTime = ms;
	INFO("setBufferTime ", ms, "ms on stream ", name())
	if (_pSubscription)
		_pSubscription->setNumber("bufferTime", ms);
	return _bufferTime;
}

void FlashStream::messageHandler(const string& method, AMFReader& message, FlashWriter& writer, Net::Stats& netStats) {
	if (method == "play") {
		disengage(&writer);

		message.readString(_name);
		Exception ex;
		_pSubscription = new Subscription(writer);
		
		if (!api.subscribe(ex, peer, _name, *_pSubscription)) {
			if(ex.cast<Ex::Unfound>())
				writer.writeAMFStatus("NetStream.Play.StreamNotFound", "error", ex);
			else
				writer.writeAMFStatus("NetStream.Play.Failed", "error", ex);
			delete _pSubscription;
			_pSubscription = NULL;
			return;
		}
		// support a format data!
		const char* format = _pSubscription->getString("format");
		if (format)
			_pSubscription->setFormat(format);

		onStart(id, writer); // stream begin
		writer.writeAMFStatus("NetStream.Play.Reset", "Playing and resetting " + _name); // for entiere playlist
		writer.writeAMFStatus("NetStream.Play.Start", "Started playing "+ _name); // for item
		AMFWriter& amf(writer.writeAMFData("|RtmpSampleAccess"));
		amf.writeBoolean(true); // audioSampleAccess
		amf.writeBoolean(true); // videoSampleAccess

		if (_bufferTime)
			_pSubscription->setNumber("bufferTime", _bufferTime);

		return;
	}
	
	if (method == "closeStream") {
		disengage(&writer);
		return;
	}
	
	if (method == "publish") {

		disengage(&writer);

		string type;
		message.readString(_name);
		if (message.readString(type)) {
			if(String::ICompare(type, "append")==0) {
				_name += _name.find('?') == string::npos ? '?' : '&';
				_name += "append=true";
			} else if (String::ICompare(type, "record") == 0) {
				// if stream has no extension, add a FLV extension!
				size_t found = _name.find('?');
				size_t point = _name.find_last_of('.', found);
				if (point == string::npos) {
					if (found == string::npos)
						_name += ".flv";
					else
						_name.insert(found, ".flv");
				}
			}
		};

		Exception ex;
		_pPublication = api.publish(ex, peer, _name);
		if (_pPublication) {
			writer.writeAMFStatus("NetStream.Publish.Start", _name + " is now published");
			_audioTrack = _videoTrack = 1;
			_audioConfig.reset();
			_dataTrack = 0;
			if (_pPublication->recording()) {
				MediaStream* pRecorder = _pPublication->recorder();
				pRecorder->onRunning = [this, &writer]() {
					writer.writeAMFStatus("NetStream.Record.Start", _name + " recording started");
				};
				pRecorder->onStop = [this, &writer, pRecorder]() {
					if(pRecorder->ex)
						writer.writeAMFStatus("NetStream.Record.Failed", "error", pRecorder->ex);
					writer.writeAMFStatus("NetStream.Record.Stop", _pPublication->name() + " recording stopped");
					writer.flush();
				};
			} else if (ex) {
				// recording pb!
				if (ex.cast<Ex::Unsupported>())
					writer.writeAMFStatus("NetStream.Record.Failed", "error", ex);
				else
					writer.writeAMFStatus("NetStream.Record.NoAccess", "error", ex);
			}
		} else {
			writer.writeAMFStatus("NetStream.Publish.BadName", "error", ex);
			//writer.close();
		}
		return;
	}
	
	if (_pSubscription) {

		if(method == "receiveAudio") {
			bool enable(true);
			message.readBoolean(enable);
			_pSubscription->setBoolean("audio", enable);
			return;
		}
		
		if (method == "receiveVideo") {
			bool enable(true);
			message.readBoolean(enable);
			_pSubscription->setBoolean("video", enable);
			return;
		}
		
		if (method == "pause") {
			bool paused(true);
			message.readBoolean(paused);
			// TODO support pause for VOD
		
			if (paused) {
				// useless, client knows it when it calls NetStream::pause method
				// writer.writeAMFStatus("NetStream.Pause.Notify", _pListener->publication.name() + " paused");
			} else {
				UInt32 position;
				if (message.readNumber(position))
					_pSubscription->setNumber("time", position);
				onStart(id, writer); // stream begin
				// useless, client knows it when it calls NetStream::resume method
				//	writer.writeAMFStatus("NetStream.Unpause.Notify", _pListener->publication.name() + " resumed");
			}
			return;
		}
		
		if (method == "seek") {
			UInt32 position;
			if (message.readNumber(position)) {
				_pSubscription->setNumber("time", position);
				 // TODO support seek for VOD
				onStart(id, writer); // stream begin
				// useless, client knows it when it calls NetStream::seek method, and wait "NetStream.Seek.Complete" rather (raised by client side)
				// writer.writeAMFStatus("NetStream.Seek.Notify", _pListener->publication.name() + " seek operation");
			} else
				writer.writeAMFStatus("NetStream.Seek.InvalidTime", "error", _pSubscription->name() + string(" seek operation must pass in argument a milliseconds position time"));
			return;
		}
	}

	ERROR("Message '", method,"' unknown on stream ", name());
}

void FlashStream::dataHandler(UInt32 timestamp, const Packet& packet) {
	if (!packet)
		return; // to expect recursivity of dataHandler (see code below)

	if(!_pPublication) {
		ERROR("a data packet has been received on a no publishing stream ", name(),", certainly a publication currently closing");
		return;
	}

	// fast checking, necessary AMF0 here!
	if (*packet.data() == AMF::AMF0_NULL) {
		// IF NetStream.send(Null,...) => manual publish

		AMFReader reader(packet);
		reader.readNull();
		Packet bytes;
		if (reader.readByte(bytes)) {
			// netStream.send(null, tag as ByteArray , data as ByteArray, ...)
			// => audio / video / data
			Media::Audio::Tag  audio;
			Media::Video::Tag  video;
			Media::Data::Type  data;
			UInt8			   track;
			BinaryReader tag(bytes.data(), bytes.size());
			Media::Type type = Media::Unpack(tag, audio, video, data, track);
			switch (type) {
				case Media::TYPE_AUDIO:
					audio.time = timestamp;
					_audioTrack = track; // trick to fix _audioTrack of NetStream audio (call without bytes)
					while (reader.readByte(bytes)) {
						_pPublication->writeAudio(audio, bytes, track);
						_audioTrack = 1;
					}
					return dataHandler(timestamp, packet + reader->position());
				case Media::TYPE_VIDEO:
					video.time = timestamp;
					_videoTrack = track; // trick to fix _videoTrack of NetStream video (call without bytes)
					while (reader.readByte(bytes)) {
						_pPublication->writeVideo(video, bytes, track);
						_videoTrack = 1;
					}
					return dataHandler(timestamp, packet + reader->position());
				case Media::TYPE_DATA:
					_dataTrack = track; // trick to fix _dataTrack of NetStream data (call without bytes)
					while (reader.readByte(bytes)) {
						_pPublication->writeData(data, bytes, track);
						_dataTrack = 0;
					}
					break;
				default:
					ERROR("Malformed media header size");
			}
			return;
		}

		if (reader.nextType() == DataReader::NIL) {
			// To allow a handler null with a bytearray or string following
			_pPublication->writeData(Media::Data::TYPE_AMF, packet + reader->position(), _dataTrack);
			return;
		}
		
	} else if (*packet.data() == AMF::AMF0_STRING && packet.size()>3 && *(packet.data() + 3) == '@' && *(packet.data() + 1) == 0) {

		switch (*(packet.data() + 2)) {
			case 15:
				if (memcmp(packet.data() + 3, EXPAND("@clearDataFrame")) != 0)
					break;
				_pPublication->clear();
				return;
			case 13: {
				if (memcmp(packet.data() + 3, EXPAND("@setDataFrame")) != 0)
					break;
				// @setDataFrame
				AMFReader reader(packet);
				reader.next(); // @setDataFrame
				if (reader.nextType() == DataReader::STRING)
					reader.next(); // remove onMetaData
				_pPublication->addProperties(_dataTrack, Media::Data::TYPE_AMF, packet + reader->position());
				return;
			}
		}
	}

	_pPublication->writeData(Media::Data::TYPE_AMF, packet, _dataTrack);
}

void FlashStream::rawHandler(UInt16 type, const Packet& packet, FlashWriter& writer) {
	if(type==0x0022) { // TODO Here we receive RTMFP flow sync signal, useless to support it!
		//TRACE("Sync ",id," : ",data.read32(),"/",data.read32());
		return;
	}
	ERROR("Raw message ",String::Format<UInt16>("%.4x",type)," unknown on stream ", name());
}

void FlashStream::audioHandler(UInt32 timestamp, const Packet& packet) {
	if(!_pPublication) {
		WARN("an audio packet has been received on a no publishing stream ", name(),", certainly a publication currently closing");
		return;
	}

	_audio.time = timestamp;
	_pPublication->writeAudio(_audio, packet + FLVReader::ReadMediaHeader(packet.data(), packet.size(), _audio, _audioConfig), _audioTrack);
}

void FlashStream::videoHandler(UInt32 timestamp, const Packet& packet) {
	if(!_pPublication) {
		WARN("a video packet has been received on a no publishing stream ", name(),", certainly a publication currently closing");
		return;
	}

	_video.time = timestamp;
	UInt32 readen(FLVReader::ReadMediaHeader(packet.data(), packet.size(), _video));
	if (_video.frame == Media::Video::FRAME_CONFIG && (_video.codec == Media::Video::CODEC_H264 || _video.codec == Media::Video::CODEC_HEVC)) {
		shared<Buffer> pBuffer(SET);
		if (_video.codec == Media::Video::CODEC_HEVC)
			readen += HEVC::ReadVideoConfig(packet.data() + readen, packet.size() - readen, *pBuffer);
		else
			readen += AVC::ReadVideoConfig(packet.data() + readen, packet.size() - readen, *pBuffer);
		_pPublication->writeVideo(_video, Packet(pBuffer), _videoTrack);
		if (packet.size() <= readen)
			return; //rest nothing
	}
	_pPublication->writeVideo(_video, packet+readen, _videoTrack);
}



} // namespace Mona
