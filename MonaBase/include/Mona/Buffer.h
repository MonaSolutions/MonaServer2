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
#include "Mona/Allocator.h"
#include <atomic>

namespace Mona {

#define BUFFER_RESET(BUFFER,SIZE) (BUFFER ? BUFFER->resize(SIZE, false) : (BUFFER.reset(new Buffer(SIZE)), *BUFFER))

struct Buffer : Binary, virtual Object {
	/*!
	Buffer dynamic with clear/resize/data/size method (like std) */
	Buffer(UInt32 size = 0, const void* data = NULL);

	virtual ~Buffer();


	Buffer&			clip(UInt32 offset);
	Buffer&			append(const void* data, UInt32 size);
	Buffer&			append(UInt32 count, UInt8 value);
	Buffer&			resize(UInt32 size, bool preserveContent = true);
	Buffer&			clear() { resize(0, false); return *this; }

	bool			empty() { return !size(); }

	UInt8*			data() { return _data; } // beware, data() can be null
	const UInt8*	data() const { return _data; }
	UInt32			size() const { return _size; }

	UInt32			capacity() const { return _capacity; }

	template<typename AllocatorType=Allocator>
	static void SetAllocator(AllocatorType& allocator=Allocator::Default()) { _Allocator = &allocator; }

	static Buffer&   Null() { static Buffer Null(nullptr, 0); return Null; } // usefull for Writer Serializer for example (and can't be encapsulate in a shared<Buffer>)

private:
	Buffer(void* buffer, UInt32 size);

	UInt32				_offset;
	UInt8*				_data;
	UInt32				_size;
	UInt32				_capacity;
	UInt8*				_buffer;

	static std::atomic<Allocator*>	_Allocator;

	friend struct BinaryWriter;
};




} // namespace Mona
