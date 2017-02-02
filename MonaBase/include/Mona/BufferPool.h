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
#include "Mona/Timer.h"
#include <mutex>

namespace Mona {

class BufferPool : public Allocator, public virtual Object {
public:
	BufferPool(const Timer&	timer);
	~BufferPool();

	UInt32 available() const { return _buffers.size(); }
	void   clear();

	UInt8* allocate(UInt32& size) const;
	void   deallocate(UInt8* buffer, UInt32 size) const;

private:
	mutable std::multimap<UInt32,UInt8*>	_buffers;
	mutable std::mutex						_mutex;
	const Timer&							_timer;
	Timer::OnTimer							_onTimer;
	mutable UInt32							_minCount;
	mutable UInt32							_maxSize;
};


} // namespace Mona
