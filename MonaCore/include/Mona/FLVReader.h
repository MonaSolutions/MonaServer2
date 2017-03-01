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
#include "Mona/AMF.h"

namespace Mona {


struct FLVReader : virtual Object, MediaReader {
	FLVReader() : _begin(true),_size(0),_type(AMF::TYPE_EMPTY),_rate(0),_channels(0), _syncError(false) {}

	static UInt8  ReadMediaHeader(const UInt8* data, UInt32 size, Media::Audio::Tag& tag);
	static UInt8  ReadMediaHeader(const UInt8* data, UInt32 size, Media::Video::Tag& tag);
	static UInt32 ReadAVCConfig(const UInt8* data, UInt32 size, Buffer& buffer);
private:
	UInt32	parse(const Packet& packet, Media::Source& source);

	void	onFlush(const Packet& packet, Media::Source& source);

	// Read/Parse state
	bool				_begin;
	UInt32				_size;
	AMF::Type			_type;
	bool				_syncError;

	// Save audio configto get a correct and exact rate and channels informations
	UInt32				_rate;
	UInt8				_channels;
};


} // namespace Mona
