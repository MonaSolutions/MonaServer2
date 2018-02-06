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

#include "Mona/HEVC.h"

using namespace std;

namespace Mona {


// Custom version of BitReader wich ignore the 03 byte when readding a serie of 0x000003
struct BitstreamReader : BitReader, virtual Object {
	BitstreamReader(const UInt8* data, UInt32 size) : BitReader(data, size) {}

	virtual bool read() {
		if (_current == _end)
			return false;
		bool result = (*_current & (0x80 >> _bit++)) ? true : false;
		if (_bit == 8) {
			_bit = 0;
			if (++_current != _end && position() >= 16 && *_current == 3 && *(_current - 1) == 0 && *(_current - 2) == 0)
				++_current; // ignore 0x03 after two 0x00
		}
		return result;
	}

	template<typename ResultType>
	ResultType read(UInt8 count = (sizeof(ResultType) * 8)) { return BitReader::read<ResultType>(count); }

	virtual UInt64 next(UInt64 count = 1) {
		UInt64 gotten(0);
		while (_current != _end && count--) {
			++gotten;
			if (++_bit == 8) {
				_bit = 0;
				if (++_current != _end && position() >= 16 && *_current == 3 && *(_current - 1) == 0 && *(_current - 2) == 0)
					++_current; // ignore 0x03 after two 0x00
			}
		}
		return gotten;
	}
};

Media::Video::Frame HEVC::Frames[] = {
	Media::Video::FRAME_INTER, // NAL TRAIL N = 0
	Media::Video::FRAME_INTER, // NAL TRAIL R = 1
	Media::Video::FRAME_INTER, // NAL TSA N = 2
	Media::Video::FRAME_INTER, // NAL TSA R = 3
	Media::Video::FRAME_INTER, // NAL STSA N = 4
	Media::Video::FRAME_INTER, // NAL STSA R = 5
	Media::Video::FRAME_INTER, // NAL RADL N = 6
	Media::Video::FRAME_INTER, // NAL RADL R = 7
	Media::Video::FRAME_INTER, // NAL RASL N = 8
	Media::Video::FRAME_INTER, // NAL RASL R = 9
	Media::Video::FRAME_UNSPECIFIED, // 10
	Media::Video::FRAME_UNSPECIFIED, // 11
	Media::Video::FRAME_UNSPECIFIED, // 12
	Media::Video::FRAME_UNSPECIFIED, // 13
	Media::Video::FRAME_UNSPECIFIED, // 14
	Media::Video::FRAME_UNSPECIFIED, // 15
	Media::Video::FRAME_KEY, // NAL BLA W LP = 16
	Media::Video::FRAME_KEY, // NAL BLA W RADL = 17
	Media::Video::FRAME_KEY, // NAL BLA N LP = 18
	Media::Video::FRAME_KEY, // IDR W RADL = 19
	Media::Video::FRAME_KEY, // IDR N LP = 20
	Media::Video::FRAME_KEY, // NAL CRA NUT = 21
	Media::Video::FRAME_UNSPECIFIED, // 22
	Media::Video::FRAME_KEY, // NAL IRAP VCL23 = 23
	Media::Video::FRAME_UNSPECIFIED, // 24
	Media::Video::FRAME_UNSPECIFIED, // 25
	Media::Video::FRAME_UNSPECIFIED, // 26
	Media::Video::FRAME_UNSPECIFIED, // 27
	Media::Video::FRAME_UNSPECIFIED, // 28
	Media::Video::FRAME_UNSPECIFIED, // 29
	Media::Video::FRAME_UNSPECIFIED, // 30
	Media::Video::FRAME_UNSPECIFIED, // 31
	Media::Video::FRAME_CONFIG, // VPS = 32
	Media::Video::FRAME_CONFIG, // SPS = 33
	Media::Video::FRAME_CONFIG, // PPS = 34
	Media::Video::FRAME_UNSPECIFIED, // Access Unit Delimiter = 35
	Media::Video::FRAME_UNSPECIFIED, // NAL EOS NUT = 36
	Media::Video::FRAME_UNSPECIFIED, // NAL EOB NUT = 37
	Media::Video::FRAME_UNSPECIFIED, // NAL FD NUT = 38
	Media::Video::FRAME_INFO, // SEI PREFIX = 39
	Media::Video::FRAME_INFO, // SEI SUFFIX = 40
};

Media::Video::Frame HEVC::UpdateFrame(UInt8 type, Media::Video::Frame frame) {
	if (frame == Media::Video::FRAME_KEY || frame == Media::Video::FRAME_CONFIG)
		return frame;

	type = Frames[type];
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


bool HEVC::ParseVideoConfig(const Packet& packet, Packet& vps, Packet& sps, Packet& pps) {
	BinaryReader reader(packet.data(), packet.size());

	// VPS, SPS & PPS
	UInt32 length;
	while (reader.available()) {
		length = reader.read32();
		if (length > reader.available())
			length = reader.available();
		if (!length)
			continue;
		UInt8 id((*reader.current() & 0x7f) >> 1);
		if (id == 32)
			vps.set(packet, reader.current(), length);
		else if (id == 33)
			sps.set(packet, reader.current(), length);
		else if (id == 34)
			pps.set(packet, reader.current(), length);
		reader.next(length);
	}
	if (sps && vps)
		return true; // pps not mandatory
	WARN("HEVC configuration malformed");
	return false;
}


UInt32 HEVC::ReadVideoConfig(const UInt8* data, UInt32 size, Buffer& buffer) {
	BinaryReader reader(data, size);
	BinaryWriter writer(buffer);

	reader.next(22); // header
	UInt8 numOfArrays = reader.read8();

	// VPS, SPS and PPS
	while (reader.available() >= 3 && numOfArrays--) {
		UInt32 pos = writer.size();

		UInt8 nalType = reader.read8(); // arrayCompleteness + reserved + nal unit type
		UInt16 nalus = reader.read16();
		writer.next(4); // size

		switch (nalType & 0x3f) {
		case 0x20:
		case 0x21:
		case 0x22:
			break;
		default:
			ERROR("Unexpected type received : ", nalType & 0x3f, " (expected VPS, SPS & PPS)")
			return reader.position();
		}		

		for (UInt16 i = 0; i < nalus; ++i) {
			UInt16 nalULength = reader.read16();
			writer.write(reader.current(), nalULength);
			reader.next(nalULength);
		}
		BinaryWriter(BIN writer.data() + pos, 4).write32(writer.size() - pos - 4);
		// PPS received, the rest is SEI and other : ignored
		if ((nalType & 0x3f) == 0x22)
			break;
	}
	return size; // we want to ignore the remaining data otherwise it will overwrite the codecs
}

BinaryWriter& HEVC::WriteVideoConfig(BinaryWriter& writer, const Packet& vps, const Packet& sps, const Packet& pps) {
	// HEVC mp4 header (TODO: generate or save the header, https://stackoverflow.com/questions/32697608/where-can-i-find-hevc-h-265-specs)
	writer.write("\x1\x1\x60\x0\x0\x0\x90\x0\x0\x0\x0\x0\x3c\xf0\x0\xfc\xfd\xf8\xf8\x0\x0\xf", 22);
	writer.write8(3); // number of arrays

	// vps
	writer.write8(0x20).write16(1).write16(vps.size()).write(vps); // type + nb of nalus + size + config packet

	// sps
	writer.write8(0x21).write16(1).write16(sps.size()).write(sps);

	// pps
	writer.write8(0x22).write16(1).write16(pps.size()).write(pps);
	return writer;
}

bool HEVC::ProcessProfileTierLevel(UInt8 max_sub_layers_minus1, BitstreamReader& reader) {

	reader.next(8); // general_profile_space, general_tier_flag, general_profile_idc

	reader.next(32); // general_profile_compatibility_flag;

	reader.next(4); // general_progressive_source_flag, general_interlaced_source_flag, general_non_packed_constraint_flag, general_frame_only_constraint_flag

	reader.next(32);
	reader.next(12);
	reader.next(8); // general_level_idc

	vector<char> subLayerProfiles;
	vector<char> subLayerLevels;
	for (UInt8 i = 0; i < max_sub_layers_minus1; i++) {
		subLayerProfiles.push_back(reader.read());
		subLayerLevels.push_back(reader.read());
	}

	if (max_sub_layers_minus1 > 0) {
		for (UInt8 i = max_sub_layers_minus1; i<8; i++)
			reader.next(2);
	}

	for (UInt8 i = 0; i<max_sub_layers_minus1; i++) {

		if (subLayerProfiles[i]) {
			reader.next(8); // sub_layer_profile_space, sub_layer_tier_flag, sub_layer_profile_idc
			reader.next(32); // sub_layer_profile_compatibility_flag

			reader.next(4); // sub_layer_progressive_source_flag, sub_layer_interlaced_source_flag, sub_layer_non_packed_constraint_flag, sub_layer_frame_only_constraint_flag
			reader.next(32);
			reader.next(12);
		}

		if (subLayerLevels[i])
			reader.next(8); // sub_layer_level_idc
	}

	return true;
}

UInt32 HEVC::SPSToVideoDimension(const UInt8* data, UInt32 size) {
	BitstreamReader reader(data, size);
	if (((reader.read<UInt8>(7) & 0x3f)) != 33) { // forbidden zero bit, SPS type
		ERROR("Invalid SPS data");
		return 0;
	}
	reader.next(9); // nuh layer id, nuh temporal id plus1

	reader.next(4); // video_parameter_set_id
	UInt8 max_sub_layers_minus1 = reader.read<UInt8>(3);
	reader.next(); // temporal_id_nesting_flag

	ProcessProfileTierLevel(max_sub_layers_minus1, reader);

	MPEG4::ReadExpGolomb(reader); // sps_seq_parameter_set_id
	//  psps -> sps_seq_parameter_set_id = 0;
	UInt16 chroma_format_idc = MPEG4::ReadExpGolomb(reader);

	if (chroma_format_idc == 3)
		reader.next(); // separate_colour_plane_flag

	UInt16 pic_width_in_luma_samples = MPEG4::ReadExpGolomb(reader);
	UInt16 pic_height_in_luma_samples = MPEG4::ReadExpGolomb(reader);
	
	// we ignore the rest of sps, not useful here

	// return width << 16 | height;
	return (pic_width_in_luma_samples << 16) | pic_height_in_luma_samples;
}

} // namespace Mona
