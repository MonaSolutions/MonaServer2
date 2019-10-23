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
#include "Mona/PersistentData.h"
#include "Mona/FileSystem.h"

using namespace std;
using namespace Mona;

namespace PersistentDataTest {

static PersistentData	_Data;
static string			_Path;
static PersistentData::ForEach	_ForEach([](const string& path, const Packet& packet) {
	CHECK((path.compare("") == 0 && packet.size() == 5 && memcmp(packet.data(), EXPAND("salut")) == 0) ||
		(path.compare("/Test") == 0 && packet.size() == 8 && memcmp(packet.data(), EXPAND("aur\0voir")) == 0) ||
		(path.compare("/Sub") == 0 && packet.size() == 3 && memcmp(packet.data(), EXPAND("val")) == 0));
});


ADD_TEST(Load) {
	// create base of test
	Exception ex;
	FileSystem::MakeFolder(_Path.assign(FileSystem::GetHome("")).append(".MonaTests/"));
	_Data.load(ex, _Path, _ForEach);
	CHECK(!ex)
}


ADD_TEST(Add) {
	Exception ex;
	_Data.add("", Packet(EXPAND("salut")));
	_Data.add("Test", Packet(EXPAND("aurevoir")));
	_Data.add("Test", Packet(EXPAND("aur\0voir")));
	_Data.add("/Test/../Sub", Packet(EXPAND("val")));

	_Data.flush();
	CHECK(FileSystem::Exists(_Path));
	CHECK(FileSystem::Exists(_Path+"Test/"));
	CHECK(FileSystem::Exists(_Path+"Sub/"));
}

ADD_TEST(Reload) {
	// create base of test
	Exception ex;
	_Data.load(ex,_Path,_ForEach);
	CHECK(!ex);
}

ADD_TEST(Remove) {
	// create base of test
	Exception ex;
	_Data.remove("/Test/NoExists");
	_Data.remove("/Sub");
	_Data.remove("");
	_Data.flush();

	CHECK(!FileSystem::Exists(_Path + "Sub/"));

	_Data.remove("Test");
	_Data.remove("");
	_Data.flush();

	CHECK(!FileSystem::Exists(_Path));;
}

}
