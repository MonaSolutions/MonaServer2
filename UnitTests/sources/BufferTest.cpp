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
	Timer timer;
	BufferPool	bufferPool(timer);
	UInt32 size(10000);
	UInt8* buffer(bufferPool.allocate(size));
	CHECK(buffer && size == 10000 && !bufferPool.available());
	bufferPool.deallocate(buffer, size);
	CHECK(bufferPool.available()==1);
	size = 9999;
	buffer = bufferPool.allocate(size);
	CHECK(buffer && size == 10000 && !bufferPool.available());
	bufferPool.deallocate(buffer, size);
	CHECK(bufferPool.available()==1);
	bufferPool.clear();
	CHECK(!bufferPool.available());
	
	Buffer::SetAllocator(bufferPool);
	{
		Buffer buffer;
		CHECK(!bufferPool.available());
	}
	CHECK(!bufferPool.available());
	{
		Buffer buffer(1);
		CHECK(!bufferPool.available());
	}
	CHECK(bufferPool.available()==1);

	Buffer::SetAllocator();
	{
		Buffer buffer;
		CHECK(bufferPool.available()==1);
	}
}

}
