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
#include "Mona/MediaWriter.h"

namespace Mona {

template <typename VideoType>
struct NALNetWriter : MediaTrackWriter, virtual Object {
	// https://tools.ietf.org/html/rfc6184
	// http://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/

	NALNetWriter();

	void beginMedia() { _nal = NAL_START; _time = 0; }
	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize);
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize);

private:
	UInt32  _time;
	enum Nal {
		NAL_UNDEFINED=0,
		NAL_VCL,
		NAL_CONFIG,
		NAL_START
	} _nal;

	static const Packet	_Unit;
};



} // namespace Mona
