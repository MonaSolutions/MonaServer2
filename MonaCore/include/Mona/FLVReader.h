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
	FLVReader() : _begin(true),_size(0),_type(AMF::TYPE_EMPTY), _syncError(false) {}

	/*!
	Read fmash media header, beware you have to use previous Media::Audio/Video::Tag on every call */
	static UInt8  ReadMediaHeader(const UInt8* data, UInt32 size, Media::Audio::Tag& tag, Media::Audio::Config& config);
	static UInt8  ReadMediaHeader(const UInt8* data, UInt32 size, Media::Video::Tag& tag);
private:
	UInt32	parse(Packet& buffer, Media::Source& source);

	void	onFlush(Packet& buffer, Media::Source& source);

	// Read/Parse state
	bool				_begin;
	UInt32				_size;
	AMF::Type			_type;
	bool				_syncError;

	// Save audio config to get a correct and exact rate and channels informations
	Media::Video::Tag	 _video;
	Media::Audio::Tag	 _audio;
	Media::Audio::Config _audioConfig;
};


} // namespace Mona
