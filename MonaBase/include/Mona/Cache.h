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

namespace Mona {


struct Cache : virtual Object {
	NULLABLE

	struct Memory : virtual Object {
		Memory(UInt32 capacity = 0x1FFFFFFF) : _capacity(capacity), _size(0), _pCacheFront(NULL), _pCacheBack(NULL) {}
		UInt32 size() const {  return _size; }
		UInt32 capacity() const { return _capacity; }
	private:
		Cache& add(Cache& cache, shared<Buffer>& pBuffer);
		Cache& remove(Cache& cache);

		Cache*	_pCacheFront;
		Cache*	_pCacheBack;
		UInt32	_size;
		UInt32	_capacity;
		friend struct Cache;
	};


	Cache(Cache::Memory& memory) : memory(memory), _pNext(NULL), _pPrev(NULL) {}
	~Cache() { reset(); }

	Cache::Memory& memory;

	operator bool() const { return _pBuffer.operator bool(); }
	operator const shared<const Buffer>&() const { return _pBuffer; }
	const Buffer* operator->() const { return _pBuffer.get(); }
	const Buffer& operator*() const { return *_pBuffer; }

	Cache& operator=(shared<Buffer>& pBuffer) { return reset(pBuffer); }
	Cache& operator=(std::nullptr_t) { return reset(); }
	
	Cache& reset(shared<Buffer>& pBuffer) { return memory.add(*this, pBuffer); }
	Cache& reset() { return memory.remove(*this); }
	
private:
	shared<const Buffer>	_pBuffer;
	Cache*					_pPrev;
	Cache*					_pNext;
	friend struct Memory;
};


} // namespace Mona
