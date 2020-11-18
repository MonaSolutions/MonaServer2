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

#include "Mona/Segment.h"

using namespace std;

namespace Mona {


size_t Segment::ReadName(const string& name, UInt32& sequence, UInt16& duration) {
	// check format NAME.S###
	size_t size = name.rfind('.');
	if (size == string::npos || (name.size() - size) < 5)
		return string::npos; // no '.' and space for S###
	const char* code = name.c_str() + name.size();
	if(!isb64url(*--code) || !isb64url(*--code) || !isb64url(*--code))
		return string::npos; // no valid ### code
	// extract sequence
	const char* seq = name.c_str() + size + 1;
	if (!String::ToNumber(seq, code - seq, sequence))
		return string::npos; // no valid number S sequence
	// extract duration
	string text;
	DEBUG_CHECK(Util::FromBase64URL(BIN code, 3, text));
	duration = Byte::From16Network(*(UInt16*)text.data());
	return size;
}



bool Segment::add(UInt32 time) {
	if (!_pFirstTime) {
		_pFirstTime.set(_lastTime = time);
		return true;
	}
	if (Util::Distance(_lastTime, time) <= 0)
		return true;
	if (Util::Distance(*_pFirstTime, _lastTime) > 0xFFFF)
		return false;
	_lastTime = time;
	return true;
}


} // namespace Mona
