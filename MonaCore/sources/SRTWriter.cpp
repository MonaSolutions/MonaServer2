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

void SRTWriter::endMedia(const OnWrite& onWrite) {
	_pBuffer.reset();
}

void SRTWriter::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	if (Media::Data::TYPE_TEXT != type || !onWrite)
		return;
	BinaryWriter writer(BUFFER_RESET(_pBuffer, 0));
	String::Append(writer, ++_index, '\n', String::Date(Date(_time), _timeFormat), " --> ", String::Date(Date(_time + (UInt32)min(max(packet.size() / 20.0, 3) * 1000, 10000)), _timeFormat), '\n');
	writer.write(packet).write(EXPAND("\n\n"));
	onWrite(Packet(_pBuffer));
	DEBUG("cc", track, "(", _time,") => ", string(STR packet.data(), packet.size()));
}



} // namespace Mona
