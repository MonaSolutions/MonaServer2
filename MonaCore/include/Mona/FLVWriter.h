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
#include "Mona/AMF.h"

namespace Mona {


/*!
Real Time Media writer => Flash media format */
struct FLVWriter : MediaWriter, virtual Object {
	// Composition time
	// compositionTime = (PTS - DTS) / 90.0 
	// http://stackoverflow.com/questions/7054954/the-composition-timects-when-wrapping-h-264-nalus

	FLVWriter() {}

	void		beginMedia(const OnWrite& onWrite);
	void		writeProperties(const Media::Properties& properties, const OnWrite& onWrite) { Media::Data::Type type(Media::Data::TYPE_AMF);  write(0, AMF::TYPE_EMPTY, 0, false, 0, 0, properties.data(type), onWrite); }
	void		writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, AMF::TYPE_AUDIO, ToCodecs(tag), tag.isConfig, tag.time, 0, packet, onWrite); }
	void		writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, AMF::TYPE_VIDEO, ToCodecs(tag), tag.frame == Media::Video::FRAME_CONFIG, tag.time, tag.compositionOffset, packet, onWrite); }
	void		writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { write(track, AMF::TYPE_DATA, 0, false, 0, 0, packet, onWrite); }
	void		endMedia(const OnWrite& onWrite) {}

	static UInt8 ToCodecs(const Media::Audio::Tag& tag);
	static UInt8 ToCodecs(const Media::Video::Tag& tag) { return ((tag.frame== Media::Video::FRAME_CONFIG ? Media::Video::FRAME_KEY : tag.frame) << 4) | tag.codec; }

private:
	void write(UInt8 track, AMF::Type type, UInt8 codecs, bool isConfig, UInt32 time, UInt16 compositionOffset, const Packet& packet, const OnWrite& onWrite);
};



} // namespace Mona
