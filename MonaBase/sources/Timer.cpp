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
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

Timer::~Timer() {
	for(const auto& it : _timers) {
		for (const OnTimer* pTimer : *it.second)
			pTimer->_nextRaising = 0;
	}
}

void Timer::set(const OnTimer& onTimer,  UInt32 timeout) const {
	remove(onTimer);
	add(onTimer, timeout);
}

void Timer::add(const OnTimer& onTimer,  UInt32 timeout) const {
	if (!timeout)
		return;
	++_count;
	auto& it(_timers[(onTimer._nextRaising=Time::Now() + timeout)]);
	if (!it)
		it.reset(new std::set<const OnTimer*>());
	it->emplace(&onTimer);
}
void Timer::remove(const OnTimer& onTimer) const {
	if (!onTimer._nextRaising)
		return;
	const auto& it(_timers.find(onTimer._nextRaising));
	if (it != _timers.end()) {
		const auto& itTimer(it->second->find(&onTimer));
		if (itTimer != it->second->end()) {
			if (it->second->size()==1)
				_timers.erase(it);
			else
				it->second->erase(itTimer);
			onTimer._nextRaising = 0;
			--_count;
			return;
		}
	}
	FATAL_ERROR("Timer already used on an other Timer machine, create both individual Timer::Type rather");
}

UInt32 Timer::raise() {
	while (!_timers.empty()) {
		const auto& it(_timers.begin());
		Int64 now(Time::Now());
		if (it->first > now)
			return UInt32(it->first - now); // > 0!
		auto itTimers(it->second);
		_timers.erase(it);
		for (const OnTimer* pTimer : *itTimers) {
			pTimer->_nextRaising = 0;
			UInt32 timeout = (*pTimer)();
			--_count;
			if(timeout)
				add(*pTimer, timeout);
		}
	}
	return 0; //empty!
}


} // namespace Mona
