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
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

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

UInt16 MPEG4::ReadExpGolomb(BitReader& reader) {
	UInt8 i(0);
	while (reader.available() && !reader.read())
		++i;
	UInt16 result = reader.read<UInt16>(i);
	if (i > 15) {
		WARN("Exponential-Golomb code exceeding unsigned 16 bits");
		return 0;
	}
	return result + (1 << i) - 1;
}

} // namespace Mona
