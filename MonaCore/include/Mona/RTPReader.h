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
#include "Mona/Logs.h"

namespace Mona {

template<typename RTP_ProfileType>
struct RTPReader : MediaReader, virtual Object {
	// http://www.cse.wustl.edu/~jain/books/ftp/rtp.pdf
	// http://www.networksorcery.com/enp/protocol/rtp.htm
	// /!\ Packet passed have to be whole, not fragmented

	template <typename ...ProfileArgs>
	RTPReader(ProfileArgs... args) : _first(true), _profile(args ...) {}

private:
	UInt32	parse(const Packet& packet, Media::Source& source) {

		BinaryReader reader(packet.data(), packet.size());

		if (reader.available() < 12) {
			CRITIC("RTP packet have to be whole, not fragmented");
			return 0;
		}
		
		UInt8 byte(reader.read8());
		UInt8 padding(0);
		if (byte & 0x20) // padding, last byte is the padding size
			 padding = packet.data()[packet.size()-1];

		bool extension = byte & 0x10;
		UInt8 csrcCount = byte & 0x0F; // TODO onLost!!

		byte = reader.read8();
		bool marker(byte & 0x80);
		UInt8 playloadType(byte & 0x7F);
		if (playloadType != _profile.playloadType()) {
			ERROR(typeof<RTP_ProfileType>()," configured to receive ",_profile.playloadType()," playload type and not ",playloadType)
			return 0;
		}

		UInt16 count(reader.read16());
		UInt16 lost(_first ? 0 : (count-_count-1));
		_count = count;
		
		UInt32 time(reader.read32());
		UInt32 ssrc(reader.read32());
		if (ssrc != _ssrc) {
			if (_first)
				WARN(typeof<RTP_ProfileType>()," SSRC have changed, ", _ssrc, " => ", ssrc);
			_ssrc = ssrc;
		}

		while (csrcCount--)
			reader.read32();
	
		_profile.parse(time, extension, lost, Packet(packet, reader.current(),reader.available()-padding) , source);

		_first = false;
		return 0;
	}
	void onFlush(const Packet& packet, Media::Source& source) {
		_profile.flush(source);
		_first = true;
		_time = 0;
		MediaReader::onFlush(packet, source);
	}

	RTP_ProfileType _profile;

	UInt32 _time;
	UInt32 _ssrc;
	UInt16 _count;
	bool   _first;

};


} // namespace Mona
