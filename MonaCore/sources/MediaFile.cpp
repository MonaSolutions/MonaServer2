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

#include "Mona/MediaFile.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {

MediaFile::Reader* MediaFile::Reader::New(const Path& path, const char* subMime, const Timer& timer, IOFile& io) {
	MediaReader* pReader = MediaReader::New(subMime);
	return pReader ? new MediaFile::Reader(path, pReader, timer, io) : NULL;
}

UInt32 MediaFile::Reader::Decoder::decode(shared<Buffer>& pBuffer, bool end) {
	DUMP_RESPONSE(_name.c_str(), pBuffer->data(), pBuffer->size(), _path);
	Packet packet(pBuffer); // to capture pBuffer!
	if (!_pReader || _pReader.unique())
		return 0;
	_mediaTimeGotten = false;
	_pReader->read(packet, *this);
	if (end)
		_pReader.reset(); // no flush, because will be done by the main thread!
	else if(!_mediaTimeGotten)
		return packet.size(); // continue to read if no flush
	_handler.queue(onFlush);
	return 0;
}

MediaFile::Reader::Reader(const Path& path, MediaReader* pReader, const Timer& timer, IOFile& io) :
	Media::Stream(TYPE_FILE, path), io(io), _pReader(pReader), timer(timer), _pMedias(new deque<unique<Media::Base>>()),
		_onTimer([this](UInt32 delay) {
			unique<Media::Base> pMedia;
			while (!_pMedias->empty()) {
				pMedia = move(_pMedias->front());
				_pMedias->pop_front();
				if (pMedia) {
					if (*pMedia || typeid(*pMedia) != typeid(Lost)) {
						_pSource->writeMedia(*pMedia);
						UInt32 time;
						switch (pMedia->type) {
							default: continue;
							case Media::TYPE_AUDIO: {
								const Media::Audio& audio = (const Media::Audio&)*pMedia;
								if (audio.tag.isConfig)
									continue; // skip this unrendered packet and where time is sometimes unprecise
								time = audio.tag.time;
								break;
							}
							case Media::TYPE_VIDEO: {
								const Media::Video& video = (const Media::Video&)*pMedia;
								if (video.tag.frame == Media::Video::FRAME_CONFIG)
									continue; // skip this unrendered packet and where time is sometimes unprecise
								time = ((const Media::Video&)*pMedia).tag.time;
								break;
							}
						}
						if (!_realTime) {
							_realTime.update(Time::Now() - time);
							continue;
						}
						Int32 delta = Int32(time - _realTime.elapsed());
						if (abs(delta) > 1000) {
							// more than 1 fps means certainly a timestamp progression error..
							WARN(description(), " resets horloge (delta time of ", delta, "ms)");
							_realTime.update(Time::Now() - time);
							continue;
						}
						if (delta<20) // 20 ms for timer performance reason (to limit timer raising), not more otherwise not progressive (and player load data by wave)
							continue;
						_pSource->flush();
						this->timer.set(_onTimer, delta);
						return 0; // pause!
					}
					_pSource->reportLost(pMedia->type, (Lost&)(*pMedia), pMedia->track);
				} else 
					_pSource->reset();
			}
			if (_pReader.unique()) {
				// end of file!
				stop();
				return 0;
			}
			if(pMedia)
				_pSource->flush(); // flush before because reading can take time (and to avoid too large amout of data transfer)
			this->io.read(_pFile); // continue to read immediatly
			return 0;
		}) {
	_onFileError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
}

void MediaFile::Reader::start(const Parameters& parameters) {
	if (!_pSource) {
		Stream::stop<Ex::Intern>(LOG_ERROR, "call start(Media::Source& source) in first");
		return;
	}
	if (_pFile)
		return;
	_realTime =	0; // reset realTime
	Decoder* pDecoder = new Decoder(io.handler, _pReader, path, _pSource->name(), _pMedias);
	pDecoder->onFlush = _onFlush = [this]() { _onTimer(); };
	_pFile.reset(new File(path, File::MODE_READ));
	io.subscribe(_pFile, pDecoder, nullptr, _onFileError);
	io.read(_pFile);
	INFO(description(), " starts");
}

void MediaFile::Reader::stop() {
	if (!_pFile)
		return;
	timer.set(_onTimer, 0);
	io.unsubscribe(_pFile);
	_onFlush = nullptr;
	_pMedias.reset(new std::deque<unique<Media::Base>>());
	INFO(description(), " stops"); // to display "stops" before possible "publication reset"
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if (_pReader.unique())
		_pReader->flush(*_pSource);
	else
		_pSource->reset(); // because the source.reset of _pReader->flush() can't be called (parallel!)
	_pReader.reset(MediaReader::New(_pReader->subMime()));
}





MediaFile::Writer* MediaFile::Writer::New(const Path& path, const char* subMime, IOFile& io) {
	//if (String::ICompare(subMime, EXPAND("m3u8")) == 0 || String::ICompare(subMime, EXPAND("x-mpegURL")) == 0)
	//	return new MediaHLS::Writer(path, io);
	MediaWriter* pWriter = MediaWriter::New(subMime);
	return pWriter ? new MediaFile::Writer(path, pWriter, io) : NULL;
}

MediaFile::Writer::Write::Write(const shared<string>& pName, IOFile& io, const shared<File>& pFile, const shared<MediaWriter>& pWriter) : Runner("MediaFileWrite"), _io(io), _pFile(pFile), pWriter(pWriter), _pName(pName),
	onWrite([this](const Packet& packet) {
		DUMP_REQUEST(_pName->c_str(), packet.data(), packet.size(), _pFile->path());
		_io.write(_pFile, packet);
		//Exception ex;
		//_pFile->write(ex, packet.data(), packet.size());
	}) {
}

MediaFile::Writer::Writer(const Path& path, MediaWriter* pWriter, IOFile& io) :
	Media::Stream(TYPE_FILE, path), io(io), _writeTrack(0), _running(false), _pWriter(pWriter) {
	_onError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
}
 
void MediaFile::Writer::start(const Parameters& parameters) {
	_running = true;
}

void MediaFile::Writer::setMediaParams(const Parameters& parameters) {
	_append = parameters.getBoolean<false>("append");
}

bool MediaFile::Writer::beginMedia(const string& name) {
	start(); // begin media, we can try to start here (just on beginMedia!)
	// New media, so open the file to write here => overwrite by default, otherwise append if requested!
	if (_pFile)
		return true; // already running (MBR switch)
	_pFile.reset(new File(path, _append ? File::MODE_APPEND : File::MODE_WRITE));
	io.subscribe(_pFile, _onError);
	INFO(description(), " starts");
	_pName.reset(new string(name));
	return write<Write>();
}

void MediaFile::Writer::endMedia() {
	write<EndWrite>();
	stop();
}

void MediaFile::Writer::stop() {
	_pName.reset();
	if(_pFile) {
		io.unsubscribe(_pFile);
		INFO(description(), " stops");
	}
	_running = false;
	
}



} // namespace Mona
