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



} // namespace Mona
