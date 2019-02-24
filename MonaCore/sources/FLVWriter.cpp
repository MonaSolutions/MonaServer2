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

#include "Mona/FLVWriter.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


UInt8 FLVWriter::ToCodecs(const Media::Audio::Tag& tag) {
	UInt8 codecs(tag.channels >= 2 ? 1 : 0);
	if (tag.codec == Media::Audio::CODEC_AAC)
		codecs = 1; // always 1 for AAC, readen in inband PCE
	else if (tag.codec >= Media::Audio::CODEC_NELLYMOSER_16K && tag.codec <= Media::Audio::CODEC_NELLYMOSER)
		codecs = 0; // always 0 for NELLYMOSER
	else if (!tag.channels) {
		WARN("Number of Channels on Flash Audio track not indicated, set to stereo");
		codecs = 1; // stereo
	} else if (tag.channels > 2)
		ERROR("Flash doesn't support audio track with ", tag.channels, " channels (just mono or stereo are supported)");

	codecs |= 2; // always 16 bits!

	if (tag.codec == Media::Audio::CODEC_AAC)
		codecs |= (3 << 2); // Always 3 for AAC (real info in config packet)
	else if (tag.codec == Media::Audio::CODEC_MP3 && tag.rate == 8000)
		return codecs | (Media::Audio::CODEC_MP38K_FLV << 4); // soundRate = 0
	else if (tag.rate >= 44100) {
		codecs |= (3 << 2);
		if (tag.rate>44100)
			DEBUG(tag.rate, " rate not supported by Flash, set to 44100");
	} else if (tag.rate >= 22050) {
		codecs |= (2 << 2);
		if (tag.rate>22050)
			DEBUG(tag.rate, " rate not supported by Flash, set to 22050");
	} else if (tag.rate >= 11025) {
		codecs |= (1<<2);
		if (tag.rate>11025)
			DEBUG(tag.rate, " rate not supported by Flash, set to 11025");
	} else if (tag.rate != 5512)
		DEBUG(tag.rate, " rate not supported by Flash, set to 5512");

	return codecs | (tag.codec << 4);
}

void FLVWriter::beginMedia(const OnWrite& onWrite) {
	if (!onWrite)
		return;
	static const Packet Header(EXPAND("\x46\x4c\x56\x01\x05\x00\x00\x00\x09\x00\x00\x00\x00"));
	onWrite(Header);
}

void FLVWriter::write(UInt8 track, AMF::Type type, UInt8 codecs, bool isConfig, UInt32 time, UInt16 compositionOffset, const Packet& packet, const OnWrite& onWrite) {
	if (!onWrite)
		return;
	TRACE(type, " ", codecs, " ", isConfig, " ", time," ", compositionOffset);

	bool isAVC(false);

	UInt32 size(packet.size());

	Packet sps, pps, vps;
	UInt8 codec;
	switch (type) {
		case AMF::TYPE_VIDEO:
			++size; // for codec byte
			codec = codecs & 0x0F;
			if ((isAVC = (codec == Media::Video::CODEC_H264 || codec == Media::Video::CODEC_HEVC))) {
				size += 4; // for config byte + composition offset 3 bytes
				if (isConfig) {
					// find just sps and pps data, ignore the rest
					size -= packet.size();
					if (((codec == Media::Video::CODEC_H264) && !AVC::ParseVideoConfig(packet, sps, pps)) || ((codec == Media::Video::CODEC_HEVC) && !HEVC::ParseVideoConfig(packet, vps, sps, pps)))
						return;
					size += vps.size() + sps.size() + pps.size() + ((codec == Media::Video::CODEC_H264) ? 11 : 38); // TODO: Use a constant for video config header size
				}
			}
			break;
		case AMF::TYPE_AUDIO:
			if (packet || !isConfig)
				++size; // for codec byte
			else
				codecs = 0; // audio end
			if ((isAVC = ((codecs >> 4) == Media::Audio::CODEC_AAC)))
				++size; // for config byte
			break;
		case AMF::TYPE_EMPTY: // Metadata!
			break;
		default:;
	}

	
	/// 11 bytes of header
	shared<Buffer> pBuffer(SET);
	BinaryWriter writer(*pBuffer);

	writer.write8(type);
	writer.write24(size);
	writer.write24(time);
	writer.write8(time >> 24);
	writer.write24(track); // stream id used for track!
	
	// Codec type
	if(codecs)
		writer.write8(codecs);

	if (isAVC) {
		 // H264/HEVC or AAC!
		writer.write8(isConfig ? 0 : 1);
		// Composition offset
		if (type == AMF::TYPE_VIDEO)
			writer.write24(compositionOffset);
	}

	if (!sps) {
		onWrite(Packet(pBuffer)); // header
		pBuffer.set();
		if (packet)
			onWrite(packet);
	} else if (vps)
		HEVC::WriteVideoConfig(writer, vps, sps, pps); // write vps sps + pps
	else
		AVC::WriteVideoConfig(writer, sps, pps); // write sps + pps

	BinaryWriter(*pBuffer).write32(11 + size); // footer
	onWrite(Packet(pBuffer));
}



} // namespace Mona
