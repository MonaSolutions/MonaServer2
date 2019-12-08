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

atomic_flag Buffer::Allocator::_Mutex = ATOMIC_FLAG_INIT;
unique<Buffer::Allocator>	Buffer::Allocator::_PAllocator(SET);

UInt32 Buffer::Allocator::ComputeCapacity(UInt32 size) {
	if (size <= 16) // at minimum allocate 16 bytes!
		return 16;
	// fast compute the closer upper power of two for new capacity
	UInt32 capacity = size - 1; // at minimum allocate 16 bytes!
	capacity |= capacity >> 1;
	capacity |= capacity >> 2;
	capacity |= capacity >> 4;
	capacity |= capacity >> 8;
	capacity |= capacity >> 16;
	return ++capacity ? capacity : size;  // if =0 exceeds UInt32
}
UInt8* Buffer::Allocator::Alloc(UInt32& size) {
	size = ComputeCapacity(size);
	if (size>0x80000000 || !TryLock())
		return new UInt8[size];
	UInt8* buffer = _PAllocator->alloc(size);
	Unlock();
	return buffer;
}
void Buffer::Allocator::Free(UInt8* buffer, UInt32 size) {
	if ((!size || (size & (size - 1))) || !TryLock())  // check than we have a size create with Alloc (capacity log2) + TryLock working!
		return delete[] buffer;
	_PAllocator->free(buffer, size);
	Unlock();
}

static UInt8 _Empty;

Buffer::Buffer(UInt32 size) : _offset(0), _size(size), _capacity(size) {
	_data = _buffer = (size ? Allocator::Alloc(_capacity) : &_Empty);
}
Buffer::Buffer(const void* data, UInt32 size) : _offset(0), _size(size), _capacity(size) {
	memcpy(_data = _buffer = (size ? Allocator::Alloc(_capacity) : &_Empty), data, size);
}

Buffer::Buffer(UInt32 size, void* buffer) : _offset(0), _data(BIN buffer), _size(size),_capacity(size), _buffer(NULL) {}

Buffer::~Buffer() {
	if (_buffer && (_capacity+=_offset))
		Allocator::Free(_buffer, _capacity);
}

Buffer& Buffer::append(const void* data, UInt32 size) {
	if (!_data) // to expect null Buffer 
		return self;
	UInt32 oldSize(_size);
	resize(_size + size);
	memcpy(_data + oldSize, data, size);
	return self;
}
Buffer& Buffer::append(UInt32 count, UInt8 value) {
	if (!_data) // to expect null Buffer 
		return self;
	UInt32 oldSize(_size);
	resize(_size + count);
	memset(_data + oldSize, value, count);
	return self;
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

	// allocate
	UInt32 oldCapacity(_capacity);
	_data = Allocator::Alloc(_capacity=size);

	 // copy data
	if (preserveData)
		memcpy(_data, oldData, _size);


	// deallocate if was allocated
	if (oldCapacity)
		Allocator::Free(_buffer, oldCapacity);

	_size = size;
	_buffer=_data;
	return *this;
}



} // namespace Mona
