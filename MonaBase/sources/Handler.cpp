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

UInt32 Handler::flush(UInt32 count) {
	bool all(count==0);
	UInt32 done(0);
	for (;;) {
		shared<Runner> pRunner;
		{
			lock_guard<mutex> lock(_mutex);
			if (_runners.empty() || (!all && count--))
				break;
			if (!all && count--)
				break;
			pRunner = move(_runners.front());
			_runners.pop_front();
		}
		Exception ex;
		Thread::ChangeName newName(Thread::CurrentName(), ":", pRunner->name);
		AUTO_ERROR(pRunner->run(ex), newName);
		++done;
	}
	return done;
}



} // namespace Mona
