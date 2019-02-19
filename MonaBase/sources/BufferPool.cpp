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

#include "Mona/BufferPool.h"


using namespace std;


namespace Mona {

UInt8* BufferPool::Buffers::pop() {
	if (empty())
		return NULL;
	UInt8* buffer = back();
	pop_back();
	if (size() < _minSize)
		_minSize = size();
	return buffer;
}
void BufferPool::Buffers::push(UInt8* buffer) {
	emplace_back(buffer);
	if (size() > _maxSize)
		_maxSize = size();
}
void BufferPool::Buffers::manage(vector<UInt8*>& gc) {
	// pickUp
	UInt32 position = gc.size();
	gc.resize(position + _minSize);
	memcpy(gc.data() + position, data() + size() - _minSize, _minSize * sizeof(UInt8*));
	// reserve max capacity + remove erasing buffer + reset _minSize/_maxSize
	_minSize = size() - _minSize;
	resize(_maxSize);
	shrink_to_fit();
	resize(_minSize);
	_maxSize = 0;
}

bool BufferPool::run(Exception& ex, const volatile bool& requestStop) {
	UInt16 timeout = 10000;
	while (!requestStop) {
		if (timeout && wakeUp.wait(timeout)) // wait()==true means requestStop=true because there is no other wakeUp.set elsewhere
			return true;
		Time time;
		for (Buffers& buffers : _buffers) {
			vector<UInt8*> gc;
			while (!tryLock())
				this_thread::yield();
			// garbage collector!
			buffers.manage(gc);
			unlock();
			for (UInt8* buffer : gc)
				delete[] buffer;
		}
		timeout = (UInt16)max(10000 - time.elapsed(), 0);
	}
	return true;
}

UInt8 BufferPool::computeIndexAndSize(UInt32& size) {
	UInt32 capacity = ComputeCapacity(size);
	if (!capacity) // if = 0 exceeds UInt32, unchange size wanted!
		return 0;
	size = capacity--;
	// compute index
	capacity = (capacity << 3) - capacity;    // Multiply by 7.
	capacity = (capacity << 8) - capacity;    // Multiply by 255.
	capacity = (capacity << 8) - capacity;    // Again.
	capacity = (capacity << 8) - capacity;    // Again.
	static UInt8 table[64] = { 100, 101, 99, 12, 99, 102, 25, 99, 13, 99, 99, 99, 103, 18, 26, 99,
		99, 99, 16, 14, 7, 99, 9, 99, 99, 0, 99, 3, 99, 19, 27, 99,
		11, 99, 24, 99, 99, 99, 17, 99, 15, 6, 8, 99, 2, 99, 99, 10,
		23, 99, 99, 5, 99, 1, 99, 22, 99, 4, 21, 99, 20, 99, 28, 99 };
	return table[capacity >> 26];
}

UInt8* BufferPool::allocate(UInt32& size) {
	Buffers& buffers(_buffers[computeIndexAndSize(size)]);
	if (!size || !tryLock()) // if superior to max (size=0) or impossible to fast lock => explicit new
		return new UInt8[size];
	UInt8* buffer = buffers.pop();
	if (!buffer)
		buffer = new UInt8[size];
	unlock();
	return buffer;
}
void BufferPool::deallocate(UInt8* buffer, UInt32 size) {
	UInt32 capacity = size;
	UInt8 index = computeIndexAndSize(capacity);
	if (!capacity || (capacity != size && !index--))
		return delete[] buffer; // don't keep it => superior to 0x80000000 or inferior to 16
	Buffers& buffers(_buffers[index]);
	if (!tryLock()) // impossible to fast lock, instant deletion
		return delete[] buffer;
	buffers.push(buffer);
	unlock();
}


} // namespace Mona
