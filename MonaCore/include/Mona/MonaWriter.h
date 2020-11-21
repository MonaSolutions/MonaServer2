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


/*!
[UInt32=>size][Media::Pack][...data...] */

struct MonaWriter : MediaWriter, virtual Object {
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, tag, packet, onWrite); }
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, tag, packet, onWrite); }
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
		if (track) // ignore data command (just subtitle)
			write(track, type, packet, onWrite);
	}
	void writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
		Media::Data::Type type(Media::Data::TYPE_JSON);
		const Packet& packet = properties.data(type);
		static const Packet Header(EXPAND("[\"@properties\","));
		write(0, Media::Data::TYPE_JSON, packet+1, onWrite, Header); // +1 to remove "["
	}
private:
	
	template<typename TagType>
	void write(UInt8 track, const TagType& tag, const Packet& packet, const OnWrite& onWrite, const Packet& header = Packet::Null()) {
		if (!onWrite)
			return;
		UInt8 buffer[9];
		BinaryWriter writer(buffer, sizeof(buffer));
		UInt32 size = header.size() + packet.size();
		Media::Pack(writer.write32(Media::PackedSize(tag, track) + size), tag, track);
		onWrite(Packet(writer.data(), writer.size()));
		if(header)
			onWrite(header);
		onWrite(packet);
	}

	
};



} // namespace Mona
