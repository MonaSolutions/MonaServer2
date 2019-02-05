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

#include "Mona/Logs.h"
#include "Mona/Util.h"

using namespace std;

namespace Mona {


mutex					Logs::_Mutex;

volatile bool			Logs::_IsDumping(false);
string					Logs::_Dump;

Int32					Logs::_DumpLimit(-1);
volatile bool			Logs::_DumpRequest(true);
volatile bool			Logs::_DumpResponse(true);
atomic<LOG_LEVEL>		Logs::_Level(LOG_DEFAULT); // default log level
Logs::Loggers			Logs::_Loggers;
thread_local bool		Logs::_Logging(false);

void Logs::SetDump(const char* name) {
	lock_guard<mutex> lock(_Mutex);
	_DumpResponse = _DumpRequest = true;
	if (!name) {
		_IsDumping = false;
		_Dump.clear();
		_Dump.shrink_to_fit();
		return;
	}
	_IsDumping = true;
	_Dump = name;
	if (_Dump.empty())
		return;
	if (_Dump.back() == '>') {
		_DumpRequest = false;
		_Dump.pop_back();
	} else if (_Dump.back() == '<') {
		_DumpResponse = false;
		_Dump.pop_back();
	}
}

void Logs::Dump(const string& header, const UInt8* data, UInt32 size) {
	Buffer out;
	Util::Dump(data, (_DumpLimit<0 || size<UInt32(_DumpLimit)) ? size : _DumpLimit, out);
	for (auto& it : _Loggers) {
		if (*it.second && !it.second->dump(header, out.data(), out.size()))
			_Loggers.fail(*it.second);
	}
	_Loggers.flush();
}

void Logs::Loggers::flush() {
	while (!_failed.empty()) {
		Logger& logger(*_failed.front());
		String message(logger.name, " log has failed");
		const char* fatal = logger.fatal;
		if (fatal) {
			if (*fatal)
				String::Append(message, ", ", fatal);
		}
		_failed.pop_back();
		erase(logger.name); // erase here to remove it from _Loggers before dispatching loop
		for (auto& it : _Loggers) {
			if (*it.second && !it.second->log(LOG_ERROR, __FILE__, __LINE__, message))
				_failed.emplace_back(it.second.get());
		}
		if (fatal) // fatal is last to get logs on the other targets
			FATAL_ERROR(message);
	}
}


} // namespace Mona
