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
#include "Mona/Media.h"
#include "Mona/MediaWriter.h"

namespace Mona {

struct BitstreamReader;
struct HEVC : virtual Static {
	static Media::Video::Frame UpdateFrame(UInt8 type, Media::Video::Frame frame = Media::Video::FRAME_UNSPECIFIED);

	/// \brief Parse a config buffer into 3 packets (VPS, SPS & PPS)
	static bool				ParseVideoConfig(const Packet& packet, Packet& vps, Packet& sps, Packet& pps);

	/// \brief Extract the content of VPS, SPS & PPS (preceded by size) into the buffer for further usage
	static UInt32			ReadVideoConfig(const UInt8* data, UInt32 size, Buffer& buffer);

	/// \brief Write the MP4 video config packet from VPS, SPS & PPS packets
	static BinaryWriter&	WriteVideoConfig(BinaryWriter& writer, const Packet& vps, const Packet& sps, const Packet& pps);

	/// \brief Extract video dimension from the SPS packet
	static UInt32			SPSToVideoDimension(const UInt8* data, UInt32 size);

	/// \brief Mapping of HEVC frame type to Media::Video frame type
	static Media::Video::Frame Frames[];
private:
	static bool ProcessProfileTierLevel(UInt8 max_sub_layers_minus1, BitstreamReader& reader);
	static UInt16 ReadExpGolomb(BitstreamReader& reader);

};


} // namespace Mona
