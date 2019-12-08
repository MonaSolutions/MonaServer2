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
#include <thread>
#include <atomic>

namespace Mona {

#define BUFFER_RESET(BUFFER,SIZE) (BUFFER ? BUFFER->resize(SIZE, false) : BUFFER.set(SIZE))

/*!
Buffer dynamic with clear/resize/data/size method (like std) */
struct Buffer : Binary, virtual Object {
	Buffer(UInt32 size = 0);
	Buffer(const void* data, UInt32 size);

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

	static Buffer&   Null() { static Buffer Null(0, nullptr); return Null; } // usefull for Writer Serializer for example (and can't be encapsulate in a shared<Buffer>)


	struct Allocator : virtual Object {
		template<typename AllocatorType=Allocator, typename ...Args>
		static void   Set(Args&&... args) { Lock(); _PAllocator.set<AllocatorType>(std::forward<Args>(args)...); Unlock(); }
		static UInt8* Alloc(UInt32& size);
		static void	  Free(UInt8* buffer, UInt32 size);
		static UInt32 ComputeCapacity(UInt32 size);
	protected:
		virtual UInt8* alloc(UInt32& capacity) { return new UInt8[capacity]; }
		virtual void   free(UInt8* buffer, UInt32 capacity) { delete[] buffer; }

		static void Lock() { while (!TryLock()) std::this_thread::yield(); }
		static void Unlock() { _Mutex.clear(std::memory_order_release); }
	private:
		static bool TryLock() { return !_Mutex.test_and_set(std::memory_order_acquire); }

		static std::atomic_flag  _Mutex;
		static unique<Allocator> _PAllocator;
	};
private:
	Buffer(UInt32 size, void* buffer);

	UInt32				_offset;
	UInt8*				_data;
	UInt32				_size;
	UInt32				_capacity;
	UInt8*				_buffer;


	friend struct BinaryWriter;
};




} // namespace Mona
