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
#include "Mona/Runner.h"
#include "Mona/Time.h"
#include <set>

namespace Mona {


struct Timer : virtual Object {
	Timer() : _count(0) {}
	~Timer();

/*!
	OnTimer is a function which returns the timeout in ms of next call, or 0 to stop the timer.
	"count" parameter informs on the number of raised time */
	struct OnTimer : std::function<UInt32(UInt32 count)>, virtual Object {
		OnTimer() : _nextRaising(0), count(0) {}
		// explicit to forbid to pass in "const OnTimer" parameter directly a lambda function
		template<typename FunctionType>
		explicit OnTimer(FunctionType&& function) : _nextRaising(0), count(0), std::function<UInt32(UInt32)>(std::move(function)) {}

		~OnTimer() { if (_nextRaising) FATAL_ERROR("OnTimer function deleting while running"); }

		template<typename FunctionType>
		OnTimer& operator=(FunctionType&& function) {
			std::function<UInt32(UInt32)>::operator=(std::move(function));
			return *this;
		}

		operator UInt32() const { if (!_nextRaising) return 0; Int64 now(Time::Now()); return UInt32(now <= _nextRaising ? 1 : (now - _nextRaising)); } // subscribed + timeout infos

		/*!
		Call the timer, also allow to call the timer on set => timer.set(onTimer, onTimer()); */
		UInt32 operator()() const { return std::function<UInt32(UInt32 count)>::operator()(++(UInt32&)count); }

		const UInt32 count;
	private:
		mutable Int64	_nextRaising;

		friend struct Timer;
	};

	UInt32 count() const { return _count; }

/*!
	Set timer, timeout is the first raising timeout
	If timeout is 0 it removes the timer! */
	void set(const OnTimer& onTimer, UInt32 timeout) const;

/*!
	Return the time in ms to wait before to call one time again raise method (>0), or 0 if there is no more timer! */
	UInt32 raise();

private:
	void add(const OnTimer& onTimer,  UInt32 timeout) const;
	void remove(const OnTimer& onTimer) const;

	mutable	UInt32														_count;
	mutable std::map<Int64, shared<std::set<const OnTimer*>>>	_timers;
};



} // namespace Mona
