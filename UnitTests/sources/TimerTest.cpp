/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "Test.h"
#include "Mona/Stopwatch.h"
#include "Mona/Timer.h"
#include "Mona/Thread.h"

using namespace Mona;
using namespace std;

namespace TimerTest {

ADD_TEST(All) {

	
	Stopwatch stopwatch;
	
	Timer timer;

	Timer::OnTimer onTimer100([](UInt32 delay) { return 100; });
	Timer::OnTimer onTimer200([](UInt32 delay) { return 200; });
	Timer::OnTimer onTimer400([](UInt32 delay) { return 400; });
	Timer::OnTimer onTimer500([](UInt32 delay) { return 500; });
	Timer::OnTimer onInsideRemove; onInsideRemove = [&](UInt32 delay) { timer.set(onInsideRemove, 0);  return 0; };
	Timer::OnTimer onAutoRemove([](UInt32 delay) { return 0; });

	timer.set(onTimer100, 100);
	timer.set(onTimer200, 200);
	timer.set(onTimer400, 400);
	timer.set(onTimer500, 500);
	timer.set(onTimer500, 550);
	timer.set(onInsideRemove, 500);
	timer.set(onAutoRemove, 550);

	CHECK(timer.count() == 6);

	stopwatch.start();

	UInt32 timeout;
	while ((timeout = timer.raise()) && timer.count() > 4)
		Thread::Sleep(timeout);

	stopwatch.stop();

	CHECK(stopwatch.elapsed()>500 && stopwatch.elapsed()<=600);

	CHECK(onTimer100.count == 5 && onTimer200.count == 2 && onTimer400.count == 1 && onTimer500.count == 1 && onAutoRemove.count==1 && onInsideRemove.count == 1);
	CHECK(timer.count() == 4);

	timer.set(onTimer100, 0);
	timer.set(onTimer200, 0);
	timer.set(onTimer400, 0);
	timer.set(onTimer500, 0);

	CHECK(!timer.count() && !timer.raise())
}

}
