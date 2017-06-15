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

#include "Mona/H264NALReader.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


UInt32 H264NALReader::parse(const Packet& packet, Media::Source& source) {

	const UInt8* cur(packet.data());
	const UInt8* end(packet.data() + packet.size());
	const UInt8* nal(cur);	// assume that the copy will be from start-of-data


	while(cur<end) {
		
		UInt8 value(*cur++);

		// About 00 00 01 and 00 00 00 01 difference => http://stackoverflow.com/questions/23516805/h264-nal-unit-prefixes
	
		switch(_state) {
			case 0:
				if(value == 0x00)
					_state = 1;
				break;
			case 1:
				_state = value == 0x00 ? 2 : 0;
				break;
			case 2:
				if (value == 0x00) {  // 3 zeros
					_state = 3;
					break;
				}
			case 3:
				if(value == 0x00)  // more than 2 or 3 zeros... no problem  
					break; // state stays at 3 (or 2)
				if (value == 0x01) {
					writeNal(nal, cur - nal, source);
					// trim off the [0] 0 0 1
					if(_pNal)
						_pNal->resize(_pNal->size() - _state - 1);
					// try to flush Nal!
					flushNal(source);
				
					nal = cur;
				}
			default:
				_state = 0;
				break;
		} // switch _scState
	}

	if (cur != nal)
		writeNal(nal, cur - nal, source);

	return 0;
}

void H264NALReader::writeNal(const UInt8* data, UInt32 size, Media::Source& source) {
	// here size>0
	if (!_type) {
		// Nal begin!
		_type = *data & 0x1f;
		if (!_type || _type >= 9) {
			_type = 0xFF;
			return; // ignore current NAL!
		}
		if(_pNal && (_type==7 || _type==8)) {
			if ((_pNal->data()[4] & 0x1F) != _type) {
				_pNal->append(data, size);
				return; // wait the other
			}
			// erase previous SPS or PPS
			_pNal->resize(4,false);
		} else
			_pNal.reset(new Buffer(4)); // reserve size for AVC header
		_position = 0;
		static Media::Video::Frame Frames[] = { Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_INTER, Media::Video::FRAME_KEY, Media::Video::FRAME_INFO, Media::Video::FRAME_CONFIG, Media::Video::FRAME_CONFIG };
		_tag.frame = Frames[_type];
		if(_tag.frame!=Media::Video::FRAME_CONFIG) {
			_tag.time = time;
			_tag.compositionOffset = compositionOffset;
		}
	} else {
		if (!_pNal)
			return; // ignore current NAL!
		if ((_pNal->size() + size) > 0xA00000) {
			// Max H264 slice size (0x900000 + 4 + some SEI)
			WARN("H264NALReader buffer exceeds maximum slice size");
			_pNal.reset(); // release huge buffer! (and allow to wait next 0000001)
			return;
		}
	}
	_pNal->append(data, size);
}

void H264NALReader::flushNal(Media::Source& source) {
	if (_pNal) {
		// write AVC header
		BinaryWriter(_pNal->data() + _position, 4).write32(_pNal->size() - _position - 4);

		if (_tag.frame == Media::Video::FRAME_CONFIG) {
			if (!_position) {
				// concat SPS with PPS, so wait the both!
				_position = _pNal->size();
				_pNal->resize(_position + 4); // reserve size for AVC header
				_type = 0;
				return;
			}
			// take the more updated time for config frame!
			_tag.time = time;
			_tag.compositionOffset = compositionOffset;
		}
		source.writeVideo(track, _tag, Packet(_pNal));
	}
	_type = 0;
}

void H264NALReader::onFlush(const Packet& packet, Media::Source& source) {
	flushNal(source);
	_pNal.reset();
	_state = 0;
	MediaTrackReader::onFlush(packet, source);
}



} // namespace Mona
