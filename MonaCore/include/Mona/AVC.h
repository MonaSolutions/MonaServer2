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
#include "Mona/MPEG4.h"
#include "Mona/Media.h"
#include "Mona/MediaWriter.h"

namespace Mona {

struct AVC : virtual Static {
	enum NAL {
		NAL_UNDEFINED = 0,
		NAL_SLICE_NIDR = 1,
		NAL_SLICE_A = 2,
		NAL_SLICE_B = 3,
		NAL_SLICE_C = 4,
		NAL_SLICE_IDR = 5,
		NAL_SEI = 6,
		NAL_SPS = 7,
		NAL_PPS = 8,
		NAL_AUD = 9,
		NAL_END_SEQ = 10,
		NAL_END_STREAM = 11,
		NAL_FILLER = 12,
	};
	static const Media::Video::Codec CODEC = Media::Video::CODEC_H264;
	/*!
	Get Nal type from byte */
	static UInt8				NalType(const UInt8 byte) { return byte & 0x1F; }
	/*!
	Get Frame type from Nal type */
	static Media::Video::Frame	UpdateFrame(UInt8 type, Media::Video::Frame frame = Media::Video::FRAME_UNSPECIFIED);
	/*!
	Parse a config buffer into 3 packets (VPS, SPS & PPS) */
	static bool					ParseVideoConfig(const Packet& packet, Packet& sps, Packet& pps);
	/*!
	Extract the content of VPS, SPS & PPS (preceded by size) into the buffer for further usage */
	static UInt32				ReadVideoConfig(const UInt8* data, UInt32 size, Buffer& buffer);
	/*!
	Write the MP4 video config packet from VPS, SPS & PPS packets */
	static BinaryWriter&		WriteVideoConfig(BinaryWriter& writer, const Packet& sps, const Packet& pps);
	/*!
	Extract video dimension from the SPS packet */
	static UInt32				SPSToVideoDimension(const UInt8* data, UInt32 size);
	/*!
	Mapping of AVC frame type to Media::Video frame type */
	static Media::Video::Frame	Frames[];
};


} // namespace Mona
