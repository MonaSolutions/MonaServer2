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
#include "Mona/BitReader.h"

using namespace Mona;
using namespace std;
#undef BitTest

namespace BitTest {


ADD_TEST(BitReader) {
	BitReader reader(BIN EXPAND("\x00\xFF\xAA"));
	CHECK(reader.available() == 24);
	CHECK(reader.size() == 3);
	CHECK(reader.position() == 0);
	CHECK(reader.read<UInt8>() == 0);
	CHECK(reader.read<UInt8>() == 0xFF);
	CHECK(reader.read<UInt8>() == 0xAA);
	CHECK(reader.position() == 24);

	reader.reset();
	CHECK(reader.position() == 0);
	CHECK(reader.read<UInt8>(8) == 0);
	CHECK(reader.read<UInt8>(8) == 0xFF);
	CHECK(reader.read<UInt8>(8) == 0xAA);

	reader.reset();
	for (UInt8 i = 0; i < 8; ++i)
		CHECK(!reader.read());
	for (UInt8 i = 0; i < 8; ++i)
		CHECK(reader.read());
	for (UInt8 i = 0; i < 8; ++i)
		CHECK(reader.read() == ((i%2) ? false : true));

	reader.reset();
	CHECK(reader.next(0xFF) == 24);
	CHECK(reader.available() == 0);

	reader.reset();
	CHECK(reader.shrink(8) == 8);
	CHECK(reader.available() == 8);
}

}
