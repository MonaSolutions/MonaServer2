/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/MediaWriter.h"

namespace Mona {


#define SEGMENT(BASENAME, SEQUENCE, EXTENSION) BASENAME, '.', sequence, '.', EXTENSION

/*!
Tool to compute queue congestion */
struct Segments : virtual Static {

	enum {
		DURATION_TIMEOUT = 12000
	};

	static bool   Clear(Exception& ex, const Path& path, IOFile& io);
	static UInt32 NextSequence(Exception &ex, const Path& path, IOFile& io);
	static Path	  Segment(const Path& path, UInt32 sequence) { return Path(path.parent(), SEGMENT(path.baseName(), sequence, path.extension())); }
	static bool   MathSegment(const Path& path, const Path& file) { UInt32 sequence; return MathSegment(path, file, sequence); }
	static bool   MathSegment(const Path& path, const Path& file, UInt32& sequence);

	struct Writer : MediaWriter, virtual Object {
		typedef Event<void(UInt32 duration)> ON(Segment);

		Writer(const shared<MediaWriter>& pWriter, UInt8 sequences=1) : _pWriter(pWriter), sequences(sequences) {}

		UInt8 sequences;

		const char*	format() const { return _pWriter->format(); }
		virtual MIME::Type	mime() const { return _pWriter->mime(); }
		virtual const char* subMime() const { return _pWriter->subMime(); }

		void beginMedia(const OnWrite& onWrite);
		void endMedia(const OnWrite& onWrite);
		void writeProperties(const Media::Properties& properties, const OnWrite& onWrite);
		void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
		void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
		void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { _pWriter->writeData(track, type, packet, onWrite); }

	private:
		bool newSegment(const OnWrite& onWrite);

		shared<MediaWriter>						_pWriter;
		unique<Media::Data>						_pProperties;
		std::map<UInt8, Media::Audio::Config>	_audioConfigs;
		std::map<UInt8, Media::Video::Config>	_videoConfigs;
		bool									_keying;
		UInt32									_segTime;
		bool									_first;
		bool									_firstVideo;
		UInt32									_lastTime;
		
		UInt8									_sequence;
	};
};

} // namespace Mona

