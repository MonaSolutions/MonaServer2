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
#include "Mona/Allocator.h"
#include "Mona/Thread.h"
#include <atomic>

namespace Mona {

struct BufferPool : Allocator, private Thread, virtual Object {

	BufferPool() : Thread("BufferPool") { start(Thread::PRIORITY_LOWEST); }
	~BufferPool() { stop(); }

	UInt8* allocate(UInt32& size);
	void   deallocate(UInt8* buffer, UInt32 size);

private:
	bool run(Exception& ex, const volatile bool& requestStop);
	UInt8 computeIndexAndSize(UInt32& size);
	bool tryLock() { return !_mutex.test_and_set(std::memory_order_acquire); }
	void unlock() { _mutex.clear(std::memory_order_release); }

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
	std::atomic_flag _mutex = ATOMIC_FLAG_INIT;
};


} // namespace Mona
