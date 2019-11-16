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
SRT subtitles compatible VTT */
struct SRTWriter : MediaWriter, virtual Object {
	SRTWriter() : timeFormat("%TH:%M:%S,%i") {}

	const char*	 timeFormat;

	void beginMedia(const OnWrite& onWrite);
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { _time = tag.time; }
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { _time = tag.time; }
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite);
	
private:
	UInt32			_index;
	UInt32			_time;
};



} // namespace Mona
