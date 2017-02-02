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


ADD_TEST(TimerTest, All) {

	
	Stopwatch stopwatch;
	
	Timer timer;

	Timer::OnTimer onTimer10([](UInt32 count) { return 10; });
	Timer::OnTimer onTimer20([](UInt32 count) { return 20; });
	Timer::OnTimer onTimer40([](UInt32 count) { return 40; });
	Timer::OnTimer onTimer50([](UInt32 count) { return 50; });
	Timer::OnTimer onInsideRemove; onInsideRemove = [&](UInt32 count) { timer.set(onInsideRemove, 0);  return 0; };
	Timer::OnTimer onAutoRemove([](UInt32 count) { return 0; });

	timer.set(onTimer10, 10);
	timer.set(onTimer20, 20);
	timer.set(onTimer40, 40);
	timer.set(onTimer50, 50);
	timer.set(onTimer50, 55);
	timer.set(onInsideRemove, 50);
	timer.set(onAutoRemove, 55);

	CHECK(timer.count() == 6);

	stopwatch.start();

	UInt32 timeout;
	while ((timeout = timer.raise()) && timer.count() > 4)
		Thread::Sleep(timeout);

	stopwatch.stop();

	CHECK(stopwatch.elapsed()>50 && stopwatch.elapsed()<=60);

	CHECK(onTimer10.count == 5 && onTimer20.count == 2 && onTimer40.count == 1 && onTimer50.count == 1 && onAutoRemove.count==1 && onInsideRemove.count == 1);
	CHECK(timer.count() == 4);

	timer.set(onTimer10, 0);
	timer.set(onTimer20, 0);
	timer.set(onTimer40, 0);
	timer.set(onTimer50, 0);

	CHECK(!timer.count() && !timer.raise())
}
