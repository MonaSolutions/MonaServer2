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


#include "Mona/Handler.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

void Handler::queue(const Event<void()>& onResult) const {
	struct Result : Runner, virtual Object {
		Result(const Event<void()>& onResult) : _onResult(onResult), Runner(typeof(onResult).c_str()) {}
		bool run(Exception& ex) { _onResult(); return true; }
	private:
		Event<void()>	_onResult;
	};
	queue(make_shared<Result>(onResult));
}

UInt32 Handler::flush() {
	// Flush all what is possible now, and not dynamically in real-time (in rechecking _runners)
	// to keep the possibility to do something else between two flushs!
	deque<shared<Runner>> runners;
	{
		lock_guard<mutex> lock(_mutex);
		runners = move(_runners);
	}
	Exception ex;
	for (shared<Runner>& pRunner : runners) {
		Thread::ChangeName newName('.',pRunner->name); // '.' to signal that its a sub-runner, wait the name of the thread in htop
		AUTO_ERROR(pRunner->run(ex=nullptr), newName);
		pRunner.reset(); // release resources
	}
	return runners.size();
}


} // namespace Mona
