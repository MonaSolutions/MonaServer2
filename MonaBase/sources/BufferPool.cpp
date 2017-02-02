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


BufferPool::BufferPool(const Timer&	timer) : _timer(timer), _minCount(0), _maxSize(0),
	_onTimer([this](UInt32)->UInt32 {
		// remove bigger buffers
		lock_guard<mutex> lock(_mutex);
		if (_minCount > 100) // limit suppression, more than 100 it can take signifiant time!
			_minCount = 100;
		++_minCount;

		// remove useless bigger buffer!
		while (--_minCount) {
			auto it(_buffers.end());
			if ((--it)->first <= _maxSize)
				break;
			delete[] it->second;
			_buffers.erase(it);
		}
		// remove smaller buffer!
		auto it = _buffers.begin();
		while (_minCount--) {
			delete[] it->second;
			_buffers.erase(it++);
		}
		
		_minCount = _buffers.size();
		_maxSize = 0;
		return 10000;
	}) {
	_timer.set(_onTimer, 10000);

}

BufferPool::~BufferPool() {
	_timer.set(_onTimer, 0);
	clear();
}

void BufferPool::clear() {
	lock_guard<mutex> lock(_mutex);
	for(const auto& it : _buffers)
		delete [] it.second;
	_buffers.clear();
}


UInt8* BufferPool::allocate(UInt32& size) const {
	// choose the bigger buffer 
	lock_guard<mutex> lock(_mutex);
	if (size>_maxSize)
		_maxSize = size;

	if (_buffers.empty()) {
		_minCount = 0;
		return new UInt8[size]();
	}
	// at less one available here
	auto itBigger(_buffers.end());
	--itBigger;
	if (size > itBigger->first)
		return new UInt8[size]();

	size = itBigger->first;
	UInt8* buffer(itBigger->second);
	_buffers.erase(itBigger);
	if (_buffers.size() < _minCount)
		_minCount = _buffers.size();
	return buffer;
}

void BufferPool::deallocate(UInt8* buffer, UInt32 size) const {
	lock_guard<mutex> lock(_mutex);
	_buffers.emplace(size,buffer);
}



} // namespace Mona
