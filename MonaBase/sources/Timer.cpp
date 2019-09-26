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


#include "Mona/Timer.h"


using namespace std;

namespace Mona {

Timer::~Timer() {
	for(const auto& it : _timers) {
		for (const OnTimer* pTimer : *it.second)
			pTimer->_nextRaising = 0;
	}
}

const Timer::OnTimer& Timer::set(const OnTimer& onTimer,  UInt32 timeout) const {
	shared<std::set<const OnTimer*>> pMove;
	if (!remove(onTimer, pMove)) {
		add(onTimer, timeout);
		return onTimer;
	}
	if (timeout) {
		// MOVE (more efficient!)
		++_count;
		const auto& it = _timers.emplace(onTimer._nextRaising = Time::Now() + timeout, move(pMove));
		if (!it.second)
			it.first->second->emplace(&onTimer);
		 else if (!it.first->second)
			 it.first->second.set().emplace(&onTimer);
	}
	return onTimer;
}

void Timer::add(const OnTimer& onTimer,  UInt32 timeout) const {
	if (!timeout)
		return;
	++_count;
	auto& it(_timers[(onTimer._nextRaising=Time::Now() + timeout)]);
	if (!it)
		it.set();
	it->emplace(&onTimer);
}

bool Timer::remove(const OnTimer& onTimer, shared<std::set<const OnTimer*>>& pMove) const {
	if (!onTimer._nextRaising)
		return false;
	const auto& it(_timers.find(onTimer._nextRaising));
	if (it != _timers.end()) {
		onTimer._nextRaising = 0;
		--_count;
		if (it->second->size() == 1) {
			if(*it->second->begin() == &onTimer) {
				pMove = move(it->second);
				_timers.erase(it);
				return true;
			}
		} else if(it->second->erase(&onTimer))
			return true;
	}
	FATAL_ERROR("Timer already used on an other Timer machine, create both individual Timer::Type rather");
	return false;
}

UInt32 Timer::raise() {
	while (!_timers.empty()) {
		const auto& it(_timers.begin());
		Int64 waiting(it->first-Time::Now());
		if (waiting>0)
			return UInt32(waiting); // > 0!
		auto itTimers(move(it->second));
		_timers.erase(it);
		for (const OnTimer* pTimer : *itTimers) {
			pTimer->_nextRaising = 0;
			UInt32 timeout = (*pTimer)(UInt32(-waiting));
			--_count;
			if(timeout)
				add(*pTimer, timeout);
		}
	}
	return 0; //empty!
}


} // namespace Mona
