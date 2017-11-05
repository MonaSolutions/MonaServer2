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
Transform a 00 00 01 NAL format to a AVC NAL format (more speed to read) */
struct H264NALReader : virtual Object, MediaTrackReader {
	// http://apple-http-osmf.googlecode.com/svn/trunk/src/at/matthew/httpstreaming/HTTPStreamingMP2PESVideo.as
	// NAL types => http://gentlelogic.blogspot.fr/2011/11/exploring-h264-part-2-h264-bitstream.html

	H264NALReader(UInt8 track=1) : MediaTrackReader(track), _tag(Media::Video::CODEC_H264), _state(0), _type(0xFF) {}

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
