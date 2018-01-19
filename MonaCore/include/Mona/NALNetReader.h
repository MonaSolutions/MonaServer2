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

/*!
Transform a 00 00 01 NAL suffixed format to a NAL format (faster to read) 
start code prefix => https://en.wikipedia.org/wiki/Network_Abstraction_Layer#NAL_Units_in_Byte-Stream_Format_Use
*/
template <class VideoType>
struct NALNetReader : virtual Object, MediaTrackReader {
	// AVC NAL types => http://gentlelogic.blogspot.fr/2011/11/exploring-h264-part-2-h264-bitstream.html
	// HEVC NAL types => https://github.com/virinext/hevcesbrowser/blob/master/hevcparser/include/Hevc.h#L13-L41

	NALNetReader(UInt8 track = 1);

private:

	UInt32	parse(Packet& buffer, Media::Source& source);
	void	onFlush(Packet& buffer, Media::Source& source);

	void    writeNal(const UInt8* data, UInt32 size, Media::Source& source, bool eon=false);
	void	flushNal(Media::Source& source);

	UInt8					_type;
	Media::Video::Tag		_tag;
	UInt8					_state;
	UInt32					_position;
	shared<Buffer>			_pNal;
};

} // namespace Mona
