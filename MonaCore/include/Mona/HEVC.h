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

struct BitstreamReader;
struct HEVC : virtual Static {
	enum NAL {
		NAL_TRAIL_N = 0,
		NAL_TRAIL_R = 1,
		NAL_TSA_N = 2,
		NAL_TSA_R = 3,
		NAL_STSA_N = 4,
		NAL_STSA_R = 5,
		NAL_RADL_N = 6,
		NAL_RADL_R = 7,
		NAL_RASL_N = 8,
		NAL_RASL_R = 9,
		NAL_BLA_W_LP = 16,
		NAL_BLA_W_RADL = 17,
		NAL_BLA_N_LP = 18,
		NAL_IDR_W_RADL = 19,
		NAL_IDR_N_LP = 20,
		NAL_CRA_NUT = 21,
		NAL_IRAP_VCL23 = 23,
		NAL_VPS = 32,
		NAL_SPS = 33,
		NAL_PPS = 34,
		NAL_AUD = 35,
		NAL_EOS_NUT = 36,
		NAL_EOB_NUT = 37,
		NAL_FD_NUT = 38,
		NAL_SEI_PREFIX = 39,
		NAL_SEI_SUFFIX = 40,
	};
	static const Media::Video::Codec CODEC = Media::Video::CODEC_HEVC;

	/*!
	Get Nal type from byte */
	static UInt8				NalType(const UInt8 byte) { return (byte & 0x7f) >> 1; }
	/*!
	Get Frame type from type */
	static Media::Video::Frame	UpdateFrame(UInt8 type, Media::Video::Frame frame = Media::Video::FRAME_UNSPECIFIED);
	/*!
	Parse a config buffer into 3 packets (VPS, SPS & PPS) */
	static bool					ParseVideoConfig(const Packet& packet, Packet& vps, Packet& sps, Packet& pps);
	/*!
	Extract the content of VPS, SPS & PPS (preceded by size) into the buffer for further usage */
	static UInt32				ReadVideoConfig(const UInt8* data, UInt32 size, Buffer& buffer);
	/*!
	Write the MP4 video config packet from VPS, SPS & PPS packets */
	static BinaryWriter&		WriteVideoConfig(BinaryWriter& writer, const Packet& vps, const Packet& sps, const Packet& pps);
	/*!
	Extract video dimension from the SPS packet */
	static UInt32				SPSToVideoDimension(const UInt8* data, UInt32 size);
	/*!
	Mapping of HEVC frame type to Media::Video frame type */
	static Media::Video::Frame	Frames[];

private:
	static bool ProcessProfileTierLevel(UInt8 max_sub_layers_minus1, BitstreamReader& reader);

};


} // namespace Mona
