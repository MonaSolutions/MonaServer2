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
class FLVWriter : public MediaWriter, public virtual Object {
public:
	// Composition time
	// compositionTime = (PTS - DTS) / 90.0 
	// http://stackoverflow.com/questions/7054954/the-composition-timects-when-wrapping-h-264-nalus

	FLVWriter() {}

	void		beginMedia(const OnWrite& onWrite);
	void		writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, AMF::TYPE_AUDIO, ToCodecs(tag), tag.isConfig, tag.time, 0, packet, onWrite); }
	void		writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { write(track, AMF::TYPE_VIDEO, ToCodecs(tag), tag.frame == Media::Video::FRAME_CONFIG, tag.time, tag.compositionOffset, packet, onWrite); }
	void		writeData(UInt16 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { write(track, AMF::TYPE_DATA, 0, false, 0, 0, packet, onWrite); }
	void		endMedia(const OnWrite& onWrite);

	static UInt8 ToCodecs(const Media::Audio::Tag& tag);
	static UInt8 ToCodecs(const Media::Video::Tag& tag) { return ((tag.frame== Media::Video::FRAME_CONFIG ? Media::Video::FRAME_KEY : tag.frame) << 4) | tag.codec; }

	static bool  ParseAVCConfig(const Packet& packet, Packet& sps, Packet& pps);
	static void  WriteAVCConfig(const Packet& sps, const Packet& pps, BinaryWriter& writer, const OnWrite& onWrite = nullptr);
private:
	void write(UInt16 track, AMF::Type type, UInt8 codecs, bool isConfig, UInt32 time, UInt32 compositionOffset, const Packet& packet, const OnWrite& onWrite);
	
	UInt8		_buffer[20];
	Packet		_sps;
	Packet		_pps;
};



} // namespace Mona
