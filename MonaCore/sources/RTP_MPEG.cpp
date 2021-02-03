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

#include "Mona/RTP_MPEG.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

bool RTP_MPEG::writeAudio(const Media::Audio::Tag& tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite) {
	if (!writer) { // After header (new RTP packet)
		if (canWrite < 4)
			return false;
		writer.write32(0); // Frag_offset 
		canWrite -= 4;
	}

	if (canWrite > reader.available())
		canWrite = reader.available();
	writer.write(reader.current(), canWrite);
	reader.next(canWrite);
	return true;
}

bool RTP_MPEG::writeVideo(const Media::Video::Tag& tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite) {
	ERROR("RTP_MPEG profile has no video ability implemented yet");
	return false;
}

void RTP_MPEG::parse(UInt32 time, UInt8 extension, UInt16 lost, const Packet& packet, Media::Source& source) {
	ERROR("RTP_MPEG profile has no parsing ability implemented yet");
}
void RTP_MPEG::flush(Media::Source& source) {}


} // namespace Mona
