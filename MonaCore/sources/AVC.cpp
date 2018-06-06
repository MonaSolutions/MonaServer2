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

#include "Mona/AVC.h"

using namespace std;

namespace Mona {

Media::Video::Frame AVC::Frames[] = {
	Media::Video::FRAME_UNSPECIFIED, 
	Media::Video::FRAME_INTER, 
	Media::Video::FRAME_INTER, 
	Media::Video::FRAME_INTER, 
	Media::Video::FRAME_INTER, 
	Media::Video::FRAME_KEY, 
	Media::Video::FRAME_INFO, 
	Media::Video::FRAME_CONFIG, // SPS
	Media::Video::FRAME_CONFIG // PPS
};

Media::Video::Frame AVC::UpdateFrame(UInt8 type, Media::Video::Frame frame) {
	if (frame == Media::Video::FRAME_KEY || frame == Media::Video::FRAME_CONFIG)
		return frame;
	
	type = type > 8 ? Media::Video::FRAME_UNSPECIFIED : Frames[type];
	if (!frame)
		return Media::Video::Frame(type); // change
	switch (type) {
		case Media::Video::FRAME_UNSPECIFIED:
			return frame; // unchange
		case Media::Video::FRAME_KEY:
		case Media::Video::FRAME_CONFIG:
		case Media::Video::FRAME_INTER:
			return Media::Video::Frame(type); // change
		default:; // inter, disposable, info (old frame) => disposable, info (new frame)
	}
	return (frame == Media::Video::FRAME_INTER || frame == Media::Video::FRAME_DISPOSABLE_INTER) ? frame : Media::Video::Frame(type);
}

bool AVC::ParseVideoConfig(const Packet& packet, Packet& sps, Packet& pps) {
	BinaryReader reader(packet.data(), packet.size());
	UInt32 length;
	while (reader.available()) {
		length = reader.read32();
		if (length > reader.available())
			length = reader.available();
		if (!length)
			continue;
		UInt8 id(*reader.current() & 0x1F);
		if (id == 7)
			sps.set(packet, reader.current(), length);
		else if (id == 8)
			pps.set(packet, reader.current(), length);
		reader.next(length);
	}
	if (sps)
		return true; // pps not mandatory
	WARN("H264 configuration malformed");
	return false;
}


UInt32 AVC::ReadVideoConfig(const UInt8* data, UInt32 size, Buffer& buffer) {
	BinaryReader reader(data, size);

	reader.next(5); // skip avcC version 1 + 3 bytes of profile, compatibility, level + 1 byte xFF

					// SPS and PPS
	BinaryWriter writer(buffer);
	UInt8 count(reader.read8() & 0x1F);
	bool isPPS(false);
	while (reader.available() >= 2 && count--) {
		size = reader.read16();
		if (size > reader.available())
			size = reader.available();
		writer.write32(size).write(reader.current(), size);
		reader.next(size);
		if (!count) {
			if (isPPS)
				break;
			count = reader.read8(); // PPS now!
			isPPS = true;
		}
	}
	return reader.position();
}

BinaryWriter& AVC::WriteVideoConfig(BinaryWriter& writer, const Packet& sps, const Packet& pps) {
	// SPS + PPS
	writer.write8(0x01); // avcC version 1
	writer.write(sps.data() + 1, 3); // profile, compatibility, level

	writer.write8(0xff); // 111111 + 2 bit NAL size - 1
							// sps
	writer.write8(0xe1); // 11 + number of SPS
	writer.write16(sps.size());
	writer.write(sps);

	// pps
	writer.write8(0x01); // number of PPS
	writer.write16(pps.size());
	writer.write(pps);
	return writer;
}


UInt32 AVC::SPSToVideoDimension(const UInt8* data, UInt32 size) {
	if ((*data++ & 0x1f) != 7) {
		ERROR("Invalid SPS data");
		return 0;
	}

	BitReader reader(data, size - 1);

	UInt16 leftOffset = 0, rightOffset = 0, topOffset = 0, bottomOffset = 0;
	UInt16 subWidthC = 0, subHeightC = 0;

	UInt8 idc = reader.read<UInt8>();
	reader.next(16); // constraincts
	MPEG4::ReadExpGolomb(reader); // seq_parameter_set_id

	switch (idc) {
	case 100:
	case 110:
	case 122:
	case 144:
	case 44:
	case 83:
	case 86:
	case 118:
		switch (MPEG4::ReadExpGolomb(reader)) { // chroma_format_idc
		case 1: // 4:2:0
			subWidthC = subHeightC = 2;
			break;
		case 2: // 4:2:2
			subWidthC = 2;
			subHeightC = 1;
			break;
		case 3: // 4:4:4
			if (!reader.read())
				subWidthC = subHeightC = 1; // separate_colour_plane_flag 
			break;
		}

		MPEG4::ReadExpGolomb(reader); // bit_depth_luma_minus8
		MPEG4::ReadExpGolomb(reader); // bit_depth_chroma_minus8
		reader.next(); // qpprime_y_zero_transform_bypass_flag
		if (reader.read()) { // seq_scaling_matrix_present_flag
			for (UInt8 i = 0; i < 8; ++i) {
				if (reader.read()) { // seq_scaling_list_present_flag
					UInt8 sizeOfScalingList = (i < 6) ? 16 : 64;
					UInt8 scale = 8;
					for (UInt8 j = 0; j < sizeOfScalingList; ++j) {
						Int16 delta = MPEG4::ReadExpGolomb(reader);
						if (delta & 1)
							delta = (delta + 1) / 2;
						else
							delta = -(delta / 2);
						scale = (scale + delta + 256) % 256;
						if (!scale)
							break;
					}
				}
			}
		}
		break;
	}

	MPEG4::ReadExpGolomb(reader); // log2_max_frame_num_minus4
	UInt16 picOrderCntType = MPEG4::ReadExpGolomb(reader);
	if (!picOrderCntType) {
		MPEG4::ReadExpGolomb(reader); // log2_max_pic_order_cnt_lsb_minus4
	}
	else if (picOrderCntType == 1) {
		reader.next(); // delta_pic_order_always_zero_flag
		MPEG4::ReadExpGolomb(reader); // offset_for_non_ref_pic
		MPEG4::ReadExpGolomb(reader); // offset_for_top_to_bottom_field
		UInt16 refFrames = MPEG4::ReadExpGolomb(reader);
		for (UInt16 i = 0; i < refFrames; ++i)
			MPEG4::ReadExpGolomb(reader); // sps->offset_for_ref_frame[ i ] = ReadSE();
	}
	MPEG4::ReadExpGolomb(reader); // max_num_ref_frames
	reader.next(); // gaps_in_frame_num_value_allowed_flag
	UInt16 picWidth = (MPEG4::ReadExpGolomb(reader) + 1) * 16;
	UInt16 picHeight = (MPEG4::ReadExpGolomb(reader) + 1) * 16;
	if (!reader.read()) { // frame_mbs_only_flag
		picHeight *= 2;
		subHeightC *= 2;
		reader.next(); // mb_adaptive_frame_field_flag
	}

	reader.next(); // direct_8x8_inference_flag
	if (reader.read()) { // frame_cropping_flag
		leftOffset = MPEG4::ReadExpGolomb(reader);
		rightOffset = MPEG4::ReadExpGolomb(reader);
		topOffset = MPEG4::ReadExpGolomb(reader);
		bottomOffset = MPEG4::ReadExpGolomb(reader);
	}

	// return width << 16 | height;
	return ((picWidth - subWidthC * (leftOffset + rightOffset)) << 16) | (picHeight - subHeightC * (topOffset + bottomOffset));
}

} // namespace Mona
