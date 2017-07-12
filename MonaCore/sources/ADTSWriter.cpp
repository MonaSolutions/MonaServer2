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

#include "Mona/ADTSWriter.h"
#include "Mona/MPEG4.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

UInt8* ADTSWriter::WriteConfig(UInt8 type, UInt8 rateIndex, UInt8 channels, UInt8 config[2]) {
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf

	config[0] = type<<3; // 5 bits of object type (ADTS profile 2 first bits => MPEG-4 Audio Object Type minus 1)
	config[0] |= (rateIndex & 0x0F)>>1;
	config[1] = (rateIndex & 0x01)<<7;
	config[1] |= (channels&0x0F)<<3;
	return config;
}

UInt8 ADTSWriter::RateToIndex(UInt32 rate) {
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf

	static map<UInt32,UInt8> Rates = {
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
	} else if (it->first!=rate)
		WARN("ADTS format doesn't support ", rate, " audio rate, set to ", it->first);
	return it->second;
}

void ADTSWriter::beginMedia() {
	_codecType = 0;
	_channels = 0;
}

void ADTSWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	if (tag.codec != Media::Audio::CODEC_AAC && tag.codec != Media::Audio::CODEC_MP3 && tag.codec != Media::Audio::CODEC_MP3_8K) {
		ERROR("ADTS format doesn't support ", Media::Audio::CodecToString(tag.codec), " codec");
		return;
	}

	if (tag.isConfig) { // Read config packet to get object type
		// has configs! prefer AAC configs!
		UInt8 type(MPEG4::ReadAudioConfig(packet.data(), 2, _rateIndex, _channels));
		if (type)
			_codecType = type - 1; // minus 1
		return; // remove config packet (no need, already all infos in header)
	}

	if (!onWrite)
		return;

	UInt8 header[7];
	finalSize = packet.size()+sizeof(header);
	BinaryWriter writer(header, sizeof(header));
	if (tag.codec == Media::Audio::CODEC_AAC)
		writer.write16(0xFFF1); // MPEG4
	else {
		writer.write16(0xFFF9); // MPEG2
		// read MP3 header to get codecType
		if (packet.size() > 1 && *packet.data() == 0xFF) {
			UInt8 layerBits((packet.data()[1] >> 1) & 0x03);
			if (!layerBits)
				layerBits = 1;
			_codecType = 35 - layerBits;
		}
	}

	writer.write8((_codecType<<6) | ((_codecType ? _rateIndex : RateToIndex(tag.rate)) << 2) | (((_channels ? _channels : tag.channels) >> 2) & 0x01));
	writer.write32(((_channels ? _channels : tag.channels) & 0x03)<<30 | (finalSize & 0x1FFF)<<13 | 0x1FFC); // 0x1FFC => buffer fullness all bits to 1 + 1 AAC frame per ADTS frame minus 1 (for compatibility maximum)

	onWrite(Packet(writer)); // header
	onWrite(packet); // content
}

void ADTSWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	ERROR("ADTS is an audio container, impossible to write any video frame")
}


} // namespace Mona
