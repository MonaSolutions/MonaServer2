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

unique<MediaFile::Reader> MediaFile::Reader::New(Exception& ex, const char* request, Media::Source& source, const Timer& timer, IOFile& io, string&& format) {
	Path path(move(format));
	if (!(request = MediaStream::Format(ex, MediaStream::TYPE_FILE, request, path)))
		return nullptr;
	unique<MediaReader> pReader = MediaReader::New(request);
	if (!pReader)
		ex.set<Ex::Unsupported>("File stream with unsupported format ", request);
	return make_unique<MediaFile::Reader>(path, move(pReader), source, timer, io);
}

UInt32 MediaFile::Reader::Decoder::decode(shared<Buffer>& pBuffer, bool end) {
	DUMP_RESPONSE(_name.c_str(), pBuffer->data(), pBuffer->size(), _path);
	Packet packet(pBuffer); // to capture pBuffer!
	if (!_pReader || _pReader.unique())
		return 0;
	_mediaTimeGotten = false;
	_pReader->read(packet, self);
	if (end)
		_pReader.reset(); // no flush, because will be done by the main thread (end of file call stop())
	else if(!_mediaTimeGotten)
		return packet.size(); // continue to read if no flush
	_handler.queue(onFlush);
	return 0;
}

static bool ReadMediaTime(Media::Base& media, UInt32& time) {
	switch (media.type) {
		case Media::TYPE_AUDIO: {
			const Media::Audio& audio = (const Media::Audio&)media;
			if (audio.tag.isConfig)
				return false; // skip config unrendered packet because its time is sometimes unprecise
			time = audio.tag.time;
			return true;
		}
		case Media::TYPE_VIDEO: {
			const Media::Video& video = (const Media::Video&)media;
			if (video.tag.frame == Media::Video::FRAME_CONFIG && video)
				return false; // skip config unrendered packet because its time is sometimes unprecise (excepting when is the special empty packet to maintain subtitle)
			time = video.tag.time;
			return true;
		}
		default:;
	}
	return false;
}

MediaFile::Reader::Reader(const Path& path, unique<MediaReader>&& pReader, Media::Source& source, const Timer& timer, IOFile& io) :
		path(path), io(io), _pReader(move(pReader)), timer(timer), _pMedias(SET),
		MediaStream(TYPE_FILE, source, "Stream source file://...", Path(path.parent()).name(), '/', path.baseName(), '.', path.extension().empty() ? pReader->format() : path.extension().c_str()),
		_onTimer([this, &source](UInt32 delay) {
			UInt32 size = _pMedias->size();
			bool end = _pReader.unique();
			while (!_pMedias->empty()) {
				if (_pMedias->front()) {
					Media::Base& media(*_pMedias->front());
					if (media || typeid(media) != typeid(Lost)) {
						UInt32 time;
						if(ReadMediaTime(media, time) && !end){
							if (_realTime) {
								Int32 delta = range<Int32>(time - _realTime.elapsed());
								if (delta > 20) { // 20 ms for timer performance reason (to limit timer raising), not more otherwise not progressive (and player load data by wave)
									// wait delta time!
									if(_pMedias->size()<size)
										source.flush();
									return delta;
								}
								if (delta < -1000) {
									// more than 1 fps means certainly a timestamp progression error due to a CPU latency..
									WARN(description, " resets horloge (delta time of ", -delta, "ms)");
									_realTime.update(Time::Now() - time);
								}
							} else
								_realTime.update(Time::Now() - time);
						}
						source.writeMedia(media);
					} else
						source.reportLost(media.type, (Lost&)media, media.track);
				} else
					source.reset();
				_pMedias->pop_front();
			} // end of while medias
			if (_pMedias->size()<size)
				source.flush(); // flush read before because reading can take time (and to avoid too large amout of data transfer)
			// Here _pMedias is empty!
			if (end) {
				// end of file!
				stop();
				return 0;
			}
			this->io.read(_pFile); // continue to read immediatly
			return 0;
		}) {
	_onFileError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
}

bool MediaFile::Reader::starting(const Parameters& parameters) {
	if(!_pReader) {
		stop<Ex::Intern>(LOG_ERROR, "Unknown format type to read");
		return false;
	}
	_pReader->setParams(parameters);
	_realTime =	0; // reset realTime
	Decoder* pDecoder = new Decoder(io.handler, _pReader, path, source.name(), _pMedias);
	pDecoder->onFlush = _onFlush = [this]() { timer.set(_onTimer, _onTimer()); };
	_pFile.set(path, File::MODE_READ);
	io.subscribe(_pFile, pDecoder, nullptr, _onFileError);
	io.read(_pFile);
	return run();
}

void MediaFile::Reader::stopping() {
	timer.set(_onTimer, 0);
	io.unsubscribe(_pFile);
	_onFlush = nullptr;
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if (_pReader.unique()) { // else always used by the decoder, impossible to flush!
		_onTimer(); // flush possible current media remaining
		_pReader->flush(source); // reset + flush!
	} else
		source.reset(); // because _pReader could be used always by the decoder (parallel!)
	// new resource for next start
	_pMedias.set();
	_pReader = MediaReader::New(_pReader->subMime());
}



unique<MediaFile::Writer> MediaFile::Writer::New(Exception& ex, const char* request, IOFile& io, string&& format) {
	bool isM3U8 = String::ICompare(format, EXPAND("x-mpegURL")) == 0;
	if (isM3U8) // just valid for MediaFile::Writer => M3U8 + TSWriter!
		format = "ts";
	Path path(move(format));
	if (!(request = MediaStream::Format(ex, MediaStream::TYPE_FILE, request, path)))
		return nullptr;
	unique<MediaWriter> pWriter = MediaWriter::New(request);
	if (!pWriter) {
		ex.set<Ex::Unsupported>("File stream with unsupported format ", request);
		return nullptr;
	}
	if (isM3U8)
		path.set(path.parent(), path.baseName(), ".m3u8");
	return make_unique<MediaFile::Writer>(path, move(pWriter), io);
}

MediaFile::Writer::File::File(const string& name, const Path& path, const shared<MediaWriter>& pWriter, const shared<Playlist::Writer>& pPlaylist, UInt8 sequences, IOFile& io) : Path(path), name(name), sequence(0), FileWriter(io), pPlaylist(pPlaylist),
	onWrite([this, address = String(path.parent(), path.name())](const Packet& packet) {
		DUMP_REQUEST(this->name.c_str(), packet.data(), packet.size(), address);
		write(packet);
		// Exception ex; _pFile->write(ex, packet.data(), packet.size()); // Just usefull to test!
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

MediaFile::Writer::Writer(const Path& path, unique<MediaWriter>&& pWriter, IOFile& io) : 
	path(path), io(io), _writeTrack(0), _pWriter(move(pWriter)), _sequences(1), _append(false),
	MediaStream(TYPE_FILE, "Stream target file://...", Path(path.parent()).name(), '/', path.baseName(), '.', path.extension().empty() ? pWriter->format() : path.extension().c_str()) {
	if (String::ICompare(path.extension(), "m3u8") == 0)
		_pPlaylist.set<M3U8::Writer>(io);
	if(_pPlaylist)
		_pPlaylist->onError = [this](const Exception& ex) { WARN(description, ", ", ex); };
}

bool MediaFile::Writer::starting(const Parameters& parameters) {
	if (!_pWriter) {
		stop<Ex::Intern>(LOG_ERROR, "Unknown format type to write");
		return false;
	}
	// pulse starting, nothing todo (wait beginMedia to create _pFile)
	parameters.getBoolean("append", _append);
	if (parameters.getNumber<UInt8>("sequences", _sequences)) {
		if (!_pPlaylist)
			WARN(description, ", sequences usefull just for segments")
		else if (_pFile)
			write<ChangeSequences>(_sequences);
	}
	return true;
}

bool MediaFile::Writer::beginMedia(const string& name) {
	if (!start()) // begin media, we can try to start here (just on beginMedia!)
		return false;
	// New media, so open the file to write here => overwrite by default, otherwise append if requested!
	if (_pFile)
		return true; // already running (MBR switch)
	if (!run())
		return false;
	_pFile.set(name, path, _pWriter, _pPlaylist, _sequences, io).onError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
	write<Begin>(_append);
	return true;
}

bool MediaFile::Writer::writeProperties(const Media::Properties& properties) {
	Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
	const Packet& packet(properties.data(type));
	return write<MediaWrite<Media::Data>>(type, packet, 0, true);
}

bool MediaFile::Writer::endMedia() {
	stop();
	return false;
}

void MediaFile::Writer::stopping() {
	if (!_pFile)
		return;
	write<EndWrite>(); // _pWriter->endMedia()!
	_pFile.reset();
}



} // namespace Mona
