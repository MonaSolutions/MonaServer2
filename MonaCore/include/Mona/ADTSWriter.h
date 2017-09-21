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
#include "Mona/MediaWriter.h"

namespace Mona {


struct ADTSWriter : MediaTrackWriter, virtual Object {
	// Compatible AAC and MP3
	// http://wiki.multimedia.cx/index.php?title=ADTS
	// http://blog.olivierlanglois.net/index.php/2008/09/12/aac_adts_header_buffer_fullness_field
	// http://thompsonng.blogspot.fr/2010/06/adts-audio-data-transport-stream-frame.html
	// http://thompsonng.blogspot.fr/2010/03/aac-configuration.html

	ADTSWriter() : _codecType(0),_channels(0) {}

	void beginMedia();
	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize);
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize);
	
	static UInt8*	WriteConfig(UInt8 type, UInt32 rate, UInt8 channels, UInt8 config[2]) { return WriteConfig(type, RateToIndex(rate), channels, config); }
	static UInt8*	WriteConfig(UInt8 type, UInt8 rateIndex, UInt8 channels, UInt8 config[2]);

private:
	static UInt8	RateToIndex(UInt32 rate);

	UInt8	_codecType;
	UInt8	_rateIndex;
	UInt8	_channels;
};



} // namespace Mona
