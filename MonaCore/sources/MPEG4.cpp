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

#include "Mona/MPEG4.h"
#include "Mona/BitReader.h"

using namespace std;

namespace Mona {

Media::Video::Frame MPEG4::UpdateFrame(UInt8 type, Media::Video::Frame frame) {
	if (frame == Media::Video::FRAME_KEY || frame == Media::Video::FRAME_CONFIG)
		return frame;
	static Media::Video::Frame Frames[] = { Media::Video::FRAME_UNSPECIFIED, Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_KEY, Media::Video::FRAME_INFO, Media::Video::FRAME_CONFIG, Media::Video::FRAME_CONFIG };
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

bool MPEG4::ParseVideoConfig(const Packet& packet, Packet& sps, Packet& pps) {
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


UInt32 MPEG4::ReadVideoConfig(const UInt8* data, UInt32 size, Buffer& buffer) {
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

BinaryWriter& MPEG4::WriteVideoConfig(const Packet& sps, const Packet& pps, BinaryWriter& writer) {
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


UInt8 MPEG4::ReadAudioConfig(const UInt8* data, UInt32 size, UInt32& rate, UInt8& channels) {
	UInt8 rateIndex;
	UInt8 type(ReadAudioConfig(data, size, rateIndex, channels));
	if (!type)
		return 0;
	rate = RateFromIndex(rateIndex);
	return type;
}

UInt8 MPEG4::ReadAudioConfig(const UInt8* data, UInt32 size, UInt8& rateIndex, UInt8& channels) {
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf

	if (size < 2) {
		WARN("AAC configuration packet must have a minimum size of 2 bytes");
		return 0;
	}

	UInt8 type(data[0] >> 3);
	if (!type) {
		WARN("AAC configuration packet invalid");
		return 0;
	}

	rateIndex = (data[0] & 3) << 1;
	rateIndex |= data[1] >> 7;

	channels = (data[1] >> 3) & 0x0F;

	return type;
}


UInt8* MPEG4::WriteAudioConfig(UInt8 type, UInt8 rateIndex, UInt8 channels, UInt8 config[2]) {
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf

	config[0] = type << 3; // 5 bits of object type (ADTS profile 2 first bits => MPEG-4 Audio Object Type minus 1)
	config[0] |= (rateIndex & 0x0F) >> 1;
	config[1] = (rateIndex & 0x01) << 7;
	config[1] |= (channels & 0x0F) << 3;
	return config;
}


UInt8 MPEG4::RateToIndex(UInt32 rate) {
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf

	static map<UInt32, UInt8> Rates = {
		{ 96000, 0 },
		{ 88200, 1 },
		{ 64000, 2 },
		{ 48000, 3 },
		{ 44100, 4 },
		{ 32000, 5 },
		{ 24000, 6 },
		{ 22050, 7 },
		{ 16000, 8 },
		{ 12000, 9 },
		{ 11025, 10 },
		{ 8000, 11 },
		{ 7350, 12 }
	};
	auto it(Rates.lower_bound(rate));
	if (it == Rates.end()) {
		// > 96000
		it = Rates.begin();
		WARN("ADTS format doesn't support ", rate, " audio rate, set to 96000");
	} else if (it->first != rate)
		WARN("ADTS format doesn't support ", rate, " audio rate, set to ", it->first);
	return it->second;
}


static UInt16 ReadExpGolomb(BitReader& reader) {
	UInt8 i(0);
	while (!reader.read())
		++i;
	UInt16 result = reader.read<UInt16>(i);
	if (i > 15) {
		WARN("Exponential-Golomb code exceeding unsigned 16 bits");
		return 0;
	}
	return result + (1 << i) - 1;
}

UInt32 MPEG4::SPSToVideoDimension(const UInt8* data, UInt32 size) {
	if ((*data++ & 0x1f) != 7) {
		ERROR("Invalid SPS data");
		return 0;
	}

	BitReader reader(data, size-1);

	UInt16 leftOffset = 0, rightOffset = 0, topOffset = 0, bottomOffset = 0;
	UInt16 subWidthC = 0, subHeightC = 0;

	UInt8 idc = reader.read<UInt8>();
	reader.next(16); // constraincts
	ReadExpGolomb(reader); // seq_parameter_set_id

	switch (idc) {
		case 100:
		case 110:
		case 122:
		case 144:
		case 44:
		case 83:
		case 86:
		case 118:
			switch (ReadExpGolomb(reader)) { // chroma_format_idc
				case 1: // 4:2:0
					subWidthC = subHeightC = 2;
					break;
				case 2: // 4:2:2
					subWidthC = 2;
					subHeightC = 1;
					break;
				case 3: // 4:4:4
					if(!reader.read())
						subWidthC = subHeightC = 1; // separate_colour_plane_flag 
					break;
			}

			ReadExpGolomb(reader); // bit_depth_luma_minus8
			ReadExpGolomb(reader); // bit_depth_chroma_minus8
			reader.next(); // qpprime_y_zero_transform_bypass_flag
			if (reader.read()) { // seq_scaling_matrix_present_flag
				for (UInt8 i = 0; i < 8; ++i) {
					if (reader.read()) { // seq_scaling_list_present_flag
						UInt8 sizeOfScalingList = (i < 6) ? 16 : 64;
						UInt8 scale = 8;
						for (UInt8 j = 0; j < sizeOfScalingList; ++j) {
							Int16 delta = ReadExpGolomb(reader);
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

	ReadExpGolomb(reader); // log2_max_frame_num_minus4
	UInt16 picOrderCntType = ReadExpGolomb(reader);
	if (!picOrderCntType) {
		ReadExpGolomb(reader); // log2_max_pic_order_cnt_lsb_minus4
	} else if (picOrderCntType == 1) {
		reader.next(); // delta_pic_order_always_zero_flag
		ReadExpGolomb(reader); // offset_for_non_ref_pic
		ReadExpGolomb(reader); // offset_for_top_to_bottom_field
		UInt16 refFrames = ReadExpGolomb(reader);
		for (UInt16 i = 0; i < refFrames; ++i)
			ReadExpGolomb(reader); // sps->offset_for_ref_frame[ i ] = ReadSE();
	}
	ReadExpGolomb(reader); // max_num_ref_frames
	reader.next(); // gaps_in_frame_num_value_allowed_flag
	UInt16 picWidth = (ReadExpGolomb(reader) + 1) * 16;
	UInt16 picHeight = (ReadExpGolomb(reader) + 1) * 16;
	if (!reader.read()) { // frame_mbs_only_flag
		subHeightC *= 2;
		reader.next(); // mb_adaptive_frame_field_flag
	}

	reader.next(); // direct_8x8_inference_flag
	if (reader.read()) { // frame_cropping_flag
		leftOffset = ReadExpGolomb(reader);
		rightOffset = ReadExpGolomb(reader);
		topOffset = ReadExpGolomb(reader);
		bottomOffset = ReadExpGolomb(reader);
	}
	
	// return width << 16 | height;
	return ((picWidth - subWidthC * (bottomOffset + topOffset)) << 16) | (picHeight - subHeightC * (rightOffset + leftOffset));
}


} // namespace Mona
