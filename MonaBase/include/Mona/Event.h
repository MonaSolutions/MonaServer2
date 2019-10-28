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
#include "Mona/Exceptions.h"
#include <functional>
#include <mutex>

namespace Mona {

#define ON(NAME)  On##NAME; On##NAME on##NAME

template<typename Result, typename... Args> struct Event;
/*!
Fast mechanism event
onEvent1 = onEvent2 = []() { ... code ...} => onEvent1() { onEvent2() { []() { ... code ...} } }; 
onEvent3 = onEvent2 => OK, onEvent3() { onEvent2() { []() { ... code ...} } }; 
onEvent1 = onEvent3 => FATAL, can not be bound to multiple functions, unsubscribe with onEvent1 = nullptr in first!
onEvent2 = []() { ... code2 ...} => OK
delete onEvent1 => OK
delete onEvent2 => OK
When Object1 has to manipulate Object2 and Object2 manipulates Object1 uses an event to least solicited way
When Base class can invoke Child class function, uses event 
Protect from bad code:
- Avoid event subscription overrides
- Avoid the case where base class call the event assigned to a function of child class
- Resolve "double inclusion" conception problem
Finally allows too to return an argument (result, why multiple subscription is forbidden, prefered rather a conception model with a list of subscriber)
/!\ For performance and design reason it's not thread-safe */
template<typename Result, typename... Args>
struct Event<Result(Args ...)> : virtual Object {
	NULLABLE(!_pFunction || !_pFunction->operator bool()) // 'true' if is subscribed

	Event(std::nullptr_t) {} // Null Event, usefull just for (const Event& event=nullptr) default parameter
	Event() : _pFunction(SET) {}
	Event(const Event& event) : _pFunction(SET) { operator=(event); }
	Event(const Event&& event) : _pFunction(event._pFunction) {}
	/*!
	Build an event subscriber from lambda function */
	explicit Event(std::function<Result(Args...)>&& function) : _pFunction(SET, std::move(function)) {}

	/*!
	Raise the event */
	Result operator()(Args... args) const { return _pFunction && *_pFunction ? (*_pFunction)(std::forward<Args>(args)...) : Result();}

	/*!
	Assign lambda function */
	Event& operator=(std::function<Result(Args...)>&& function) {
		if (!_pFunction)
			FATAL_ERROR("Null event ", typeof(*this), " can't assign function ", typeof(function));
		if (*_pFunction)
			FATAL_ERROR("Event ", typeof(*this), " already subscribed, unsubscribe before with nullptr assignement");
		*_pFunction = std::move(function);
		return *this;
	}
	/*!
	Subscribe to event */
	Event& operator=(const Event& event) {
		if (!_pFunction)
			FATAL_ERROR(typeof(event), " try to subscribe to null event");
		if (*_pFunction)
			FATAL_ERROR("Event ", typeof(*this), " already subscribed, unsubscribe before with nullptr assignement");
		*_pFunction = [weakFunction = weak<std::function<Result(Args...)>>(event._pFunction)](Args... args) {
			shared<std::function<Result(Args...)>> pFunction(weakFunction.lock());
			return (pFunction && *pFunction) ? (*pFunction)(std::forward<Args>(args)...) : Result();
		};
		return *this;
	}
	Event& operator=(const Event&& event) {
		if (!_pFunction)
			FATAL_ERROR(typeof(event), " try to subscribe to null event");
		if (*_pFunction)
			FATAL_ERROR("Event ", typeof(*this), " already subscribed, unsubscribe before with nullptr assignement");
		_pFunction = event._pFunction;
		return *this;
	}
	/*!
	Unsubscribe to event or erase function */
	Event& operator=(std::nullptr_t) {
		if(_pFunction)
			*_pFunction = nullptr;
		return *this;
	}

private:
	shared<std::function<Result(Args...)>>	_pFunction;
};



} // namespace Mona
