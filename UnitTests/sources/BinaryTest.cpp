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
#include "Mona/BinaryWriter.h"
#include "Mona/BinaryReader.h"

using namespace Mona;
using namespace std;

namespace BinaryTest {

template <typename ...Args>
void Write(Args&&... args) {
	BinaryWriter writer(args ...);

	bool bval = true;
	writer.write(&bval, sizeof(bval));
	bval = false;
	writer.write(&bval, sizeof(bval));
	writer.write8('a');
	writer.write16(-100);
	writer.write16(50000);
	writer.write32(-123456);
	writer.write32(123456);
	writer.write32(-1234567890);
	writer.write32(1234567890);

	writer.write64(-1234567890);
	writer.write64(1234567890);

	writer.writeDouble(UInt64(0x2FFFFFFFFFFFFE)); // last convertible uint to double without truncation

	float fVal = 1.5;
	writer.write(&fVal, sizeof(fVal));
	double dVal = -1.5;
	writer.write(&dVal, sizeof(dVal));

	writer.write7Bit<UInt32>(100);
	writer.write7Bit<UInt32>(1000);
	writer.write7Bit<UInt32>(10000);
	writer.write7Bit<UInt32>(100000);
	writer.write7Bit<UInt32>(1000000);

	writer.write7Bit<UInt64>(100);
	writer.write7Bit<UInt64>(1000);
	writer.write7Bit<UInt64>(10000);
	writer.write7Bit<UInt64>(100000);
	writer.write7Bit<UInt64>(1000000);

	writer.write("RAW");
}

template <typename ...Args>
void Read(Args&&... args) {
	BinaryReader reader(args ...);
	bool b;
	reader.read(sizeof(b), (char*)&b);
	CHECK(b);
	reader.read(sizeof(b), (char*)&b);
	CHECK(!b);

	char c = reader.read8();
	CHECK(c == 'a');

	short shortv = reader.read16();
	CHECK(shortv == -100);

	unsigned short ushortv = reader.read16();
	CHECK(ushortv == 50000);

	int intv = reader.read32();
	CHECK(intv == -123456);

	unsigned uintv = reader.read32();
	CHECK(uintv == 123456);

	Int32 longv = reader.read32();
	CHECK(longv == -1234567890);

	UInt32 ulongv = reader.read32();
	CHECK(ulongv == 1234567890);

	Int64 int64v = reader.read64();
	CHECK(int64v == -1234567890);

	UInt64 uint64v = reader.read64();
	CHECK(uint64v == 1234567890);

	uint64v = (UInt64)reader.readDouble();
	CHECK(uint64v == 0x2FFFFFFFFFFFFE);

	float floatv;
	reader.read(sizeof(floatv), (char *)&floatv);
	CHECK(floatv == 1.5);

	double doublev;
	reader.read(sizeof(doublev), (char *)&doublev);
	CHECK(doublev == -1.5);

	UInt32 uint32v = reader.read7Bit<UInt32>();
	CHECK(uint32v == 100);
	uint32v = reader.read7Bit<UInt32>();
	CHECK(uint32v == 1000);
	uint32v = reader.read7Bit<UInt32>();
	CHECK(uint32v == 10000);
	uint32v = reader.read7Bit<UInt32>();
	CHECK(uint32v == 100000);
	uint32v = reader.read7Bit<UInt32>();
	CHECK(uint32v == 1000000);

	uint64v = reader.read7Bit<UInt64>();
	CHECK(uint64v == 100);
	uint64v = reader.read7Bit<UInt64>();
	CHECK(uint64v == 1000);
	uint64v = reader.read7Bit<UInt64>();
	CHECK(uint64v == 10000);
	uint64v = reader.read7Bit<UInt64>();
	CHECK(uint64v == 100000);
	uint64v = reader.read7Bit<UInt64>();
	CHECK(uint64v == 1000000);

	CHECK(String::ICompare(STR reader.current(), 3, "RAW")==0);
}


ADD_TEST(Native) {
	Buffer buffer(128);
	Write(buffer.data(),buffer.size(), Byte::ORDER_NATIVE);
	Read(buffer.data(),buffer.size(), Byte::ORDER_NATIVE);
}

ADD_TEST(BigEndian) {
	Buffer buffer(128);
	Write(buffer.data(),buffer.size(), Byte::ORDER_BIG_ENDIAN);
	Read(buffer.data(),buffer.size(), Byte::ORDER_BIG_ENDIAN);
}

ADD_TEST(LittleEndian) {
	Buffer buffer(128);
	Write(buffer.data(),buffer.size(), Byte::ORDER_LITTLE_ENDIAN);
	Read(buffer.data(), buffer.size(), Byte::ORDER_LITTLE_ENDIAN);
}

}
