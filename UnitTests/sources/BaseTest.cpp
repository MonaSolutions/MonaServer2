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
#include "Mona/XMLParser.h"


using namespace Mona;
using namespace std;

namespace BaseTest {

template<typename Type>
struct Obj {};

ADD_TEST(typeof) {
	Buffer buffer;
	CHECK(typeof(buffer) == "Buffer");
	int i;
	CHECK(typeof(i) == "int");
	XMLParser::XMLState state;
	CHECK(typeof(state) == "XMLParser::XMLState");
	Obj<Buffer> type;
	CHECK(typeof(type) == "BaseTest::Obj<Buffer>");
}

ADD_TEST(parameters) {
	Parameters parameters;
	parameters.setString("name1", "value1");
	parameters.setString("name2", "value2");
	parameters.setString("name2_", "value2_");
	parameters.setString("name3", "value4");

	UInt32 size(0);
	for (auto& it : parameters.range("name2")) {
		CHECK(it.first.compare(0,5, "name2") == 0);
		++size;
	}
	CHECK(size == 2);

	size = 0;
	for (auto& it : parameters.from("name2")) {
		CHECK(it.first.compare("name1") != 0);
		++size;
	}
	CHECK(size == 3);
}

ADD_TEST(range) {

	UInt32 u32 = 0xFFFFFFFF;
	CHECK(range<UInt16>(u32) == 0xFFFF);

	UInt16 u16 = 0xFFFF;
	CHECK(range<UInt32>(u16) == 0x0000FFFF);

	Int32 i32 = -1;
	CHECK(range<UInt32>(i32) == 0);

	u32 = 0xFFFFFFFF;
	CHECK(range<Int32>(u32) == 0x7FFFFFFF);

}

}
