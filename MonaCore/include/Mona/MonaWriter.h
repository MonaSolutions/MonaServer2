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
AUDIO 	  [UInt8=>size header PAIR>4][tag header + track(UInt16=>optional)][UInt32=>size][...data...]
VIDEO	  [UInt8=>size header IMPAIR>4][tag header + track(UInt16=>optional)][UInt32=>size][...data...]
DATA 	  [UInt8=>size header<4][UInt8=>type + track(UInt16=>optional)][UInt32=>size][...data...] */
class MonaWriter : public MediaWriter, public virtual Object {
public:
	MonaWriter() {}

	void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { if(onWrite) write(track, tag, packet, onWrite); }
	void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { if (onWrite) write(track, tag, packet, onWrite); }
	void writeData(UInt16 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { if (onWrite) write(track, type, packet, onWrite); }

private:
	
	template<typename TagType>
	void write(UInt16 track, const TagType& tag, const Packet& packet, const OnWrite& onWrite) {
		onWrite(write(track, packet, tag.pack(BinaryWriter(_buffer, sizeof(_buffer)).write8(tag.packSize() + (track ? 2 : 0)))));
		onWrite(packet);
	}
	void write(UInt16 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
		onWrite(write(track, packet, BinaryWriter(_buffer, sizeof(_buffer)).write8(track ? 3 : 1).write8(type)));
		onWrite(packet);
	}
	BinaryWriter& write(UInt16 track, const Packet& packet, BinaryWriter& writer) {
		return track ? writer.write16(track).write32(packet.size()) : writer.write32(packet.size());
	}
	
	UInt8		_buffer[16];
};



} // namespace Mona
