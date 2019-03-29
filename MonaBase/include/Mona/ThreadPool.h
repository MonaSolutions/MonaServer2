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
#include "Mona/ThreadQueue.h"
#include <vector>

namespace Mona {

struct ThreadPool : virtual Object {
	ThreadPool(UInt16 threads = 0) : _current(0) { init(threads); }
	ThreadPool(Thread::Priority priority, UInt16 threads = 0) : _current(0) { init(threads, priority); }

	UInt16	threads() const { return _size; }

	UInt16	join();

	template<typename RunnerType>
	void queue(UInt16& thread, RunnerType&& pRunner) const {
		if (thread)
			return _threads[thread - 1]->queue(std::forward<RunnerType>(pRunner));
		_threads[thread = (_current++%_size)]->queue(std::forward<RunnerType>(pRunner));
		++thread;
	}
	template<typename RunnerType>
	void queue(std::nullptr_t, RunnerType&& pRunner) const { UInt16 thread(0); queue<RunnerType>(thread, std::forward<RunnerType>(pRunner)); }
	template <typename RunnerType, typename ...Args>
	void queue(UInt16& thread, Args&&... args) const { queue(thread, std::make_shared<RunnerType>(std::forward<Args>(args)...)); }
	template <typename RunnerType, typename ...Args>
	void queue(std::nullptr_t, Args&&... args) const { UInt16 thread(0); queue<RunnerType>(thread, std::forward<Args>(args)...); }
private:
	void init(UInt16 threads, Thread::Priority priority = Thread::PRIORITY_NORMAL);

	mutable std::vector<unique<ThreadQueue>>	_threads;
	mutable std::atomic<UInt16>					_current;
	UInt16										_size;
};


} // namespace Mona
