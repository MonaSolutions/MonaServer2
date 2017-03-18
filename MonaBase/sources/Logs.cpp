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

#include "Mona/Util.h"
#include <iostream>
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


mutex					Logs::_Mutex;
shared<string> 			Logs::_PDump;
Int32					Logs::_DumpLimit(-1);
atomic<bool>			Logs::_DumpRequest(true);
atomic<bool>			Logs::_DumpResponse(true);
#if defined(_DEBUG)
LOG_LEVEL				Logs::_Level(LOG_DEBUG); // default log level
#else
LOG_LEVEL				Logs::_Level(LOG_INFO); // default log level
#endif
Logger					Logs::_DefaultLogger;
Logger*					Logs::_PLogger(&_DefaultLogger);


void Logs::SetDump(const char* name) {
	lock_guard<mutex> lock(_Mutex);
	_DumpResponse = _DumpRequest = true;
	if (!name)
		return _PDump.reset();
	_PDump.reset(new string(name));
	if (_PDump->empty())
		return;
	if (_PDump->back() == '>') {
		_DumpRequest = false;
		_PDump->resize(_PDump->size() - 1);
	} else if (_PDump->back() == '<') {
		_DumpResponse = false;
		_PDump->resize(_PDump->size() - 1);
	}
}

void Logs::Dump(const string& header, const UInt8* data, UInt32 size) {
	Buffer out;
	lock_guard<mutex> lock(_Mutex);
	Util::Dump(data, (_DumpLimit<0 || size<UInt32(_DumpLimit)) ? size : _DumpLimit, out);
	_PLogger->dump(header, out.data(), out.size());
}


} // namespace Mona
