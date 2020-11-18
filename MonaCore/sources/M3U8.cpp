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

#define DISCONTINUITY "\n#EXT-X-DISCONTINUITY"
#define ENDLIST		  "\n#EXT-X-ENDLIST"


Buffer& M3U8::Write(const Playlist& playlist, Buffer& buffer, bool isEvent) {
	UInt32 sequence = playlist.sequence;
	String::Append(buffer, "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:",
		ceil(playlist.maxDuration/1000.0), "\n#EXT-X-MEDIA-SEQUENCE:", sequence);
	if (isEvent)
		String::Append(buffer, "\n#EXT-X-PLAYLIST-TYPE:EVENT");
	for(UInt32 duration : playlist.durations) {
		if(duration) {
			String::Append(buffer, "\n#EXTINF:", String::Format<double>("%.3f", duration / 1000.0), "\n");
			String::Append(Segment::WriteName(playlist.baseName(), sequence++, duration, buffer), playlist.extension());
		} else
			String::Append(buffer, DISCONTINUITY);
	}
	return buffer;
}

M3U8::Writer::~Writer() {
	// add DISCONTINUITY rather ENDLIST otherwise impossible to append next time!
	if (!opened())
		return;
	static const Packet EndList(EXPAND(ENDLIST));
	FileWriter::write(EndList);
}

void M3U8::Writer::open(const Playlist& playlist) {
	_ext = playlist.extension();
	FileWriter::open(Path(playlist.parent(), playlist.baseName(), ".m3u8"));
	shared<Buffer> pBuffer(SET);
	Write(playlist, *pBuffer, true);
	if(playlist)
		String::Append(*pBuffer, DISCONTINUITY);
	FileWriter::write(Packet(pBuffer));
}

void M3U8::Writer::write(UInt32 sequence, UInt16 duration) {
	shared<Buffer> pBuffer(SET);
	String::Append(*pBuffer, "\n#EXTINF:", String::Format<double>("%.3f", duration / 1000.0), ",\n");
	String::Append(Segment::WriteName(self->baseName(), sequence, duration, *pBuffer), '.', _ext);
	FileWriter::write(Packet(pBuffer));
}





} // namespace Mona
