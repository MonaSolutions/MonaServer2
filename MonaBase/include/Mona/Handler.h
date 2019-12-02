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
	Handler(Signal& signal) : _pSignal(&signal) {}
	Handler() : _pSignal(NULL) {}

	void	 reset(Signal& signal);
	UInt32	 flush(bool last=false);

	/*!
	Try to queue a shared RunnerType, returns false if failed */
	template<typename RunnerType, typename = typename std::enable_if<std::is_constructible<shared<Runner>, RunnerType>::value>::type>
	bool tryQueue(RunnerType&& pRunner) const {
		DEBUG_ASSERT(pRunner); // more easy to debug that if it fails in the thread!
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_pSignal)
			return false;
		_runners.emplace_back(std::forward<RunnerType>(pRunner));
		_pSignal->set();
		return true;
	}
	/*!
	Try to build and queue a RunnerType, returns false if failed */
	template <typename RunnerType, typename ...Args>
	bool tryQueue(Args&&... args) const { return tryQueue(std::make_shared<RunnerType>(std::forward<Args>(args)...)); }
	/*!
	Try to queue an event with arguments call, returns false if failed */
	template<typename ResultType, typename ...Args>
	bool tryQueue(const Event<void(ResultType)>& onResult, Args&&... args) const {
		struct Result : Runner, virtual Object {
			Result(const Event<void(ResultType)>& onResult, Args&&... args) : _result(std::forward<Args>(args)...), _onResult(std::move(onResult)), Runner(typeof<ResultType>().c_str()) {}
			bool run(Exception& ex) { _onResult(_result); return true; }
		private:
			Event<void(ResultType)>								_onResult;
			typename std::remove_reference<ResultType>::type	_result;
		};
		return tryQueue(std::make_shared<Result>(onResult, std::forward<Args>(args)...));
	}
	/*!
	Try to queue an event without argument, returns false if failed */
	bool tryQueue(const Event<void()>& onResult) const;
	/*!
	Queue a shared RunnerType, returns false if failed */
	template<typename RunnerType, typename = typename std::enable_if<std::is_constructible<shared<Runner>, RunnerType>::value>::type>
	void queue(RunnerType&& pRunner) const {
		if (!tryQueue(std::forward<RunnerType>(pRunner)))
			FATAL_ERROR("Impossible to queue ", typeof<RunnerType>());
	}
	/*!
	Build and queue a RunnerType, returns false if failed */
	template <typename RunnerType, typename ...Args>
	void queue(Args&&... args) const {
		if(!tryQueue(std::make_shared<RunnerType>(std::forward<Args>(args)...)))
			FATAL_ERROR("Impossible to queue ", typeof<RunnerType>());
	}
	/*!
	Queue an event with arguments call, returns false if failed */
	template<typename ResultType, typename ...Args>
	void queue(const Event<void(ResultType)>& onResult, Args&&... args) const {
		if(!tryQueue(onResult, std::forward<Args>(args)...))
			FATAL_ERROR("Impossible to queue ", typeof(onResult));
	}
	/*!
	Queue an event without argument, returns false if failed */
	void queue(const Event<void()>& onResult) const {
		if(!tryQueue(onResult))
			FATAL_ERROR("Impossible to queue ", typeof(onResult));
	}

private:

	mutable std::mutex					_mutex;
	mutable std::deque<shared<Runner>>	_runners;
	Signal*								_pSignal;
};



} // namespace Mona
