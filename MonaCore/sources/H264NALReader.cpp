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
#include "Mona/MPEG4.h"
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
					writeNal(nal, cur - nal, source, true);
					nal = cur;
					_type = 0xFF; // new NAL!	
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

void H264NALReader::writeNal(const UInt8* data, UInt32 size, Media::Source& source, bool eon) {
	// flush just if:
	// - config packet complete (7 & 8)
	// - unit delimiter (9)
	// - times change
	// - three 00 prefix (_state = 3) excepting for config packet!
	bool flush = eon && _state > 2;
	if (_type==0xFF) {
		// Nal begin!
		_type = *data & 0x1f;
		if (_type > 8)
			return flushNal(source); // flush possible NAL waiting and ignore current NAL (_pNal is reseted)
		
		if (_tag.frame == Media::Video::FRAME_CONFIG) {
			UInt8 prevType = (_pNal->data()[4] & 0x1F);
			if (_type == prevType) {
				_pNal.reset();  // erase repeated config type!
				flush = false; // wait the other config type!
			} else if (_type != 7 && _type != 8) {
				if (prevType == 7)
					flushNal(source); // flush alone 7 config (valid)
				else
					_pNal.reset();  // erase alone 8 config (invalid)
				_tag.frame = MPEG4::UpdateFrame(_type);
			} else
				flush = true;
		} else {
			_tag.frame = MPEG4::UpdateFrame(_type, _tag.frame);
			if (_tag.frame == Media::Video::FRAME_CONFIG) {
				flushNal(source); // flush if time change
				flush = false; // wait the other config type!
			} else if (_tag.time != time || _tag.compositionOffset != compositionOffset)
				flushNal(source); // flush if time change
		}
		if (_pNal) { // append to current NAL
			// write NAL size
			BinaryWriter(_pNal->data() + _position, 4).write32(_pNal->size() - _position - 4); 
			// reserve size for AVC header
			_position = _pNal->size();
			_pNal->resize(_position + 4, true); 
		} else {
			_tag.time = time;
			_tag.compositionOffset = compositionOffset;
			_position = 0;
			_pNal.reset(new Buffer(4));
		}
	} else {
		if (!_pNal)
			return; // ignore current NAL!
		if ((_pNal->size() + size) > 0xA00000) {
			// Max H264 slice size (0x900000 + 4 + some SEI)
			WARN("H264NALReader buffer exceeds maximum slice size");
			return _pNal.reset(); // release huge buffer! (and allow to wait next 0000001)
		}
	}
	_pNal->append(data, size);
	if (eon)
		_pNal->resize(_pNal->size() - _state - 1, true); // trim off the [0] 0 0 1
	if(flush)
		flushNal(source);
}

void H264NALReader::flushNal(Media::Source& source) {
	if (!_pNal)
		return;
	// write NAL size
	BinaryWriter(_pNal->data() + _position, 4).write32(_pNal->size() - _position - 4);
	// transfer _pNal and reset _tag.frame
	Packet nal(_pNal);
	if (_tag.frame == Media::Video::FRAME_CONFIG || nal.size() > 4) // empty NAL valid just for CONFIG frame (useless for INTER, KEY, INFOS, etc...)
		source.writeVideo(track, _tag, nal);
	_tag.frame = Media::Video::FRAME_UNSPECIFIED;
}

void H264NALReader::onFlush(const Packet& packet, Media::Source& source) {
	flushNal(source);
	_type = 0xFF;
	_state = 0;
	MediaTrackReader::onFlush(packet, source);
}



} // namespace Mona
