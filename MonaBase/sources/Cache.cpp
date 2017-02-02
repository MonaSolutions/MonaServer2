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

#include "Mona/Cache.h"

using namespace std;

namespace Mona {

void Cache::add(const shared<const Buffer>& pBuffer) {
	auto& it = _buffers.emplace(pBuffer.get(), pBuffer);
	BufferPtr* pBufferFront(_pBufferFront);
	_pBufferFront = &it.first->second;
	if(pBufferFront && pBufferFront != _pBufferFront) {
		// repair list chained
		_pBufferFront->pPrev->pNext = _pBufferFront->pNext;
		if (_pBufferFront == _pBufferBack)
			_pBufferBack = _pBufferFront->pPrev;
		else
			_pBufferFront->pNext->pPrev = _pBufferFront->pPrev;
		// new front
		pBufferFront->pPrev = _pBufferFront;
		_pBufferFront->pNext = pBufferFront;
		_pBufferFront->pPrev = NULL;
	}
	if (!it.second)
		return; // no addition
	_size += pBuffer->size(); // new addition
	if (!_pBufferBack) {
		_pBufferBack = _pBufferFront;
		return; // contains just the new element
	}
	// erase back if exceeds size
	while (_size > _capacity) {
		pBufferFront = _pBufferBack;
		_pBufferBack = _pBufferBack->pPrev;
		_pBufferBack->pNext = NULL;
		_size -= pBufferFront->pBuffer->size();
		_buffers.erase(pBufferFront->pBuffer.get());
	}
}

void Cache::remove(const shared<const Buffer>& pBuffer) {
	auto& it = _buffers.find(pBuffer.get());
	if (it == _buffers.end())
		return;
	BufferPtr* pBufferPtr(&it->second);
	// repair list chained
	if (pBufferPtr == _pBufferFront)
		_pBufferFront = pBufferPtr->pNext;
	else
		pBufferPtr->pPrev->pNext = pBufferPtr->pNext;
	if (pBufferPtr == _pBufferBack)
		_pBufferBack = pBufferPtr->pPrev;
	else
		pBufferPtr->pNext->pPrev = pBufferPtr->pPrev;
	// remove
	_size -= pBufferPtr->pBuffer->size();
	_buffers.erase(it);
}


} // namespace Mona
