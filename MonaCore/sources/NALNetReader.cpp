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

#include "Mona/NALNetReader.h"
#include "Mona/HEVC.h"
#include "Mona/AVC.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

template <>
NALNetReader<AVC>::NALNetReader(UInt8 track) : MediaTrackReader(track), _tag(Media::Video::CODEC_H264), _state(0), _type(0xFF), _position(0) {}

template <>
NALNetReader<HEVC>::NALNetReader(UInt8 track) : MediaTrackReader(track), _tag(Media::Video::CODEC_HEVC), _state(0), _type(0xFF), _position(0) {}

template <class VideoType>
UInt32 NALNetReader<VideoType>::parse(Packet& buffer, Media::Source& source) {

	const UInt8* cur(buffer.data());
	const UInt8* end(buffer.data() + buffer.size());
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

template <class VideoType>
void NALNetReader<VideoType>::writeNal(const UInt8* data, UInt32 size, Media::Source& source, bool eon) {
	// flush just if:
	// - config packet complete (VPS, SPS & PPS)
	// - unit delimiter or nal type greater
	// - times change
	// /!\ Ignore many VLC frames are in the same NAL unit, it can be redundant coded picture
	bool flush = false;
	if (_type==0xFF) {
		// Nal begin!
		_type = VideoType::NalType(*data);
		if (_type >= VideoType::NAL_AUD)
			return flushNal(source); // flush possible NAL waiting and ignore current NAL (_pNal is reseted)
		
		if (_tag.frame == Media::Video::FRAME_CONFIG) {
			UInt8 prevType = VideoType::NalType(_pNal->data()[_position + 4]); // _pNal can't be null here when _tag.frame is initialized
			if (_type == prevType) {
				_pNal.reset();  // erase repeated config type and wait the other config type!
			}
			else if (VideoType::Frames[_type] != Media::Video::FRAME_CONFIG) {
				if (prevType == VideoType::NAL_SPS)
					flushNal(source); // flush alone SPS config (valid)
				else
					_pNal.reset();  // erase alone VPS or PPS config (invalid)
				_tag.frame = VideoType::UpdateFrame(_type);
			}
			else
				flush = true;
		} else {
			if (VideoType::Frames[_type] == Media::Video::FRAME_CONFIG)
				flushNal(source); // flush everything and wait the other config type
			else if (_tag.time != time || _tag.compositionOffset != compositionOffset)
				flushNal(source); // flush if time change
			_tag.frame = VideoType::UpdateFrame(_type, _tag.frame);
		}
		if (_pNal) { // append to current NAL
			// write NAL size
			BinaryWriter(_pNal->data() + _position, 4).write32(_pNal->size() - _position - 4); 
			// reserve size for AVC header
			_pNal->resize((_position=_pNal->size()) + 4, true);
		} else {
			_tag.time = time;
			_tag.compositionOffset = compositionOffset;
			_position = 0;
			_pNal.set(4);
		}
	} else {
		if (!_pNal)
			return; // ignore current NAL!
		if ((_pNal->size() + size) > 0xA00000) {
			// Max slice size (0x900000 + 4 + some SEI)
			WARN("NALNetReader buffer exceeds maximum slice size");
			_tag.frame = Media::Video::FRAME_UNSPECIFIED;
			_pNal.reset(); // release huge buffer! (and allow to wait next 0000001)
			return;
		}
	}
	_pNal->append(data, size);
	if (eon)
		_pNal->resize(_pNal->size() - _state - 1, true); // trim off the [0] 0 0 1
	if(flush)
		flushNal(source);
}

template <class VideoType>
void NALNetReader<VideoType>::flushNal(Media::Source& source) {
	if (!_pNal)
		return;
	// write NAL size
	BinaryWriter(_pNal->data() + _position, 4).write32(_pNal->size() - _position - 4);
	// transfer _pNal and reset _tag.frame
	Packet nal(_pNal);
	if (_tag.frame == Media::Video::FRAME_CONFIG || nal.size() > 4) // empty NAL valid just for CONFIG frame (useless for INTER, KEY, INFOS, etc...)
		source.writeVideo(_tag, nal, track);
	_tag.frame = Media::Video::FRAME_UNSPECIFIED;
}

template <class VideoType>
void NALNetReader<VideoType>::onFlush(Packet& buffer, Media::Source& source) {
	flushNal(source);
	_type = 0xFF;
	_state = 0;
	MediaTrackReader::onFlush(buffer, source);
}



} // namespace Mona
