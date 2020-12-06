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
	Path path(move(format));
	if (!(request = MediaStream::Format(ex, MediaStream::TYPE_FILE, request, path)))
		return nullptr;
	bool isM3U8 = String::ICompare(request, EXPAND("x-mpegURL")) == 0;
	if (isM3U8) // just valid for MediaFile::Writer => M3U8 + TSWriter!
		request = "ts";
	unique<MediaWriter> pWriter = MediaWriter::New(request);
	if (!pWriter) {
		ex.set<Ex::Unsupported>("File stream with unsupported format ", request);
		return nullptr;
	}
	if (isM3U8)
		path.set(path.parent(), path.baseName(), ".m3u8");
	return make_unique<MediaFile::Writer>(path, move(pWriter), io);
}

MediaFile::Writer::File::File(const string& name, const Path& path, const shared<MediaWriter>& pWriter, const shared<Playlist::Writer>& pPlaylistWriter, UInt32 segments, UInt16 duration, IOFile& io) :
	Playlist(path), name(name), segments(segments), FileWriter(io), _pPlaylistWriter(pPlaylistWriter),
	onWrite([this, address = String(path.parent(), path.name())](const Packet& packet) {
		DUMP_REQUEST(this->name.c_str(), packet.data(), packet.size(), address);
		write(packet);
		// Exception ex; _pFile->write(ex, packet.data(), packet.size()); // Just usefull to test!
	}) {
	if (_pPlaylistWriter) {
		setExtension(pWriter->format());// change path to match pWriter
		_pWriter.set<Segments::Writer>(*pWriter, duration).onSegment =
			// hold pWriter in this event, deleta the event in Writer::File::~File
			[this, pWriter = shared<MediaWriter>(pWriter)](UInt16 duration) {
				// close and rename segment with its definitive name
				close(Segment::BuildPath(self, sequence + count(), duration));
				// add item to the playlist
				_pPlaylistWriter->write(self, duration);
				// remove front surplus segments
				if(this->segments)
					Segments::Clear(self, this->segments, this->io);
				// open new file segment
				open();
			};
	} else
		_pWriter = move(pWriter);
}
MediaFile::Writer::File::~File() {
	if (!_pPlaylistWriter)
		return;
	// delete onSegment event to release shared<MediaWriter>!
	shared<Segments::Writer> pWriter = static_pointer_cast<Segments::Writer>(_pWriter);
	pWriter->onSegment = nullptr;
}

void MediaFile::Writer::Begin::process(Exception& ex, File& file) {
	if(file.segmented())
		Segments::Init(ex, file.io, file, append);
	file.open(append);
	file->beginMedia(file.onWrite);
}

MediaFile::Writer::File& MediaFile::Writer::File::open(bool append) {
	if (_pPlaylistWriter) {
		// write in temporary duration=0 file
		FileWriter::open(Segment::BuildPath(self, sequence + count(), 0));
	} else
		FileWriter::open(self, append);
	return self;
}

MediaFile::Writer::Writer(const Path& path, unique<MediaWriter>&& pWriter, IOFile& io) : 
	path(path), io(io), _writeTrack(0), _pWriter(move(pWriter)), _duration(0), _segments(0), _append(false),
	MediaStream(TYPE_FILE, "Stream target file://...", Path(path.parent()).name(), '/', path.baseName(), '.', path.extension().empty() ? pWriter->format() : path.extension().c_str()) {
	if (String::ICompare(path.extension(), "m3u8") == 0)
		_pPlaylistWriter.set<M3U8::Writer>(io).onError = [this](const Exception& ex) { WARN(description, ", ", ex); };
}

bool MediaFile::Writer::starting(const Parameters& parameters) {
	if (!_pWriter) {
		stop<Ex::Intern>(LOG_ERROR, "Unknown format type to write");
		return false;
	}
	// pulse starting, nothing todo (wait beginMedia to create _pFile)
	parameters.getBoolean("append", _append);
	if (parameters.getNumber("duration", _duration)) {
		if (!_pPlaylistWriter)
			WARN(description, ", duration is available just in segments mode")
		else if (_pFile)
			write<ChangeDuration>(_duration);
	}
	if (parameters.getNumber("segments", _segments)) {
		if (!_pPlaylistWriter)
			WARN(description, ", segments usefull just in segments mode")
		else if (_pFile)
			write<ChangeSegments>(_segments);
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
	_pFile.set(name, path, _pWriter, _pPlaylistWriter, _segments, _duration, io).onError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
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
	write<End>(); // _pWriter->endMedia()!
	_pFile.reset();
}



} // namespace Mona
