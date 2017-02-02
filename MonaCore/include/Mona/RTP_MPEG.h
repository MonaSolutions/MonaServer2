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


struct RTP_MPEG : virtual Object {
	// RTP MPA/MPV profile => https://tools.ietf.org/html/rfc2250
	// Playload types https://tools.ietf.org/html/rfc1890
	RTP_MPEG(UInt8 playloadType) : playloadType(playloadType), track(0), supportTracks(false) {}
	
	const UInt8	playloadType;
	const bool  supportTracks;
	UInt32		track;

	// Write
	bool writeAudio(Media::Audio::Tag tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite);
	bool writeVideo(Media::Video::Tag tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite);

	//Read
	void parse(UInt32 time, UInt8 extension, UInt16 lost, const Packet& packet, Media::Source& source);
	void flush(Media::Source& source);
};



} // namespace Mona
