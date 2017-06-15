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

#pragma once

#include "Mona/Mona.h"
#include "Mona/MediaReader.h"

namespace Mona {


struct ADTSReader : virtual Object, MediaTrackReader {
	// Compatible AAC and MP3
	// http://wiki.multimedia.cx/index.php?title=ADTS
	// http://blog.olivierlanglois.net/index.php/2008/09/12/aac_adts_header_buffer_fullness_field
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html
	ADTSReader(UInt8 track=1) : _syncError(false), _size(0), MediaTrackReader(track) { _tag.isConfig = true; }

	static UInt8 ReadConfig(const UInt8* data, UInt32 size, UInt32& rate, UInt8& channels);
	static UInt8 ReadConfig(const UInt8* data, UInt32 size, UInt8& rateIndex, UInt8& channels);
	
private:
	UInt32 parse(const Packet& packet, Media::Source& source);
	void   onFlush(const Packet& packet, Media::Source& source);

	static UInt32 RateFromIndex(UInt8 index) {
		static UInt32 Rates[15] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 48000, 48000 }; // 1024000.0/ [ 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 ] => t = 1/rate... 1024 samples/frame (in kHz)
		return Rates[index];
	}

	UInt16			  _size;
	bool			  _syncError;
	Media::Audio::Tag _tag;
};


} // namespace Mona
