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

#include "Mona/M3U8.h"


using namespace std;

namespace Mona {

#define HEADER		  "#EXTM3U"
#define DISCONTINUITY "\n#EXT-X-DISCONTINUITY"
#define ENDLIST		  "\n#EXT-X-ENDLIST\n" // \n at end of file (ffmpeg makes it)

#define WRITE_EXTINF(BUFFER, DURATION) String::Append(BUFFER, "\n#EXTINF:", String::Format<double>("%.3f", DURATION / 1000.0), ",\n")

Buffer& M3U8::Write(const Playlist::Master& playlist, Buffer& buffer) {
	String::Append(buffer, HEADER);
	for (const auto& it : playlist.items) {
		String::Append(buffer, "\n#EXT-X-STREAM-INF:BANDWIDTH=", it.second*8); // bit/s
		String::Append(buffer, "\n", playlist.parent(), playlist.name(), '/', it.first, ".m3u8");
	}
	return buffer;
}

Buffer& M3U8::Write(const Playlist& playlist, Buffer& buffer, const char* type) {
	UInt32 sequence = playlist.sequence;
	//  Round maxDuration, HLS tolerate a segment duration superior of 0.5 to target-duration
	// "Media Segments MUST NOT exceed the target duration by more than 0.5 seconds"
	String::Append(buffer, HEADER, "\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:",
		(playlist.maxDuration+500) / 1000, "\n#EXT-X-MEDIA-SEQUENCE:", sequence); 
	if (type)
		String::Append(buffer, "\n#EXT-X-PLAYLIST-TYPE:", type);
	else // else is a live playlist (not VOD or EVENT)
		String::Append(buffer, "\n#EXT-X-ALLOW-CACHE:NO");
	UInt32 i = 0;
	for (UInt32 duration : playlist.durations()) {
		++i;
		if (duration) {
			WRITE_EXTINF(buffer, duration);
			String::Append(Segment::WriteName(playlist.baseName(), sequence++, duration, buffer), '.', playlist.extension());
		} else if(i==playlist.count())
			String::Append(buffer, i < playlist.count() ? DISCONTINUITY : ENDLIST);
	}
	return buffer;
}


M3U8::Writer::~Writer() {
	if (!opened())
		return;
	// add ENDLIST to be playable by some player as VLC
	static const Packet EndList(EXPAND(ENDLIST));
	FileWriter::write(EndList);
}

void M3U8::Writer::open(const Playlist& playlist) {
	FileWriter::open(Path(playlist.parent(), playlist.baseName(), ".m3u8"));
	shared<Buffer> pBuffer(SET);
	Write(playlist, *pBuffer, "EVENT");
	if(playlist)
		String::Append(*pBuffer, DISCONTINUITY);
	FileWriter::write(Packet(pBuffer));
}

void M3U8::Writer::write(const string& format, UInt32 sequence, UInt16 duration) {
	shared<Buffer> pBuffer(SET);
	WRITE_EXTINF(*pBuffer, duration);
	String::Append(Segment::WriteName(self->baseName(), sequence, duration, *pBuffer), '.', format);
	FileWriter::write(Packet(pBuffer));
}





} // namespace Mona
