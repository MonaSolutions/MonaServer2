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
#include <map>

namespace Mona {

struct Cache : virtual Object {
	Cache(UInt32 capacity = 0x1FFFFFFF) : _capacity(capacity), _pBufferBack(NULL), _pBufferFront(NULL), _size(0) {}
	/*!
	Use a weak<const Buffer> is if expired add to cache */
	void add(const shared<const Buffer>& pBuffer);
	void remove(const shared<const Buffer>& pBuffer);
private:
	struct BufferPtr {
		BufferPtr(const shared<const Buffer>& pBuffer) : pNext(NULL), pPrev(NULL), pBuffer(pBuffer) {}
		shared<const Buffer> pBuffer;
		BufferPtr*			 pPrev;
		BufferPtr*			 pNext;
	};
	BufferPtr*							_pBufferFront;
	BufferPtr*							_pBufferBack;
	std::map<const Buffer*, BufferPtr>	_buffers;
	UInt32								_size;
	UInt32								_capacity;
};


} // namespace Mona
