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
#include "Mona/Buffer.h"
#include "Mona/Thread.h"

namespace Mona {

struct BufferPool : Buffer::Allocator, private Thread, virtual Object {

	BufferPool() : Thread("BufferPool") { start(Thread::PRIORITY_LOWEST); }
	~BufferPool() { stop(); }

private:
	UInt8* alloc(UInt32& capacity) {
		UInt8* buffer = _buffers[computeIndex(capacity)].pop();
		return buffer ? buffer : new UInt8[capacity];
	}
	void   free(UInt8* buffer, UInt32 capacity) { _buffers[computeIndex(capacity)].push(buffer); }

	bool run(Exception& ex, const volatile bool& requestStop);
	UInt8 computeIndex(UInt32 capacity);

	struct Buffers : private std::vector<UInt8*>, virtual Object {
		Buffers() : _minSize(0), _maxSize(0) {}
		~Buffers() { for (UInt8* buffer : self) delete[] buffer; }
		UInt8* pop();
		void   push(UInt8* buffer);
		void manage(std::vector<UInt8*>& gc);
	private:
		UInt32 _minSize;
		UInt32 _maxSize;
	};
	Buffers			 _buffers[28];
};


} // namespace Mona
