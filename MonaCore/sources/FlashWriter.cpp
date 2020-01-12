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

#include "Mona/FlashWriter.h"
#include "Mona/Session.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/FLVWriter.h"
#include "Mona/StringWriter.h"


using namespace std;


namespace Mona {


FlashWriter::FlashWriter() : _callbackHandleOnAbort(0),_callbackHandle(0), amf0(false), isMain(false), _lastTime(0) {
}

void FlashWriter::closing(Int32 error, const char* reason) {
	if (!isMain || error <= 0)
		return;
	switch(error) {
		case Session::ERROR_UPDATE:
			writeAMFError("NetConnection.Connect.AppShutdown", reason ? reason : "Server update");
			break;
		case Session::ERROR_SERVER:
			writeAMFError("NetConnection.Connect.AppShutdown", reason ? reason : "Server shutdown");
			break;
		case Session::ERROR_REJECTED:
			writeAMFError("NetConnection.Connect.Rejected", reason ? reason : "Client rejected");
			break;
		case Session::ERROR_IDLE:
			writeAMFError("NetConnection.Connect.IdleTimeout", reason ? reason : "Client idle timeout");
			break;
		case Session::ERROR_UNFOUND:
			writeAMFError("NetConnection.Connect.InvalidApp", reason ? reason : "Application unfound");
			break;
		case Session::ERROR_APPLICATION:
			writeAMFError("NetConnection.Connect.Failed", reason ? reason : "Application error");
			break;
		case Session::ERROR_PROTOCOL:
			writeAMFError("NetConnection.Connect.Failed", reason ? reason : "Protocol error");
			break;
		case Session::ERROR_UNEXPECTED:
			writeAMFError("NetConnection.Connect.Failed", reason ? reason : "Unknown error");
			break;
		default: // User code
			writeAMFError("NetConnection.Connect.Failed", reason ? reason : String("Error ", error));
	}
}

AMFWriter& FlashWriter::writeMessage() {
	AMFWriter& writer(writeInvocation("_result",_callbackHandleOnAbort = _callbackHandle));
	_callbackHandle = 0;
	return writer;
}

void FlashWriter::writeRaw(DataReader& arguments, const Packet& packet) {
	UInt8 type(AMF::TYPE_INVOCATION);
	UInt32 time(0);
	if (arguments.readNumber(type))
		arguments.readNumber(time);
	// Media::Data::TYPE_UNKNOWN for AMF invocation or data to convert in AMF Bytes, otherwise on control message write the content in raw!
	Media::Data::Type packetType(type >= AMF::TYPE_DATA_AMF3 ? Media::Data::TYPE_UNKNOWN : Media::Data::TYPE_AMF);
	DataWriter& amf = write(AMF::Type(type), time, packetType, packet);
	switch (type) {
		case AMF::TYPE_INVOCATION_AMF3:
			amf->write8(AMF::AMF0_AMF3_OBJECT);
		case AMF::TYPE_INVOCATION:
			amf.writeString(EXPAND("onRaw"));
			amf.writeNumber(0);
			amf->write8(AMF::AMF0_NULL); // for RTMP compatibility! (requiere it)
		default:;
	}
	if (packetType) { // if is not a Data or invocation so convert all in binary!
		StringWriter<> writer(amf->buffer());
		arguments.read(writer);
	} else // else we can convert it to AMF!
		arguments.read(amf);
}

AMFWriter& FlashWriter::writeInvocation(const char* name, double callback) {
	AMFWriter& writer(write(AMF::TYPE_INVOCATION));
	writer.amf0 = true;
	writer.writeString(name, strlen(name));
	writer.writeNumber(callback);
	writer->write8(AMF::AMF0_NULL); // for RTMP compatibility! (requiere it)
	writer.amf0 = amf0;
	return writer;
}

AMFWriter& FlashWriter::writeAMFState(const char* name, const char* code, const char* level, const string& description, bool withoutClosing) {
	AMFWriter& writer = (AMFWriter&)writeInvocation(name, _callbackHandleOnAbort = _callbackHandle);
	_callbackHandle = 0;
	writer.amf0 = true;
	writer.beginObject();
	writer.writeStringProperty("level", level);
	writer.writeStringProperty("code",code);
	writer.writeStringProperty("description", description);
	writer.amf0 = amf0;
	if(!withoutClosing)
		writer.endObject();
	return writer;
}

AMFWriter& FlashWriter::writeAMFData(const string& name) {
	AMFWriter& writer(write(AMF::TYPE_DATA));
	writer.amf0 = true;
	writer.writeString(name.data(),name.size());
	writer.amf0 = amf0;
	return writer;
}

bool FlashWriter::beginMedia(const string& name) {
	_pPublicationName = &name;
	writeAMFStatus("NetStream.Play.PublishNotify", name + " is now published");
	_firstAV = true;
	return !closed();
}

bool FlashWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	_time = tag.time + _lastTime;
	_firstAV = false;
	BinaryWriter& writer(*write(AMF::TYPE_AUDIO, _time, Media::Data::TYPE_AMF, packet, reliable));
	if (!packet)
		return !closed(); // "end audio signal" => detected by FP and works just if no packet content!
	writer.write8(FLVWriter::ToCodecs(tag));
	if (tag.codec == Media::Audio::CODEC_AAC)
		writer.write8(tag.isConfig ? 0 : 1);
	return !closed();
}

bool FlashWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	_time = tag.time + _lastTime;
	if (_firstAV) {
		_firstAV = false;
		// patch for flash, to avoid to wait audio, an empty audio packet has to be sent ( = "end audio signal")
		write(AMF::TYPE_AUDIO, _time, reliable);
	}
	bool isAVCConfig(tag.frame == Media::Video::FRAME_CONFIG && ((tag.codec == Media::Video::CODEC_H264 && AVC::ParseVideoConfig(packet, _sps, _pps)) || (tag.codec == Media::Video::CODEC_HEVC && HEVC::ParseVideoConfig(packet, _vps, _sps, _pps))));
	BinaryWriter& writer(*write(AMF::TYPE_VIDEO, _time, Media::Data::TYPE_AMF, isAVCConfig ? Packet::Null() : packet, reliable));
	writer.write8(FLVWriter::ToCodecs(tag));
	if (tag.codec != Media::Video::CODEC_H264 && tag.codec != Media::Video::CODEC_HEVC)
		return !closed();
	if (isAVCConfig) {
		if (tag.codec == Media::Video::CODEC_HEVC)
			HEVC::WriteVideoConfig(writer.write8(0).write24(tag.compositionOffset), _vps, _sps, _pps);
		else
			AVC::WriteVideoConfig(writer.write8(0).write24(tag.compositionOffset), _sps, _pps);
	} else
		writer.write8(1).write24(tag.compositionOffset);
	return !closed();
}

bool FlashWriter::writeData(Media::Data::Type type, const Packet& packet, bool reliable) {
	// Always give 0 here for time, otherwise RTMP or RTMFP can't receive the data (tested..)
	AMFWriter& writer(write(AMF::TYPE_DATA, 0, type, packet, reliable));
	if (type == Media::Data::TYPE_AMF && packet && (*packet.data() == AMF::AMF0_STRING || *packet.data() == AMF::AMF0_LONG_STRING))
		return true; // has already correct (AMF0) handler!
	// Handler required (else can't be received in flash)
	writer.amf0 = true;
	if(type==Media::Data::TYPE_TEXT)
		writer.writeString(EXPAND("onTextData"));
	else if (type == Media::Data::TYPE_MEDIA)
		writer.writeString(EXPAND("onMedia"));
	else
		writer.writeString(EXPAND("onData")); // onData(data as ByteArray)
	writer.amf0 = amf0;
	return !closed();
}

bool FlashWriter::writeProperties(const Media::Properties& properties) {
	// Always give 0 here for time, otherwise RTMP or RTMFP can't receive the data (tested..)
	Media::Data::Type type(amf0 ? Media::Data::TYPE_AMF0 : Media::Data::TYPE_AMF);
	AMFWriter& writer(write(AMF::TYPE_DATA, 0, Media::Data::TYPE_AMF, properties.data(type), true));
	// Handler required (else can't be received in flash)
	writer.amf0 = true;
	writer.writeString(EXPAND("onMetaData"));
	writer.amf0 = amf0;
	return !closed();
}

bool FlashWriter::endMedia() {
	_lastTime = _time;
	writeAMFStatus("NetStream.Play.UnpublishNotify", *_pPublicationName + " is now unpublished");
	return !closed();
}


} // namespace Mona
