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

#include "Mona/SRTReader.h"

using namespace std;

namespace Mona {

UInt32 SRTReader::parse(Packet& buffer, Media::Source& source) {

	const UInt8* cur = buffer.data() + _pos;
	const UInt8* end = buffer.data() + buffer.size();
	Exception ex;
	Date date;
	bool success;

	for (;;) {
		switch (_stage) {
			case STAGE_START: {
				if (signature) {
					size_t size = strlen(signature);
					if (size_t(end - cur) < size)
						break;
					if (memcmp(cur, signature, size) != 0) {
						if (!_syncError) {
							WARN(typeof(self), " waits ", signature, " signature");
							_syncError = true;
						}
						++cur;
						++buffer;
						continue;
					}
					buffer.set(buffer, cur += size);
				}
				// first time => Subtitle file are always in absolute time, send a time=0 video config!
				source.writeVideo(_config, _config, track);
				_stage = STAGE_NEXT;
				continue;
			}
			case STAGE_NEXT: {
				// remove space
				if (cur == end) 
					return 0; // keep noting!
				if (!isspace(*cur)) {
					_stage = STAGE_SEQ;
					buffer.set(buffer, cur);
				}
				++cur;
				continue;
			}
			case STAGE_SEQ: {
				// pass space
				if (cur == end)
					break;
				if (*cur != '\n') {
					if (!isspace(*cur++))
						++_pos;
					continue;
				}
				++cur;
				UInt64 sequence;
				if (String::ToNumber(STR buffer.data(), _pos+1, sequence)) {
					_stage = STAGE_TIME_BEGIN;
					_pos = 0;
					buffer.set(buffer, cur);
				} else
					WARN("SRT sequence number not found");
				continue;
			}
			case STAGE_TIME_BEGIN: {
				if ((end - cur) < 3)
					break;
				if (memcmp(cur, EXPAND("-->")) == 0) {
					AUTO_WARN(success=date.update(ex, STR buffer.data(), cur - buffer.data(), timeFormat), "SRT time parsing");
					if(success)
						_config.time = date.clock();
					_stage = STAGE_TIME_END;
					_pos = 0;
					buffer.set(buffer, cur += 3);
				} else
					++cur;
				continue;
			}
			case STAGE_TIME_END: {
				if (cur==end)
					break;
				if (*cur != '\n') {
					if (isspace(*cur++)) {
						if (!_pos) // else trim right
							++buffer; // trim left
					} else
						++_pos;
					continue;
				}
				AUTO_WARN(success = date.update(ex, STR buffer.data(), _pos, timeFormat), "SRT time parsing");
				_endTime = success ? date.clock() : (_config.time+1000);
				_stage = STAGE_TEXT;
				_pos = 0;
				buffer.set(buffer, ++cur);
				continue;
			}
			case STAGE_TEXT: {
				UInt32 available = end - cur;
				if (available < 2)
					break;
				UInt8 index = 0;
				if (*cur == '\r') {
					// search \r\n\n or \r\n\r\n
					if (available < 3)
						break; // wait one more byte
					if (cur[++index] != '\n') {
						++cur;
						continue; // was always text, continue!
					}
					/// here we have \r\n
				}
				if (cur[index++] == '\n') {
					// search \n\n or \n\r\n
					if (cur[index] == '\r') {
						/// we have here \n\r
						if (available < (index + 2u))
							break; // wait one more byte
						if (cur[++index] != '\n') {
							/// we have here \n\r!\n, we can skip 2 chars
							cur += index;
							continue; // was always text, continue!
						}
					} else if (cur[index++] != '\n') {
						/// we have here \n#, we can skip 2 chars
						cur += index;
						continue; // was always text, continue!
					}
				} else {
					++cur;
					continue; // was always text, continue!
				}
				// time begin
				source.writeVideo(_config, _config, track);
				// text
				source.writeData(Media::Data::TYPE_TEXT, Packet(buffer, buffer.data(), cur - buffer.data()), track);
				// send one video packet for every second of video to maintain subtitle!
				for (_config.time += 1000; _config.time < _endTime; _config.time += 1000)
					source.writeVideo(_config, _config, track);
				// time end
				_config.time = _endTime;
				source.writeVideo(_config, _config, track);
				_stage = STAGE_NEXT;
				_pos = 0;
				buffer.set(buffer, cur += 2);
				continue;
			}
		}
		_pos = cur - buffer.data();
		return buffer.size(); // keep all!
	}
}


void SRTReader::onFlush(Packet& buffer, Media::Source& source) {
	if (_stage == STAGE_TEXT)
		source.writeData(Media::Data::TYPE_TEXT, buffer, track);
	_stage = STAGE_START;
	_pos = 0;
	_config.time = 0;
	MediaTrackReader::onFlush(buffer, source);
}

} // namespace Mona
