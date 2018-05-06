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

void ADTSWriter::beginMedia() {
	_codecType = 0;
	_channels = 0;
}

void ADTSWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	if (tag.codec != Media::Audio::CODEC_AAC && tag.codec != Media::Audio::CODEC_MP3) {
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

	writer.write8((_codecType<<6) | ((_codecType ? _rateIndex : MPEG4::RateToIndex(tag.rate)) << 2) | (((_channels ? _channels : tag.channels) >> 2) & 0x01));
	writer.write32(((_channels ? _channels : tag.channels) & 0x03)<<30 | (finalSize & 0x1FFF)<<13 | 0x1FFC); // 0x1FFC => buffer fullness all bits to 1 + 1 AAC frame per ADTS frame minus 1 (for compatibility maximum)

	onWrite(Packet(writer.data(), writer.size())); // header
	onWrite(packet); // content
}

void ADTSWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	ERROR("ADTS is an audio container, impossible to write any video frame")
}


} // namespace Mona
