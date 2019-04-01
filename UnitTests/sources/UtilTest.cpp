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
#include "Mona/Util.h"

using namespace Mona;
using namespace std;

namespace UtilTest {

bool TestDistance(Int32 distance, UInt32 p1, UInt32 p2, UInt32 max, UInt32 min = 0) {
	if(Util::Distance(p1, p2, max, min)!=distance)
		return false;
	return -distance == Util::Distance(p2, p1, max, min);
}

ADD_TEST(distance) {
	CHECK(TestDistance(-3, 0, 5, 7));
	CHECK(TestDistance(-3, 2, 7, 9, 2));

	CHECK(TestDistance(4, 0, 4, 7));
	CHECK(TestDistance(4, 0, 4, 9, 2));
	CHECK(TestDistance(4, 0, 4, 8));
	CHECK(TestDistance(4, 2, 6, 10, 2));

	CHECK(TestDistance(-1, 0, 7, 7));
	CHECK(TestDistance(-1, 2, 9, 9, 2));
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

ADD_TEST(UnpackQuery) {
	string value;
	Parameters properties;
	CHECK(Util::UnpackQuery("name1=value1&name2=value2", properties).count()==2)
	DEBUG_CHECK(properties.getString("name1", value) && value == "value1");
	DEBUG_CHECK(properties.getString("name2", value) && value == "value2");
	properties.clear();


	string test("name1=one%20space&name2=%22one double quotes%22&name3=percent:%25&name4=%27simple quotes%27");
	CHECK(Util::UnpackQuery(test, properties).count()==4); // test "count" + DecodeUrI
	DEBUG_CHECK(properties.getString("name1", value) && value == "one space");
	DEBUG_CHECK(properties.getString("name2", value) && value == "\"one double quotes\"");
	DEBUG_CHECK(properties.getString("name3", value) && value == "percent:%");
	DEBUG_CHECK(properties.getString("name4", value) && value == "'simple quotes'");
	CHECK(Util::UnpackQuery("longquery://test;:/one%*$^/fin=value~", properties).count()==5)
	DEBUG_CHECK(properties.getString("longquery://test;:/one%*$^/fin", value) && value == "value~");

	bool next(true);
	Util::ForEachParameter forEach([&next](const string& name, const char* value) { return next; });
	CHECK(Util::UnpackQuery(test.c_str(), forEach) == 4); // test "string::pos" + DecodeUrI
	CHECK(Util::UnpackQuery("name1=value1&name2=value2", 12, forEach) == 1);
	next = false;
	CHECK(Util::UnpackQuery(test, forEach) == 1);

}


ADD_TEST(UnpackUrlPerf) {
	string address;
	string path;
	string query;
	CHECK(Util::UnpackUrl("rtmp://127.0.0.1:1234/path/file.txt?name1=value1&name2=value2", address, path,query)!=string::npos)
}


ADD_TEST(UnpackUrl) {
	string address;
	string path;
	string query;
	string file;

	CHECK(Util::UnpackUrl("/path",path,query)!=string::npos && path=="/path");
	CHECK(Util::UnpackUrl("/",path,query)==string::npos && path=="");
	CHECK(Util::UnpackUrl("/.",path,query)==string::npos && path=="");
	CHECK(Util::UnpackUrl("/..",path,query)==string::npos && path=="");
	CHECK(Util::UnpackUrl("/~",path,query)!=string::npos && path=="/~");
	CHECK(Util::UnpackUrl("/path/.",path,query)==string::npos && path=="/path");
	CHECK(Util::UnpackUrl("/path/..",path,query)==string::npos && path=="");
	CHECK(Util::UnpackUrl("/path/~",path,query)!=string::npos && path=="/path/~");
	CHECK(Util::UnpackUrl("/path/./sub/",path,query)==string::npos && path=="/path/sub");
	CHECK(Util::UnpackUrl("/path/../sub/",path,query)==string::npos && path=="/sub");
	CHECK(Util::UnpackUrl("/path/~/sub/",path,query)==string::npos && path=="/path/~/sub");
	CHECK(Util::UnpackUrl("//path//sub//.",path,query)==string::npos && path=="/path/sub");
	CHECK(Util::UnpackUrl("//path//sub//..",path,query)==string::npos && path=="/path");
	CHECK(Util::UnpackUrl("//path//sub//~",path,query)!=string::npos && path=="/path/sub/~");

	CHECK(Util::UnpackUrl("rtmp://",path,query)==string::npos);
	CHECK(Util::UnpackUrl("rtmp://127.0.0.1", address, path, query)==string::npos)
	CHECK(Util::UnpackUrl("rtmp://127.0.0.1:1234/", address, path, query)==string::npos)
	CHECK(Util::UnpackUrl("rtmp://127.0.0.1:1234/file.txt?", address, path,query)!=string::npos)
	CHECK(Util::UnpackUrl("rtmp://127.0.0.1:1234/file.txt?name1=value1&name2=value2", address, path, query)!=string::npos)
	CHECK(Util::UnpackUrl("rtmp://127.0.0.1:1234//path/file.txt?name1=value1&name2=value2", address, path, query)!=string::npos)

	DEBUG_CHECK(query == "?name1=value1&name2=value2");
	DEBUG_CHECK(path=="/path/file.txt");
	DEBUG_CHECK(address=="127.0.0.1:1234");

	DEBUG_CHECK(Util::UnpackUrl("srt://www.clubic.com?param=value", address, path, query) == string::npos && address=="www.clubic.com" && query=="?param=value" && path.empty());
}

ADD_TEST(EncodeURI) {
	string query;
	CHECK(Util::EncodeURI("https://127.0.0.1/test/", query) == "https%3A%2F%2F127.0.0.1%2Ftest%2F");
	query.clear();
	CHECK(Util::EncodeURI("sàlùt=", query) == "s%E0l%F9t%3D");
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

