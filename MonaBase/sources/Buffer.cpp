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

#include "Mona/Buffer.h"
#include "Mona/Exceptions.h"

using namespace std;

namespace Mona {

atomic<Allocator*>	Buffer::_Allocator(&Allocator::Default());


Buffer::Buffer(UInt32 size, const void* data) : _offset(0), _size(size), _capacity(size) {
	if (!size) {
		static UInt8 BufferEmpty;
		_data = _buffer = &BufferEmpty;
		return;
	}
	_data = _buffer = _Allocator.load()->allocate(_capacity);
	if (data)
		memcpy(_data,data,size);
}

Buffer::Buffer(void* buffer, UInt32 size) : _offset(0), _data(BIN buffer), _size(size),_capacity(size),_buffer(NULL) {}

Buffer::~Buffer() {
	if (_buffer && (_capacity+=_offset))
		_Allocator.load()->deallocate(_buffer, _capacity);
}

Buffer& Buffer::append(const void* data, UInt32 size) {
	if (!_data) // to expect null Buffer 
		return *this;
	UInt32 oldSize(_size);
	resize(_size + size);
	memcpy(_data + oldSize, data, size);
	return *this;
}

Buffer& Buffer::clip(UInt32 offset) {
	if (offset >= _size)
		return clear();
	if (offset) {
		_offset += offset;
		_data += offset;
		_size -= offset;
		_capacity -= offset;
	}
	return *this;
}

Buffer& Buffer::resize(UInt32 size,bool preserveData) {
	if (size <= _capacity) {
		if (_offset && !preserveData) {
			// fix possible clip
			_capacity += _offset;
			_data -= _offset;
			_offset = 0;
		}
		_size = size;
		return *this;
	}

	// here size > capacity, so size > _size

	UInt8* oldData(_data);

	// try without offset
	if (_offset) {
		_capacity += _offset;
		_data -= _offset;
		_offset = 0;
		if (size <= _capacity) {
			if (preserveData)
				memmove(_data,oldData,_size);
			_size = size;
			return *this;
		}
	}

	if (!_buffer)
		FATAL_ERROR("Static buffer exceeds maximum ",_capacity," bytes capacity");

	UInt32 oldCapacity(_capacity);
	if (!_capacity)
		_capacity = 32;
	while (size > _capacity) {
		if (_capacity >= 0x80000000) { // to expect the case where _capacity*2 > 0xFFFFFFFF (exceeds maximum size)
			_capacity = size;
			break;
		}
		_capacity *= 2;	
	}

	// allocate
	_data = _Allocator.load()->allocate(_capacity);

	 // copy data
	if (preserveData)
		memcpy(_data, oldData, _size);


	// deallocate if was allocated
	if (oldCapacity)
		_Allocator.load()->deallocate(_buffer, oldCapacity);

	_size = size;
	_buffer=_data;
	return *this;
}



} // namespace Mona
