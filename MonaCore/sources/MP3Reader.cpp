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

#include "Mona/MP3Reader.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

UInt32 MP3Reader::parse(Packet& buffer, Media::Source& source) {
	
	BinaryReader reader(buffer.data(), buffer.size());

	while (reader.available()) {

		if (!_size) {
			const UInt8* header(reader.current());
			if (reader.available() < 4)
				return reader.available();

			// 2 first bytes (syncword)
			if (header[0] != 0xFF || (header[1] & 0xF0) != 0xF0) {
				if (!_syncError) {
					WARN("MP3 syncword xFFF not found");
					_syncError = true;
				}
				reader.next(header[0] != 0xFF ? 1 : 2);
				continue;
			}
			_syncError = false;
		
			// CRC ?
			if (!(header[1] & 0x01) && reader.available() < 6)
				return reader.current()-header;
		
			UInt8 versionBit((header[1] >> 3) & 0x01);
			UInt8 layerBits((header[1] >> 1) & 0x03);
		
			// Bitrate
			static UInt32 Bitrates[2][3][16] = {
				{ // V2
					{ 0, 8000, 16000, 24000, 32000, 40000, 48000, 56000, 64000, 80000, 96000, 112000, 128000, 144000, 160000, 0 },  // L3
					{ 0, 8000, 16000, 24000, 32000, 40000, 48000, 56000, 64000, 80000, 96000, 112000, 128000, 144000, 160000, 0 },  // L2
					{ 0, 32000, 48000, 56000, 64000, 80000, 96000, 112000, 128000, 144000, 160000, 176000, 192000, 224000, 256000, 0 } // L1	
				},
				{ // V1
					{ 0, 32000, 40000, 48000, 56000, 64000, 80000, 96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000, 0 }, // L3
					{ 0, 32000, 48000, 56000, 64000, 80000, 96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000, 384000, 0 }, // L2
					{ 0, 32000, 64000, 96000, 128000, 160000, 192000, 224000, 256000, 288000, 320000, 352000, 384000, 416000, 448000, 0 } // L1
				}
			};
			UInt32 bitrate = Bitrates[versionBit][layerBits ? (layerBits-1) : layerBits][(header[2] >> 4) & 0x0F];

			// Rates
			static UInt16 Rates[] = { 22050, 24000, 16000 , 22050 };
			_tag.rate = Rates[(header[2] >> 2) & 0x03] * (versionBit+1);  // * by version

			
			// No math round but floor result required for next division!
			if (layerBits==0x03)
				_size = (UInt32(12.0 * bitrate / _tag.rate) + ((header[2]>> 1) & 0x01)) * 4; // layer 1
			else
				_size = UInt32(144.0 * bitrate / _tag.rate) + ((header[2]>> 1) & 0x01);  // layer 2 or 3

			if (_size > 16376) {
				WARN("MP3Reader parse an invalid frame size"); // possible on lost
				_size = 0;
				return 0;
			}

			// time
			_tag.time = time;
			time += UInt32(round(1024000.0/ _tag.rate)); // t = 1/rate... 1024 samples/frame and srMap is in kHz

			// channels
			if ((header[3]>>6)==0x03)
				_tag.channels = 1;
			else
				_tag.channels = 2;
		}

		if(reader.available()<_size)
			return reader.available();

		// AAC packet
		source.writeAudio(_tag, Packet(buffer, reader.current(),_size), track);
		reader.next(_size);
		_size = 0;
	};

	return 0;
}

void MP3Reader::onFlush(Packet& buffer, Media::Source& source) {
	if (_size)
		source.writeAudio(_tag, buffer, track);
	_size = 0;
	_syncError = false;
	MediaTrackReader::onFlush(buffer, source);
}

} // namespace Mona
