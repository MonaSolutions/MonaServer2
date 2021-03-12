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
#include "Mona/URL.h"

using namespace Mona;
using namespace std;

namespace URLTest {

ADD_TEST(Perf) {
	string value;
	Path path;
	URL::ForEachParameter forEach([](std::string&, const char*) {
		return true;
	});
	CHECK(URL::ParseQuery(URL::ParseRequest(URL::Parse("rtmp://127.0.0.1:1234/path/file.txt?name1=value1&name2=value2", value), path), forEach)==2);
}

static void CheckURL(const std::string& url,
	const char* protocol, const char* address, const char* path, const char* query) {
	string proto, addr;
	const char* request = URL::Parse(url, proto, addr);
	CHECK(proto == protocol && addr == address);
	size_t size = url.size();
	CHECK(strcmp(request, URL::Parse(url.data(), size, proto, addr))==0 && size == strlen(request));
	CHECK(proto == protocol && addr == address);
	size = string::npos;
	CHECK(strcmp(request, URL::Parse(url.data(), size, proto, addr)) == 0 && size == strlen(request));
	CHECK(proto == protocol && addr == address);

	Path p;
	CHECK(strcmp(URL::ParseRequest(request, p), query)==0 && p == path);
	CHECK(strcmp(URL::ParseRequest(request, size, p), query)==0 && size == strlen(query) && p == path);
	size = string::npos;
	CHECK(strcmp(URL::ParseRequest(request, size, p), query) == 0 && size == strlen(query) && p == path);
}

ADD_TEST(Parse) {
	
	CheckURL("/path", "", "", "/path", "");
	CheckURL("/", "", "", "/", "");
	CheckURL("/.", "", "", "/", "");
	CheckURL("/..", "", "", "/", "");
	CheckURL("/~", "", "", "/~", "");
	CheckURL("/path/.", "", "", "/path/", "");
	CheckURL("/path/..", "", "", "/", "");
	CheckURL("/path/~","", "", "/path/~", "");
	CheckURL("/path/./sub/", "", "", "/path/sub/", "");
	CheckURL("/path/../sub/", "", "", "/sub/", "");
	CheckURL("/path/~/sub/", "", "", "/path/~/sub/", "");
	CheckURL("//path//sub//.", "", "", "/path/sub/", "");
	CheckURL("//path//sub//..", "", "", "/path/", "");
	CheckURL("//path//sub//~", "", "", "/path/sub/~", "");

	CheckURL("/space name/file.txt", "", "", "/space name/file.txt", "");
	CheckURL("space name", "", "", "space name", "");
	CheckURL("Sintel.ts FLV", "", "", "Sintel.ts FLV", "");

	CheckURL("?name=value", "", "", "", "?name=value");
	CheckURL("path?name=value", "", "", "path", "?name=value");
#if defined(_WIN32)
	CheckURL("/C:/path?name=value", "", "", "C:/path", "?name=value");
#else
	CheckURL("/C:/path?name=value", "", "", "/C:/path", "?name=value");
#endif
	CheckURL("localhost", "", "", "localhost", "");
	CheckURL("localhost:8008", "", "localhost:8008", "", "");
	CheckURL("2001:0db8:0000:0000:0000:0000:1428:57ab", "", "2001:0db8:0000:0000:0000:0000:1428:57ab", "", "");
	CheckURL("::1", "", "::1", "", "");
	CheckURL("[::1]:8008", "", "[::1]:8008", "", "");
	CheckURL("http://::1/", "http", "::1", "", ""); // (incorrect in the browser)
	CheckURL("http://[::1]/", "http", "[::1]", "", "");
	CheckURL("http://[::1]/path/test?params", "http", "[::1]", "path/test", "?params");

	CheckURL("localhost", "", "", "localhost", ""); // localhost is path
	CheckURL("localhost:8080", "", "localhost:8080", "", ""); // localhost is address

	CheckURL(" rtmp://","rtmp", "", "", "");
	CheckURL("rtmp://127.0.0.1", "rtmp", "127.0.0.1", "", "");
	CheckURL("rtmp://127.0.0.1:1234/", "rtmp", "127.0.0.1:1234", "", "");
	CheckURL("rtmp://127.0.0.1:1234/file.txt", "rtmp", "127.0.0.1:1234", "file.txt", "");
	CheckURL("rtmp://127.0.0.1:1234/file.txt?", "rtmp", "127.0.0.1:1234", "file.txt", "?");
	CheckURL("rtmp://127.0.0.1:1234/path/file.txt?name1=value1&name2=value2", "rtmp", "127.0.0.1:1234", "path/file.txt", "?name1=value1&name2=value2");
	CheckURL("rtmp://127.0.0.1:1234//path/file.txt?name1=value1&name2=value2", "rtmp", "127.0.0.1:1234", "path/file.txt", "?name1=value1&name2=value2");

	CheckURL("srt://www.clubic.com?param=value", "srt", "www.clubic.com", "", "?param=value");
#if defined(_WIN32)
	CheckURL("srt:///c:/file.txt", "srt", "", "./c:/file.txt", "");
#else
	CheckURL("srt:///c:/file.txt", "srt", "", "c:/file.txt", "");
#endif

	// file protocol give always absolute path!
	CheckURL("file:///", "file", "", "/", "");
	CheckURL("file:///path/test", "file", "", "/path/test", "");
	CheckURL("file:///path/test?params", "file", "", "/path/test", "?params");
#if defined(_WIN32)
	CheckURL("file:///c:/", "file", "", "c:/", ""); // c disk
#else
	CheckURL("file:///c:/", "file", "", "/c:/", ""); // c disk
#endif
	CheckURL("file:///./c:/", "file", "", "/c:/", ""); // "c:" folder on main disk
}

ADD_TEST(ParseQuery) {
	string value;
	Parameters properties;
	CHECK(!URL::ParseQuery("?", properties).count())
	CHECK(URL::ParseQuery("name1=value1&name2=value2", properties).count() == 2)
	DEBUG_CHECK(properties.getString("name1", value) && value == "value1");
	DEBUG_CHECK(properties.getString("name2", value) && value == "value2");
	properties.clear();
	CHECK(URL::ParseQuery("?name1=value1&name2=value2", properties).count() == 2)
	DEBUG_CHECK(properties.getString("name1", value) && value == "value1");
	DEBUG_CHECK(properties.getString("name2", value) && value == "value2");
	properties.clear();


	string test("name1=one%20space&name2=%22one double quotes%22&name3=percent:%25&name4=%27simple quotes%27");
	CHECK(URL::ParseQuery(test, properties).count() == 4); // test "count" + DecodeUrI
	DEBUG_CHECK(properties.getString("name1", value) && value == "one space");
	DEBUG_CHECK(properties.getString("name2", value) && value == "\"one double quotes\"");
	DEBUG_CHECK(properties.getString("name3", value) && value == "percent:%");
	DEBUG_CHECK(properties.getString("name4", value) && value == "'simple quotes'");
	CHECK(URL::ParseQuery("longquery://test;:/one%*$^/fin=value~", properties).count() == 5)
		DEBUG_CHECK(properties.getString("longquery://test;:/one%*$^/fin", value) && value == "value~");

	bool next(true);
	URL::ForEachParameter forEach([&next](const string& name, const char* value) { return next; });
	CHECK(URL::ParseQuery(test.c_str(), forEach) == 4); // test "string::pos" + DecodeUrI
	CHECK(URL::ParseQuery("name1=value1&name2=value2", 12, forEach) == 1);
	next = false;
	CHECK(URL::ParseQuery(test, forEach) == 1);
}


}

