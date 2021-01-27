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

Mona::Buffer& Buffer() {
	static Mona::Buffer Buffer(256);
	return Buffer;
}

void Write(Byte::Order order) {
	
	BinaryWriter writer(Buffer().data(), Buffer().size(), order);

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
	writer.write7Bit<UInt32>(0xFFFFFFFE);
	writer.write7Bit<UInt32>(0xFFFFFFFF);

	writer.write7Bit<UInt64>(100);
	writer.write7Bit<UInt64>(1000);
	writer.write7Bit<UInt64>(10000);
	writer.write7Bit<UInt64>(100000);
	writer.write7Bit<UInt64>(1000000);
	writer.write7Bit<UInt64>(0xFFFFFFFFFFFFFFFE);
	writer.write7Bit<UInt64>(0xFFFFFFFFFFFFFFFF);

	writer.write7Bit<UInt64>(100, 4);
	writer.write7Bit<UInt64>(1000, 4);
	writer.write7Bit<UInt64>(10000, 4);
	writer.write7Bit<UInt64>(100000, 4);
	writer.write7Bit<UInt64>(1000000, 4);
	writer.write7Bit<UInt64>(0xFFFFFFFE, 4);
	writer.write7Bit<UInt64>(0xFFFFFFFF, 4);
	writer.write7Bit<UInt64>(0x100000000, 4);

	writer.write("RAW");
}

void Read(Byte::Order order) {
	BinaryReader reader(Buffer().data(), Buffer().size(), order);
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

	CHECK(reader.read7Bit<UInt32>() == 100);
	CHECK(reader.read7Bit<UInt32>() == 1000);
	CHECK(reader.read7Bit<UInt32>() == 10000);
	CHECK(reader.read7Bit<UInt32>() == 100000);
	CHECK(reader.read7Bit<UInt32>() == 1000000);
	CHECK(reader.read7Bit<UInt32>() == 0xFFFFFFFE);
	CHECK(reader.read7Bit<UInt32>() == 0xFFFFFFFF);

	CHECK(reader.read7Bit<UInt64>() == 100);
	CHECK(reader.read7Bit<UInt64>() == 1000);
	CHECK(reader.read7Bit<UInt64>() == 10000);
	CHECK(reader.read7Bit<UInt64>() == 100000);
	CHECK(reader.read7Bit<UInt64>() == 1000000);
	CHECK(reader.read7Bit<UInt64>() == 0xFFFFFFFFFFFFFFFE);
	CHECK(reader.read7Bit<UInt64>() == 0xFFFFFFFFFFFFFFFF);

	CHECK(reader.read7Bit<UInt64>(4) == 100);
	CHECK(reader.read7Bit<UInt64>(4) == 1000);
	CHECK(reader.read7Bit<UInt64>(4) == 10000);
	CHECK(reader.read7Bit<UInt64>(4) == 100000);
	CHECK(reader.read7Bit<UInt64>(4) == 1000000);
	CHECK(reader.read7Bit<UInt64>(4) == 0x1FFFFFFF);
	CHECK(reader.read7Bit<UInt64>(4) == 0x1FFFFFFF);
	CHECK(reader.read7Bit<UInt64>(4) == 0x1FFFFFFF);


	CHECK(String::ICompare(STR reader.current(), 3, "RAW")==0);
}

ADD_TEST(Native) {
	Write(Byte::ORDER_NATIVE);
	Read(Byte::ORDER_NATIVE);
}

ADD_TEST(BigEndian) {
	Write(Byte::ORDER_BIG_ENDIAN);
	Read(Byte::ORDER_BIG_ENDIAN);
}

ADD_TEST(LittleEndian) {
	Write(Byte::ORDER_LITTLE_ENDIAN);
	Read(Byte::ORDER_LITTLE_ENDIAN);
}

}
