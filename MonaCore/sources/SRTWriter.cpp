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

#include "Mona/SRTWriter.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

void SRTWriter::beginMedia(const OnWrite& onWrite) {
	_time = _index = 0;
}

void SRTWriter::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	if (Media::Data::TYPE_TEXT != type || !onWrite)
		return;
	shared<Buffer>	pBuffer(SET);
	String::Append(*pBuffer, ++_index, '\n', String::Date(_time, timeFormat), " --> ", String::Date(Date(_time + Media::ComputeTextDuration(packet.size())), timeFormat));
	String::Append(*pBuffer, '\n', packet, "\n\n");
	onWrite(Packet(pBuffer));
	DEBUG("cc", track, "(", _time,") => ", packet);
}



} // namespace Mona
