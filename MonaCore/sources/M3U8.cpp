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
#include "Mona/Segments.h"


using namespace std;

namespace Mona {

M3U8::Writer& M3U8::Writer::open(const Path& path, bool append) {
	_ext = path.extension();
	FileWriter::open(Path(path.parent(), path.baseName(), ".m3u8"), append);
	if (append) {
		static const shared<const Buffer> PDiscontinuity(SET, EXPAND("\n#EXT-X-DISCONTINUITY"));
		FileWriter::write(Packet(PDiscontinuity));
	} else {
		struct Header : Buffer, virtual Object {
			Header() : Buffer(EXPAND("#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-PLAYLIST-TYPE:EVENT\n#EXT-X-MEDIA-SEQUENCE:0\n#EXT-X-TARGETDURATION:")) {
				String::Append(self, Segments::DURATION_TIMEOUT / 1000);
			}
		};
		static const shared<const Header> PHeader(SET);
		FileWriter::write(Packet(PHeader));
	}
	return self;
}

void M3U8::Writer::write(UInt32 sequence, UInt32 duration) {
	shared<Buffer> pBuffer(SET);
	String::Append(*pBuffer, "\n#EXTINF:", String::Format<double>("%.3f", duration / 1000.0), ",\n", SEGMENT(self->baseName(), sequence, _ext));
	FileWriter::write(Packet(pBuffer));
}

} // namespace Mona
