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

#include "Mona/RTP_H264.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


bool RTP_H264::writeAudio(Media::Audio::Tag tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite) {
	ERROR("RTP_H264 is a video RTP profile and can't write any audio frame");
	return false;
}

bool RTP_H264::writeVideo(Media::Video::Tag tag, BinaryReader& reader, BinaryWriter& writer, UInt16 canWrite) {
	if (tag.codec != Media::Video::CODEC_H264) {
		ERROR("RTP_H264 profile supports only H264 video codec");
		return false;
	}
	if (canWrite < 3)
		return true; // flush! require at less 3 bytes available to write
	canWrite -= 2;

	if (!_size) {
		do {
			if (!reader.available()) {
				_size = 0;
				return true; // flush
			}
			_size = reader.read32();
			if (_size > reader.available())
				_size = reader.available();
		} while (!_size);
		--_size;
		_type = reader.read8() & 0x1F;
		/// NRI info => The value of NRI MUST be the  maximum of all the NAL units carried in the aggregation packet
		// https://www.ietf.org/rfc/rfc3984.txt
		// 6, 9, 10, 11 or 12 => NRI=00
		// 5 (key), 7 or 8 (config) => NRI=11
		// 1, 2 (inter) => NRI=10
		// 3, 4 (inter) => NRI=01
		if (tag.frame == Media::Video::FRAME_CONFIG || tag.frame == Media::Video::FRAME_KEY)
			_type |= 0x60; // NRI=11
		else if(tag.frame && tag.frame<Media::Video::FRAME_INFO)
			_type |= 0x40; // NRI=10
		_fragmentation = false;
	}

	if (!writer) {
		// After header
		if (_fragmentation || _size >= canWrite) {
			// Fragmentation 
			if (_size < canWrite)
				canWrite = _size;
			writer.write8((_type & 0x60) | 28); // FU-A => (F(0)|NRI(3)|FU-A(28))
			writer.write8((_fragmentation ? 0 : 0x80) | (_size==canWrite ? 0x40 : 0) | (_type&0x1F)); // Start flag and End flag
			writer.write(reader.current(), canWrite);
			reader.next(canWrite);
			_size -= canWrite;
			_fragmentation = true;
			return true;
		}

		// here 0<=_size<canWrite(>=1, so >=3) => aggregation maybe possible
		writer.write8((_type & 0x60) | 24);// STAP-A => (F(0)|NRI(3)|STAP-A(24))			
	} else if(_size > canWrite)  // End of aggregation, new nal in new packet!
		return true;

	// aggregation, _size<canWrite
	writer.write16(_size);
	writer.write(reader.current(), _size);
	reader.next(_size);
	_size = 0;
	return false; // no flush always
}

void RTP_H264::parse(UInt32 time, UInt8 extension, UInt16 lost, const Packet& packet, Media::Source& source) {
	ERROR("RTP_H264 profile has no parsing ability implemented yet");
}
void RTP_H264::flush(Media::Source& source) {}


} // namespace Mona
