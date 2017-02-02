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

	const UInt8* nal(packet.data());	// assume that the copy will be from start-of-data
	const UInt8* cur(packet.data());
	const UInt8* end(packet.data()+packet.size());


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
					// write to _pData
					if (writeNal(nal, cur - nal, source)) { // else means ignored frame
						// here 0<_type<9
						// trim off the [0] 0 0 1
						_pNal->resize(_pNal->size() - _state - 1);
						flushNals(source);
					}

					// start Nal reception!
					if (!_pNal)
						_pNal.reset(new Buffer());

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

void H264NALReader::onFlush(const Packet& packet, Media::Source& source) {
	// flush _pData
	if (_pNal) {
		_type = 0; // to force flush
		flushNals(source);
	}

	_pNal.reset();
	_state = 0;
	_position = 0;
	_tag.frame = Media::Video::FRAME_KEY; // to avoid the flush of config packet on _type<5
	TrackReader::onFlush(packet, source);
}

bool H264NALReader::writeNal(const UInt8* data, UInt32 size, Media::Source& source) {
	// here size>0
	if (!_pNal)
		return false;
	
	if (_position==_pNal->size()) {
		// Nal begnning!
		_type = *data&0x1f;
		if (_type && _type < 9) { // else ignore OR first 00 00 00 01 which give _type=0!

			if (_type <= 5) {
				// VCL NAL
				_tag.time = time;
				_tag.compositionOffset = compositionOffset;
				if (_tag.frame==Media::Video::FRAME_CONFIG) {
					// flush if it was a config packet, with the more udpated time
					source.writeVideo(track, _tag, Packet(_pNal));
					_pNal.reset(new Buffer());
					_position = 0;
				}
				_tag.frame = _type==5 ? Media::Video::FRAME_KEY : Media::Video::FRAME_INTER;
			} else if (_type == 7)
				_tag.frame = Media::Video::FRAME_CONFIG;

			_pNal->resize(_pNal->size() + 4);
		}
	}
	if (_pNal->size() > _position) {
		if ((_pNal->size() + size) < 0xA00000) {
			_pNal->append(data, size);
			return true;
		}
		// Max H264 slice size (0x900000 + 4 + some SEI)
		WARN("H264NALReader buffer exceeds maximum slice size");
		_pNal.reset(); // reset huge buffer! (and allow to wait next 0000001)
		_tag.frame = Media::Video::FRAME_KEY; // to avoid the flush of config packet on _type<5
		_position = 0;
	}
	return false;
}


void H264NALReader::flushNals(Media::Source& source) {

	if (_pNal->size() > _position) // Write size
		BinaryWriter(_pNal->data() + _position, 4).write32(_pNal->size() - _position - 4);
	else if (!_pNal->size())
		return; // nothing to flush

	/// End NAL

	// NAL Unit sequence => 
	// (Access Unit Delimiter	 9)
	// (SEI						 >5)
	// Picture					 <=5
	if (_type <= 5) {
		// flush
		source.writeVideo(track, _tag, Packet(_pNal));
		_tag.frame = Media::Video::FRAME_KEY; // to avoid the flush of config packet on _type<5
		_position = 0;
	} else
		_position = _pNal->size();
}


} // namespace Mona
