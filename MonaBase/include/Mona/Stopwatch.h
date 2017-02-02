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

#include "Mona/Time.h"

namespace Mona {

/// A chrono in microseconds
class Stopwatch : public virtual Object{
public:
	Stopwatch() : _start(0),_elapsed(0) {}

	void start() { _start.update(); }
	void restart() { _elapsed = 0; start(); }

	void stop() { if (_start) _elapsed += (Time::Now() - _start); _start = 0; }

	Int64 elapsed() const { if (!_start) return _elapsed;  return _elapsed + _start.elapsed(); }

private:
	Time	_start;
	Int64	_elapsed;
};

} // namespace Mona
