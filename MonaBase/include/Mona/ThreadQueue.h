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
#include "Mona/Thread.h"
#include "Mona/Runner.h"
#include <deque>

namespace Mona {

struct ThreadQueue : Thread, virtual Object {
	ThreadQueue(const char* name) : Thread(name) {}
	virtual ~ThreadQueue() { stop(); }

	static ThreadQueue*	Current() { return _PCurrent; }

	template<typename RunnerType>
	bool queue(Exception& ex, const shared<RunnerType>& pRunner) {
		std::lock_guard<std::mutex> lock(_mutex);
		if (!start(ex))
			return false;
		_runners.emplace_back(pRunner);
		wakeUp.set();
		return true;
	}

private:
	bool run(Exception& ex, const volatile bool& requestStop);

	std::deque<shared<Runner>>			_runners;
	std::mutex							_mutex;
	static thread_local ThreadQueue*	_PCurrent;
};


} // namespace Mona
