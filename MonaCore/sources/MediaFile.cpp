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
#include "Mona/TSWriter.h"
#include "Mona/M3U8.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

unique<MediaFile::Reader> MediaFile::Reader::New(const Path& path, Media::Source& source, const char* subMime, const Timer& timer, IOFile& io) {
	unique<MediaReader> pReader(MediaReader::New(subMime));
	return pReader ? make_unique<MediaFile::Reader>(path, source, move(pReader), timer, io) : nullptr;
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

MediaFile::Reader::Reader(const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const Timer& timer, IOFile& io) :
	MediaStream(TYPE_FILE, path, source), io(io), _pReader(move(pReader)), timer(timer), _pMedias(SET),
		_onTimer([this, &source](UInt32 delay) {
			unique<Media::Base> pMedia;
			while (!_pMedias->empty()) {
				pMedia = move(_pMedias->front());
				_pMedias->pop_front();
				if (pMedia) {
					if (*pMedia || typeid(*pMedia) != typeid(Lost)) {
						source.writeMedia(*pMedia);
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
						source.flush();
						this->timer.set(_onTimer, delta);
						return 0; // pause!
					}
					source.reportLost(pMedia->type, (Lost&)(*pMedia), pMedia->track);
				} else if(!_pReader.unique()) // else reset will be done on stop!
					source.reset();
			}
			if (_pReader.unique()) {
				// end of file!
				stop();
				return 0;
			}
			if(pMedia)
				source.flush(); // flush before because reading can take time (and to avoid too large amout of data transfer)
			this->io.read(_pFile); // continue to read immediatly
			return 0;
		}) {
	_onFileError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
}

bool MediaFile::Reader::starting(const Parameters& parameters) {
	_realTime =	0; // reset realTime
	Decoder* pDecoder = new Decoder(io.handler, _pReader, path, source.name(), _pMedias);
	pDecoder->onFlush = _onFlush = [this]() { _onTimer(); };
	_pFile.set(path, File::MODE_READ);
	io.subscribe(_pFile, pDecoder, nullptr, _onFileError);
	io.read(_pFile);
	return true;
}

void MediaFile::Reader::stopping() {
	timer.set(_onTimer, 0);
	io.unsubscribe(_pFile);
	_onFlush = nullptr;
	_pMedias.set();
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if (_pReader.unique())
		_pReader->flush(source);
	else
		source.reset(); // because the source.reset of _pReader->flush() can't be called (parallel!)
	_pReader = MediaReader::New(_pReader->subMime());
}





unique<MediaFile::Writer> MediaFile::Writer::New(const Path& path, const char* subMime, IOFile& io) {
	if (String::ICompare(subMime, EXPAND("x-mpegURL")) == 0) // just valid for MediaFile::Writer => M3U8 + TSWriter!
		return make_unique<MediaFile::Writer>(Path(path.parent(), path.baseName(), ".m3u8"), make_unique<TSWriter>(), io);
	unique<MediaWriter> pWriter(MediaWriter::New(subMime));
	return pWriter ? make_unique<MediaFile::Writer>(path, move(pWriter), io) : nullptr;
}

MediaFile::Writer::File::File(const string& name, const Path& path, const shared<MediaWriter>& pWriter, const shared<Playlist::Writer>& pPlaylist, UInt8 sequences, IOFile& io) : Path(path), name(name), sequence(0), FileWriter(io), pPlaylist(pPlaylist),
	onWrite([this, address = String(path.parent(), path.name())](const Packet& packet) {
		DUMP_REQUEST(this->name.c_str(), packet.data(), packet.size(), address);
		write(packet);
		//Exception ex;
		//_pFile->write(ex, packet.data(), packet.size());
	}) {
	if (pPlaylist) {
		setExtension(pWriter->format());// change path to match pWriter
		_pWriter.set<Segments::Writer>(move(pWriter), sequences).onSegment = [this](UInt32 duration) { // new segment
			this->pPlaylist->write(sequence, duration); /// write M3U8
			open(Segments::Segment(self, ++sequence)); /// open new file segment
		};
	} else
		_pWriter = move(pWriter);
}

void MediaFile::Writer::Begin::process(Exception& ex, File& file) {
	if (file.pPlaylist) {
		file.pPlaylist->open(file, append); // open or overload playlist
		if (append)
			file.sequence = Segments::NextSequence(ex, file, file.io); // search next sequence (display a WARN if error)
		else
			Segments::Clear(ex, file, file.io); // remove all previous segments (display a WARN if error)
		file.open(Segments::Segment(file, file.sequence), append);
	} else
		file.open(file, append);
	file->beginMedia(file.onWrite);
}

MediaFile::Writer::Writer(const Path& path, unique<MediaWriter>&& pWriter, IOFile& io) : _sequences(1), _append(false),
	MediaStream(TYPE_FILE, path), io(io), _writeTrack(0), _pWriter(move(pWriter)) {
	if (String::ICompare(path.extension(), "m3u8") == 0)
		_pPlaylist.set<M3U8::Writer>(io);
	if(_pPlaylist)
		_pPlaylist->onError = [this](const Exception& ex) { WARN(description(), ", ", ex); };
}

bool MediaFile::Writer::starting(const Parameters& parameters) {
	// pulse starting, nothing todo (wait beginMedia to create _pFile)
	parameters.getBoolean("append", _append);
	if (parameters.getNumber<UInt8>("sequences", _sequences)) {
		if (!_pPlaylist)
			WARN(description(), ", sequences usefull just for segments")
		else if (_pFile)
			write<ChangeSequences>(_sequences);
	}
	return false;
}

bool MediaFile::Writer::beginMedia(const string& name) {
	// New media, so open the file to write here => overwrite by default, otherwise append if requested!
	if (_pFile)
		return true; // already running (MBR switch)
	start(); // begin media, we can try to start here (just on beginMedia!)
	if (!finalizeStart())
		return false;
	_pFile.set(name, path, _pWriter, _pPlaylist, _sequences, io).onError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
	write<Begin>(_append);
	return true;
}

bool MediaFile::Writer::writeProperties(const Media::Properties& properties) {
	Media::Data::Type type;
	const Packet& packet(properties(type));
	return write<MediaWrite<Media::Data>>(type, packet, 0, true);
}

void MediaFile::Writer::endMedia() {
	stop();
}

void MediaFile::Writer::stopping() {
	if (!_pFile)
		return;
	write<EndWrite>(); // _pWriter->endMedia()!
	_pFile.reset();
}



} // namespace Mona
