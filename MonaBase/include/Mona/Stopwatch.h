/*
This code is in part based on code from the POCO C++ Libraries,
licensed under the Boost software license :
https://www.boost.org/LICENSE_1_0.txt

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
#include <chrono>

namespace Mona {

/// A chrono in microseconds
struct Stopwatch : virtual Object {
	NULLABLE(!_running)

	Stopwatch() : _elapsed(0) {}

	bool running() const { return _running; }

	void start() { _running = true;  _start = std::chrono::steady_clock::now(); }
	void restart() { _elapsed = 0;  start(); }
	void stop() { _running = false; _elapsed += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start).count(); }

	Int64 elapsed() const { return _running ? std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start).count() : _elapsed; }

private:
	std::chrono::time_point<std::chrono::steady_clock>	_start;
	Int64												_elapsed;
	bool												_running;
};

} // namespace Mona
