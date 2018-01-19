/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Binary.h"

namespace Mona {


struct BitReader : Binary, virtual Object {

	BitReader(const UInt8* data, UInt32 size) : _bit(0), _current(data), _end(data+size), _data(data), _size(size) {}

	virtual bool read();

	template<typename ResultType>
	ResultType read(UInt8 count = (sizeof(ResultType) * 8)) {
		ResultType result(0);
		while (_current != _end && count--) {
			result <<= 1;
			if (!read())
				continue;
			if (count >= (sizeof(ResultType) * 8))
				return std::numeric_limits<ResultType>::max(); // max reachs!
			result |= 1;
		}
		return result;
	}

	UInt64	position() const { return (_current-_data)*8 + _bit; }
	virtual UInt64	next(UInt64 count = 1);
	void	reset(UInt64 position = 0);
	UInt64	shrink(UInt64 available);

	UInt64	available() const { return (_end -_current)*8 - _bit; }

	// beware, data() can be null
	const UInt8*	data() const { return _data; }
	UInt32			size() const { return _size; }

	
	static BitReader Null;
protected:

	const UInt8*	_data;
	const UInt8*	_end;
	const UInt8*	_current;
	UInt32			_size;
	UInt8			_bit;
};


} // namespace Mona
