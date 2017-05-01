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

#include "Mona/ADTSReader.h"
#include "Mona/ADTSWriter.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

	
UInt8 ADTSReader::ReadConfig(const UInt8* data, UInt32 size, UInt32& rate, UInt8& channels) {
	UInt8 rateIndex;
	UInt8 type(ReadConfig(data, size, rateIndex, channels));
	if (!type)
		return 0;
	rate = RateFromIndex(rateIndex);
	return type;
}

UInt8 ADTSReader::ReadConfig(const UInt8* data, UInt32 size, UInt8& rateIndex, UInt8& channels) {
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf

	if (size < 2) {
		WARN("AAC configuration packet must have a minimum size of 2 bytes");
		return 0;
	}

	UInt8 type(data[0]>>3);
	if(!type) {
		WARN("AAC configuration packet invalid");
		return 0;
	}

	rateIndex = (data[0] & 3)<<1;
	rateIndex |= data[1]>>7;

	channels = (data[1] >> 3) & 0x0F;

	return type;
}


UInt32 ADTSReader::parse(const Packet& packet, Media::Source& source) {
	
	
	BinaryReader reader(packet.data(), packet.size());

	while (reader.available()) {

		if (!_size) {
			const UInt8* header(reader.current());
			if (reader.available() < 7)
				return reader.available();

			// 2 first bytes (syncword)
			if (header[0] != 0xFF || (header[1] & 0xF0) != 0xF0) {
				if (!_syncError) {
					WARN("ADTS syncword xFFF not found");
					_syncError = true;
				}
				reader.next(header[0] != 0xFF ? 1 : 2);
				continue;
			}
			_syncError = false;

			// CRC ?
			UInt8 headerSize(header[1] & 0x01 ? 7 : 9);
			if (reader.available() < headerSize)
				return reader.current()-header;
			reader.next(headerSize);

			bool isAACMP2 = header[1] & 0x08 ? true : false;

			_size = (header[4] << 3) | ((header[5] & 0xe0) >> 5);
			if (!isAACMP2)
				_size |= (header[3] & 0x03) << 11;
			if (_size < headerSize) {
				_size = 0;
				WARN("ADTS Frame ", _size, " size error (inferior to header)");
				continue;
			}
			_size -= headerSize;

			// codec from Audio Object Type => https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Audio_Object_Types
			UInt8 value(header[2] >> 6);
			_tag.codec = value>30 && value<34 ? Media::Audio::CODEC_MP3 : Media::Audio::CODEC_AAC;

			value = (header[2] >> 2) & 0x0f;
			_tag.rate = RateFromIndex(value);

			// time
			_tag.time = time;
			// advance next time!
			time += UInt32(round(1024000.0/ _tag.rate)); // t = 1/rate... 1024 samples/frame and srMap is in kHz

			// one private bit
			_tag.channels = ((header[2] & 0x01) << 2) | ((header[3] >> 6) & 0x03);
			// if tag.channels==0 => info in inband PCE = too complicated to get, assumed than it's stereo (TODO?)
			// Keep it because WMP is unable to read inband PCE (so sound will not worked)
			if (!_tag.channels)
				_tag.channels = 2;
	
			// Config header
			if (_tag.isConfig) {
				// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
				// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
				// http://www.mpeg-audio.org/docs/w14751_(mpeg_AAC_TransportFormats).pdf
				UInt8 config[2];
				// ADTS profile 2 first bits => MPEG-4 Audio Object Type minus 1
				ADTSWriter::WriteConfig((header[2] >> 6) + 1, value, _tag.channels, config);
				source.writeAudio(track, _tag, Packet(config, 2));

				_tag.isConfig = false; // just one time
			}
			
		}

		if(reader.available()<_size)
			return reader.available();
	
		source.writeAudio(track, _tag, Packet(packet, reader.current(), _size));
		reader.next(_size);
		_size = 0;
	};

	return 0;
}

void ADTSReader::onFlush(const Packet& packet, Media::Source& source) {
	if (_size)
		source.writeAudio(track, _tag, packet);
	_tag.isConfig = true; // to send AAC header on next parse!
	_size = 0;
	_syncError = false;
	TrackReader::onFlush(packet, source);
}

} // namespace Mona
