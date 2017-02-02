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

/*!
AUDIO 	  [UInt8=>size header PAIR>4][tag header + track(UInt16=>optional)][UInt32=>size][...data...]
VIDEO	  [UInt8=>size header IMPAIR>4][tag header + track(UInt16=>optional)][UInt32=>size][...data...]
DATA 	  [UInt8=>size header<4][UInt8=>type + track(UInt16=>optional)][UInt32=>size][...data...] */
class MonaReader : public virtual Object, public MediaReader {

public:
	MonaReader() : _type(Media::TYPE_NONE), _size(0), _track(0), _syncError(false) {}

private:
	UInt32  parse(const Packet& packet, Media::Source& source);
	void	onFlush(const Packet& packet, Media::Source& source);

	Media::Type		  _type;
	UInt32			  _size;
	UInt16			  _track;
	bool			  _syncError;

	Media::Video::Tag _video;
	Media::Audio::Tag _audio;
	Media::Data::Type _data;
};


} // namespace Mona
