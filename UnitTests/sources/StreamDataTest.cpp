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

#include "Mona/UnitTest.h"
#include "Mona/StreamData.h"

using namespace std;
using namespace Mona;

namespace StreamDataTest {



struct StreamTest : virtual Object, StreamData<UInt32&>, Packet {
	StreamTest(const Packet& pattern, UInt32 times) : _times(times), _pattern(pattern) {}
private:
	UInt32 onStreamData(Packet& buffer, UInt32& i) {
		Packet::operator=(move(buffer));
		UInt32 size = buffer.size();
		CHECK(size == (_pattern.size()*++i));
		do {
			CHECK(memcmp(buffer.data(), _pattern.data(), _pattern.size()) == 0);
		} while (buffer += _pattern.size());
		if (i < _times)
			return size;
		Packet::reset();
		return 0;
	}
	UInt32 _times;
	Packet _pattern;
};


ADD_TEST(Normal) {
	Packet bonjour(EXPAND("Bonjour"));
	StreamTest test(bonjour, 2);

	UInt32 i = 0;
	CHECK(test.addStreamData(bonjour, 0xFFFF, i) && test == bonjour);
	CHECK(test.addStreamData(bonjour, 0xFFFF, i) && !test);
}


}