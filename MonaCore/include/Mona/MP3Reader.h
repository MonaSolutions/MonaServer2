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

#pragma once

#include "Mona/Mona.h"
#include "Mona/MediaReader.h"

namespace Mona {


struct MP3Reader : virtual Object, MediaTrackReader {
	// http://cutebugs.net/files/mpeg-drafts/11172-3.pdf
	// http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
	MP3Reader(UInt8 track=1) : _syncError(false), _size(0), MediaTrackReader(track), _tag(Media::Audio::CODEC_MP3) {}

private:
	UInt32 parse(Packet& buffer, Media::Source& source);
	void   onFlush(Packet& buffer, Media::Source& source);

	UInt16 _size;
	bool   _syncError;
	Media::Audio::Tag _tag;
};


} // namespace Mona
