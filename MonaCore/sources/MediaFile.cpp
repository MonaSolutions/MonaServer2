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

UInt32 MediaFile::Reader::Decoder::decode(shared<Buffer>& pBuffer, bool end) {
	DUMP_RESPONSE(_name.c_str(), pBuffer->data(), pBuffer->size(), _path);
	Packet packet(pBuffer); // to capture pBuffer!
	if (!_pReader || _pReader.unique())
		return 0;
	_flushed = false;
	_pReader->read(packet, *this);
	if (!end)
		return _flushed ? 0 : packet.size(); // continue to read if no flush
	_pReader->flush(*this);
	_pReader.reset();
	_handler.queue(onFlush);
	return 0;
}

MediaFile::Reader::Reader(const Path& path, MediaReader* pReader, const Timer& timer, IOFile& io) :
	Media::Stream(TYPE_FILE), io(io), path(path), _pReader(pReader), timer(timer),
		_onTimer([this](UInt32 delay) {
			shared_ptr<Media::Base> pMedia;
			while (!_medias.empty()) {
				pMedia = move(_medias.front());
				_medias.pop_front();
				if (pMedia) {
					if (*pMedia || typeid(pMedia) != typeid(Lost)) {
						_pSource->writeMedia(*pMedia);
						UInt32 time;
						switch (pMedia->type) {
							default: continue;
							case Media::TYPE_AUDIO:
								time = ((const Media::Audio&)*pMedia).tag.time;
								break;
							case Media::TYPE_VIDEO:
								time = ((const Media::Video&)*pMedia).tag.time;
								break;
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
						if (delta < 20) // 20 ms to get at less one frame in advance (20ms = 50fps), not more otherwise not progressive (and player loading wave)
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

void MediaFile::Reader::start() {
	if (!_pSource) {
		Stream::stop<Ex::Intern>(LOG_ERROR, "call start(Media::Source& source) in first");
		return;
	}
	if (_pFile)
		return;
	_realTime =	0; // reset realTime
	shared<Decoder> pDecoder(new Decoder(io.handler, _pReader, path, _pSource->name()));
	pDecoder->onFlush = _onFlush = [this]() { _onTimer(); };
	pDecoder->onReset = _onReset = [this]() { _medias.emplace_back(); };
	pDecoder->onLost = _onLost = [this](shared<Lost>& pLost) { _medias.emplace_back(pLost); };
	pDecoder->onMedia = _onMedia = [this](shared<Media::Base>& pMedia) { _medias.emplace_back(pMedia); };
	io.open(_pFile, path, pDecoder, nullptr, _onFileError);
	io.read(_pFile);
	INFO(description(), " starts");
}

void MediaFile::Reader::stop() {
	if (!_pFile)
		return;
	timer.set(_onTimer, 0);
	io.close(_pFile);
	_onFlush = nullptr;
	_onReset = nullptr;
	_onLost = nullptr;
	_onMedia = nullptr;
	_medias.clear();
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if (_pReader.unique())
		_pReader->flush(*_pSource);
	else
		_pSource->reset(); // because the source.reset of _pReader->flush() can't be called (parallel!)
	_pReader.reset(MediaReader::New(_pReader->format()));
	INFO(description(), " stops");
}


MediaFile::Writer::Write::Write(const shared<string>& pName, IOFile& io, const shared<File>& pFile, const shared<MediaWriter>& pWriter) : Runner("MediaFileWrite"), _io(io), _pFile(pFile), pWriter(pWriter), _pName(pName),
	onWrite([this](const Packet& packet) {
		DUMP_REQUEST(_pName->c_str(), packet.data(), packet.size(), _pFile->path());
		_io.write(_pFile, packet);
	}) {
}

MediaFile::Writer::Writer(const Path& path, MediaWriter* pWriter, IOFile& io) :
	Media::Stream(TYPE_FILE), io(io), _writeTrack(0), _running(false), path(path), _pWriter(pWriter) {
	_onError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
}
 
void MediaFile::Writer::start() {
	_running = true;
}

void MediaFile::Writer::setMediaParams(const Parameters& parameters) {
	_append = parameters.getBoolean<false>("append");
}

bool MediaFile::Writer::beginMedia(const string& name) {
	if (!_running)
		return false; // Not started => no Log, just ejects
	// New media, so open the file to write here => overwrite by default, otherwise append if requested!
	io.open(_pFile, path, _onError, _append);
	INFO(description(), " starts");
	_pName.reset(new string(name));
	return write<Write>();
}

void MediaFile::Writer::endMedia(const string& name) {
	write<EndWrite>();
	stop();
	start();
}

void MediaFile::Writer::stop() {
	_pName.reset();
	if(_pFile) {
		io.close(_pFile);
		INFO(description(), " stops");
	}
	_running = false;
	
}



} // namespace Mona
