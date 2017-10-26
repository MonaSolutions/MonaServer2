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
#include "Mona/Event.h"
#include "Mona/Signal.h"
#include <deque>

namespace Mona {

struct Handler : virtual Object {
	Handler(Signal& signal) : _signal(signal) {}

	template<typename RunnerType>
	void queue(const shared<RunnerType>& pRunner) const {
		FATAL_CHECK(pRunner);
		std::lock_guard<std::mutex> lock(_mutex);
		_runners.emplace_back(pRunner);
		_signal.set();
	}

	template<typename ResultType, typename BaseType, typename ...Args>
	void queue(const Event<void(BaseType)>& onResult, Args&&... args) const {
		struct Result : Runner, virtual Object {
			Result(const Event<void(BaseType)>& onResult, Args&&... args) : _result(std::forward<Args>(args)...), _onResult(std::move(onResult)), Runner(typeof<ResultType>().c_str()) {}
			bool run(Exception& ex) { _onResult(_result); return true; }
		private:
			Event<void(BaseType)>								_onResult;
			typename std::remove_reference<ResultType>::type	_result;
		};
		queue(std::make_shared<Result>(onResult, std::forward<Args>(args)...));
	}
	template<typename ResultType, typename ...Args>
	void queue(const Event<void(ResultType)>& onResult, Args&&... args) const {
		queue<ResultType, ResultType>(onResult, std::forward<Args>(args)...);
	}
	void queue(const Event<void()>& onResult) const;


	UInt32 flush();

private:

	mutable std::mutex					_mutex;
	mutable std::deque<shared<Runner>>	_runners;
	Signal&								_signal;
};



} // namespace Mona
