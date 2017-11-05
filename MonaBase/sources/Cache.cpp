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

Cache& Cache::Memory::add(Cache& cache, shared<Buffer>& pBuffer) {
	// remove in first
	if (cache)
		remove(cache);

	if (pBuffer->size() > _capacity) {
		// no cache possible!
		pBuffer.reset();
		return cache;
	}

	// add
	_size += pBuffer->size();
	cache._pBuffer = move(pBuffer);
	cache._pPrev = _pCacheBack;
	_pCacheBack = cache._pPrev->_pNext = &cache;

	// remove exceeding
	while (_size > _capacity) {
		_size -= _pCacheFront->_pBuffer->size();
		_pCacheFront->_pBuffer.reset();
		_pCacheFront = _pCacheFront->_pNext;
		_pCacheFront->_pPrev = NULL;
	}
	return cache;
}

Cache& Cache::Memory::remove(Cache& cache) {
	if (!cache)
		return cache;

	_size -= cache._pBuffer->size();
	cache._pBuffer.reset();

	if (cache._pNext) {
		if (!cache._pPrev) {
			// is the first
			_pCacheFront = cache._pNext; 
			_pCacheFront->_pPrev = NULL;
		} else // is in the middle
			cache._pNext->_pPrev = cache._pPrev->_pNext; 
	} else if (cache._pPrev) {
		// is the last
		_pCacheBack = cache._pPrev;
		_pCacheBack->_pNext = NULL;
	} else // is alone
		_pCacheFront = _pCacheBack = NULL;
	return cache;
}

} // namespace Mona
