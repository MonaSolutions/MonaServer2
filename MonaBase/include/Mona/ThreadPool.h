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
	/*!
	If threads==0 use ProcessorCount*2 because gives the better result */
	ThreadPool(UInt16 threads = 0) : _current(0), _threads(threads ? threads : Thread::ProcessorCount()*2) { _size = UInt16(_threads.size()); }

	UInt16	threads() const { return _size; }

	void	join() { for (Thread& thread : _threads) thread.stop(); }


	template<typename RunnerType>
	bool  queue(Exception& ex, const shared<RunnerType>& pRunner) const { return _threads[_current++%_size].queue(ex, pRunner); }

	template<typename RunnerType>
	bool  queue(Exception& ex, const shared<RunnerType>& pRunner, UInt16& track) const {
		Thread* pThread;
		if (track > _size) {
			ex.set<Ex::Intern>("Thread track out of ThreadPool bounds");
			pThread = &_threads[_current++%_size];
		} else if (!track) {
			pThread = &_threads[track = (_current++%_size)];
			++track;
		} else
			pThread = &_threads[track - 1];
		return pThread->queue(ex, pRunner);
	}

private:
	struct Thread : virtual Object, ThreadQueue { Thread() : ThreadQueue("ThreadPool") {} };

	mutable std::vector<Thread>	_threads;
	mutable std::atomic<UInt16>	_current;
	UInt16						_size;
};


} // namespace Mona
