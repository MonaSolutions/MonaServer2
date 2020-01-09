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
#include "Mona/Util.h"
#include "limits.h"
#include <set>
#include <climits>

using namespace Mona;
using namespace std;

namespace UtilTest {

template<typename TypeD, typename Type>
bool TestDistance(TypeD distance, Type p1, Type p2, Type max, Type min = 0) {
	if(Util::Distance(p1, p2, max, min)!=distance)
		return false;
	return -distance == Util::Distance(p2, p1, max, min);
}

ADD_TEST(distance) {
	CHECK(TestDistance(-3, 0, 5, 7));
	CHECK(TestDistance(-3, 2, 7, 9, 2));
	
	CHECK(TestDistance(4, 0, 4, 7));
	CHECK(TestDistance(4, 0, 4, 8));
	CHECK(TestDistance(4, 2, 6, 10, 2));
	
	CHECK(TestDistance(-1, 0, 7, 7));
	CHECK(TestDistance(-1, 2, 9, 9, 2));

	CHECK(TestDistance(-1, 0, INT_MAX, INT_MAX));
	
	CHECK(TestDistance(-1, INT_MIN, INT_MAX, INT_MAX, INT_MIN));
	CHECK(TestDistance(LLONG_MAX, UInt64(0), UInt64(LLONG_MAX), UInt64(ULLONG_MAX)));

	
	CHECK(Util::AddDistance(1, 11, 5) == 0);
	CHECK(Util::AddDistance(18, 100, 20, 11) == 18);
	CHECK(Util::AddDistance(1, -11, 5) == 2);
	
	CHECK(Util::AddDistance(0x7FFFFFFFu, 1u, 0xFFFFFFFFu, 1u) == 0x80000000);
	CHECK(Util::AddDistance(4u, -5, 0x1000000u, 1u) == 0xFFFFFF);
	CHECK(Util::AddDistance(4, -5, 0x1000000, 1) == 0xFFFFFF);
	CHECK(Util::AddDistance(1, INT_MAX-1, INT_MAX, INT_MIN) == INT_MAX);
	CHECK(Util::AddDistance(1, INT_MAX, INT_MAX, INT_MIN) == INT_MIN);
	CHECK(Util::AddDistance(-1, INT_MIN, INT_MAX, INT_MIN) == INT_MAX);
	CHECK(Util::AddDistance(1, INT_MAX, INT_MAX) == 0);
	CHECK(Util::AddDistance(INT_MAX, 1u, INT_MAX, INT_MIN) == INT_MIN);
	CHECK(Util::AddDistance(INT_MAX, 1, INT_MAX, INT_MIN) == INT_MIN);
	CHECK(Util::AddDistance(INT_MIN, -1, INT_MAX, INT_MIN) == INT_MAX);
	CHECK(Util::AddDistance(-95, -6, -20, -100) == -20);
	CHECK(Util::AddDistance(Int64(LLONG_MAX), Int64(1), Int64(LLONG_MAX), Int64(1)) == 1);
	CHECK(Util::AddDistance(UInt64(LLONG_MAX), UInt64(1), UInt64(ULLONG_MAX), UInt64(1)) == UInt64(LLONG_MAX)+1);
}


bool TestEncode(const char* data,UInt32 size, const char* result) {
	static string Value;
	return Util::ToBase64(BIN data, size, Value) == result;
}

bool TestDecode(string data, const char* result, UInt32 size) {
	return Util::FromBase64(data) && memcmp(data.c_str(),result,size)==0;
}

ADD_TEST(Generators) {
	UInt8 max = (Util::Random() % 129) + 128;
	set<UInt8> ids;
	UInt8 next = 0;
	for (UInt8 i = 0; i < max; ++i)
		CHECK(ids.emplace(next = (next + Util::UInt8Generators[max]) % max).second);
}

ADD_TEST(Base64) {
	CHECK(TestEncode(EXPAND("\00\01\02\03\04\05"),"AAECAwQF"));
	CHECK(TestEncode(EXPAND("\00\01\02\03"), "AAECAw=="));
	CHECK(TestEncode(EXPAND("ABCDEF"),"QUJDREVG"));

	CHECK(TestDecode("AAECAwQF", EXPAND("\00\01\02\03\04\05")));
	CHECK(TestDecode("AAECAw==", EXPAND("\00\01\02\03")));
	CHECK(TestDecode("QUJDREVG", EXPAND("ABCDEF")));
	CHECK(TestDecode("QUJ\r\nDRE\r\nVG", EXPAND("ABCDEF")));
	CHECK(!TestDecode("QUJD#REVG", EXPAND("ABCDEF")));

	static string Message("The quick brown fox jumped over the lazy dog.");
	static string Result;
	Util::ToBase64(BIN Message.c_str(), Message.size(), Result);
	CHECK(Util::FromBase64(Result) && Result==Message);
	CHECK(Result==Message);


	UInt8 data[255];
	for (UInt8 i = 0; i < 255; ++i)
		data[i] = i;
	Util::ToBase64(data, sizeof(data), Result);
	CHECK(Util::FromBase64(Result));
	CHECK(memcmp(Result.data(), data, sizeof(data)) == 0);
}


}

