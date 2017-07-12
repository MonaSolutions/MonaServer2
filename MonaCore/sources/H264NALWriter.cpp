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

#include "Mona/H264NALWriter.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


H264NALWriter::H264NALWriter() {
	// Unit Access demilited requires by some plugins like HLSFlash!
	memcpy(_buffer, EXPAND("\x00\x00\x00\x01\x09\xF0\x00\x00\x00\x01"));
}

void H264NALWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	if (!onWrite)
		return;
	if (tag.codec != Media::Video::CODEC_H264) {
		ERROR("H264NAL format supports only H264 video codec");
		return;
	}
	// 9 => Access time unit required in TS container (MPEG2) => https://mailman.videolan.org/pipermail/x264-devel/2005-April/000469.html

	 // NAL UNIT delimiter at the beginning + on time change + when NAL type change (group config packet (7 or 8) in different NAL access unit than other frames)
	// (NOTE 2 – Sequence parameter set NAL units or picture parameter set NAL units may be present in an access unit,
	// but cannot follow the last VCL NAL unit of the primary coded picture within the access unit,
	// as this condition would specify the start of a new access unit.)
	// + on time change!

	// About 00 00 01 and 00 00 00 01 difference => http://stackoverflow.com/questions/23516805/h264-nal-unit-prefixes
	// 00 00 00 01 for NAL Type 7 (SPS), 8 (PPS) and after NAL UNIT delimiter

	finalSize = 0;
	
	if (_time != tag.time) {
		_time = tag.time;
		_nal = NAL_START;
	}

	Nal nal(_nal);

	BinaryReader reader(packet.data(), packet.size());
	while (reader.available()) {
		UInt32 size(reader.read32());
		if (size > reader.available())
			size = reader.available();
		if (!size)
			continue;
		UInt8 type((*reader.current() & 0x1F));
		if(type!=9) {
			Nal newNal = (type >= 1 && type <= 5) ? NAL_VCL : ((type == 7 || type == 8) ? NAL_CONFIG : NAL_UNDEFINED);
			if (nal == NAL_START || (nal && newNal != nal))
				finalSize += 10;  // NAL unit delimiter + 00 00 00 01 prefix
			else if (type == 7 || type == 8)
				finalSize += 4; // 00 00 00 01 prefix
			else
				finalSize += 3; // 00 00 01 prefix
			finalSize += size;
			nal = newNal;
		} else
			nal = NAL_START;
		reader.next(size);
	}
	reader.reset();

	while (reader.available()) {
		UInt32 size(reader.read32());
		if (size > reader.available())
			size = reader.available();
		if (!size)
			continue;
		UInt8 type((*reader.current() & 0x1F));
		if (type != 9) {
			Nal newNal = (type >= 1 && type <= 5) ? NAL_VCL : ((type == 7 || type == 8) ? NAL_CONFIG : NAL_UNDEFINED);
			if (_nal == NAL_START || (_nal && newNal != _nal))
				onWrite(Packet(_buffer, 10));  // NAL unit delimiter + 00 00 00 01 prefix
			else if (type == 7 || type == 8)
				onWrite(Packet(_buffer, 4)); // 00 00 00 01 prefix
			else
				onWrite(Packet(_buffer+1, 3)); // 00 00 00 01 prefix
			onWrite(Packet(packet, reader.current(), size));
			_nal = newNal;
		} else
			_nal = NAL_START;
		reader.next(size);
	}
}


void H264NALWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	ERROR("H264NAL is an video container and can't write any audio frame")
}




} // namespace Mona
