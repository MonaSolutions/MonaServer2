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

#include "Mona/NALNetWriter.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

template <> 
const Packet NALNetWriter<AVC>::_Unit(EXPAND("\x00\x00\x00\x01\x09\xF0\x00\x00\x00\x01")); // Unit Access demilited requires by some plugins like HLSFlash!
template <>
NALNetWriter<AVC>::NALNetWriter() {}

template <>
const Packet NALNetWriter<HEVC>::_Unit(EXPAND("\x00\x00\x00\x01\x46\x01\x50\x00\x00\x00\x01")); // Unit Access demilited requires by some plugins like HLSFlash!
template <>
NALNetWriter<HEVC>::NALNetWriter() {}

template <class VideoType>
void NALNetWriter<VideoType>::writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	if (!onWrite)
		return;
	if (tag.codec != VideoType::CODEC) {
		ERROR("NALNetWriter format supports only H264 and HEVC video codec");
		return;
	}
	// Access time unit required in TS container (MPEG2) => https://mailman.videolan.org/pipermail/x264-devel/2005-April/000469.html

	 // NAL UNIT delimiter at the beginning + on time change + when NAL type change (group config packet in different NAL access unit than other frames)
	// (NOTE 2 – Sequence parameter set NAL units or picture parameter set NAL units may be present in an access unit,
	// but cannot follow the last VCL NAL unit of the primary coded picture within the access unit,
	// as this condition would specify the start of a new access unit.)
	// + on time change!

	// About 00 00 01 and 00 00 00 01 difference => http://stackoverflow.com/questions/23516805/h264-nal-unit-prefixes
	// 00 00 00 01 for NAL VPS, SPS & PPS and after NAL UNIT delimiter

	finalSize = 0;
	
	if (_time != tag.time) {
		_time = tag.time;
		_nal = NAL_START;
	}

	Nal nal(_nal);

	BinaryReader reader(packet.data(), packet.size());
	while (reader.available()) {
		UInt32 size(reader.read32());
		if (size > reader.available())
			size = reader.available();
		if (!size)
			continue;
		UInt8 type(VideoType::NalType(*reader.current()));
		if(type != VideoType::NAL_AUD) {
			Media::Video::Frame frame = VideoType::Frames[type];
			Nal newNal = (frame == Media::Video::FRAME_INTER || frame == Media::Video::FRAME_KEY) ? NAL_VCL : ((frame == Media::Video::FRAME_CONFIG) ? NAL_CONFIG : NAL_UNDEFINED);
			if (nal == NAL_START || (nal && newNal != nal))
				finalSize += _Unit.size();  // NAL unit delimiter + 00 00 00 01 prefix
			else if (newNal == NAL_CONFIG)
				finalSize += 4; // 00 00 00 01 prefix
			else
				finalSize += 3; // 00 00 01 prefix
			finalSize += size;
			nal = newNal;
		} else
			nal = NAL_START;
		reader.next(size);
	}
	reader.reset();

	while (reader.available()) {
		UInt32 size(reader.read32());
		if (size > reader.available())
			size = reader.available();
		if (!size)
			continue;
		UInt8 type(VideoType::NalType(*reader.current()));
		if (type != VideoType::NAL_AUD) {
			Media::Video::Frame frame = VideoType::Frames[type];
			Nal newNal = (frame == Media::Video::FRAME_INTER || frame == Media::Video::FRAME_KEY) ? NAL_VCL : ((frame == Media::Video::FRAME_CONFIG) ? NAL_CONFIG : NAL_UNDEFINED);
			if (_nal == NAL_START || (_nal && newNal != _nal))
				onWrite(_Unit);  // NAL unit delimiter + 00 00 00 01 prefix
			else if (newNal == NAL_CONFIG)
				onWrite(Packet(_Unit, _Unit.data(), 4)); // 00 00 00 01 prefix
			else
				onWrite(Packet(_Unit, _Unit.data()+1, 3)); // 00 00 00 01 prefix
			onWrite(Packet(packet, reader.current(), size));
			_nal = newNal;
		} else
			_nal = NAL_START;
		reader.next(size);
	}
}

template <class VideoType>
void NALNetWriter<VideoType>::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	ERROR("NALNetWriter is a video container and can't write any audio frame")
}




} // namespace Mona
