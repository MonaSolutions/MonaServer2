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
#include "Mona/BufferPool.h"

using namespace Mona;
using namespace std;

namespace BufferTest {

ADD_TEST(Buffer) {
	Buffer buffer;
	CHECK(buffer.size()==0);
	CHECK(buffer.capacity() == 0);
	buffer.resize(10, false);
	CHECK(buffer.size()==10);
	UInt8* data(buffer.data());
	for (int i = 0; i < 10; ++i)
		*data++ = 'a'+i;
	CHECK(memcmp(buffer.data(), EXPAND("abcdefghij")) == 0);
	buffer.resize(100, true);
	CHECK(buffer.size()==100);
	CHECK(memcmp(buffer.data(), EXPAND("abcdefghij"))==0)
	CHECK(buffer.capacity()==128);
	buffer.clip(9);
	CHECK(buffer.size()==91);
	CHECK(*buffer.data()=='j')
	CHECK(buffer.size()>0);
	buffer.clear();
	CHECK(buffer.size()==0);
}

ADD_TEST(BufferPool) {
	Buffer::Allocator::Set<BufferPool>();

	UInt8* buffer;
	{
		Buffer buffer1(8);
		buffer = buffer1.data();
		CHECK(buffer1.capacity() == 16);
	}
	{
		Buffer buffer1(16);
		CHECK(buffer1.capacity() == 16);
		CHECK(buffer1.data() == buffer);
		Buffer buffer2(16);
		CHECK(buffer1.capacity() == 16);
		CHECK(buffer2.data() != buffer);
	}
	{
		Buffer buffer1(1000);
		CHECK(buffer1.capacity() == 1024);
		CHECK(buffer1.data() != buffer);
		buffer = buffer1.data();
	}
	Buffer::Allocator::Set(); // reset default Allocator
	Buffer buffer1(1000);
	CHECK(buffer1.capacity() == 1024);
}

}
