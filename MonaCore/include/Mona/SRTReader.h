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
#include "Mona/MediaReader.h"

namespace Mona {


struct SRTReader : virtual Object, MediaTrackReader {

	SRTReader(UInt8 track=1) : signature(NULL), timeFormat("%TH:%M:%S,%i"), _stage(STAGE_START), _pos(0), MediaTrackReader(track) {
	}
	
	const char*	timeFormat;
	const char* signature;

protected:
	UInt32	parse(Packet& buffer, Media::Source& source);
	void	onFlush(Packet& buffer, Media::Source& source);

private:
	
	enum Stage {
		STAGE_START,
		STAGE_NEXT,
		STAGE_SEQ,
		STAGE_TIME_BEGIN,
		STAGE_TIME_END,
		STAGE_TEXT
	}						_stage;
	UInt32					_pos;
	Media::Video::Config	_config;
	UInt32					_endTime;
	bool					_syncError;
};


} // namespace Mona
