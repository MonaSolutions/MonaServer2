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


Buffer::Buffer(UInt32 size, const void* data) : _offset(0), _size(size) {
	if (!size) {
		static UInt8 BufferEmpty;
		_capacity = 0;
		_data = _buffer = &BufferEmpty;
		return;
	}
	computeCapacity(size);
	_data = _buffer = _Allocator.load()->allocate(_capacity);
	if (data)
		memcpy(_data,data,size);
}

Buffer::Buffer(void* buffer, UInt32 size) : _offset(0), _data(BIN buffer), _size(size),_capacity(size),_buffer(NULL) {}

Buffer::~Buffer() {
	if (_buffer && (_capacity+=_offset))
		_Allocator.load()->deallocate(_buffer, _capacity);
}

void Buffer::computeCapacity(UInt32 size) {
	_capacity = max(size, 16u) - 1; // at minimum allocate 16 bytes!
	_capacity |= _capacity >> 1;
	_capacity |= _capacity >> 2;
	_capacity |= _capacity >> 4;
	_capacity |= _capacity >> 8;
	_capacity |= _capacity >> 16;
	if (!++_capacity)
		_capacity = size; // to expect the case where size*2 > 0xFFFFFFFF (exceeds maximum size)
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

Buffer& Buffer::resize(UInt32 size, bool preserveData) {
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
				memmove(_data, oldData, _size);
			_size = size;
			return *this;
		}
	}

	if (!_buffer)
		FATAL_ERROR("Static buffer exceeds maximum ",_capacity," bytes capacity");

	// fast compute the closer upper power of two for new capacity
	UInt32 oldCapacity(_capacity);
	computeCapacity(size);

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
