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
#include "Mona/File.h"

using namespace std;
using namespace Mona;

namespace PathTest {

ADD_TEST(Getters) {

	Path path;
	CHECK(!path);
	string value("/path/file.txt");
	path = value;
	CHECK(path);
	CHECK(path.length()==value.size());
	CHECK(path.isAbsolute());
	CHECK(path.name() == "file.txt");
	CHECK(path.baseName() == "file");
	CHECK(path.extension() == "txt");
	CHECK(path.parent() == "/path/");
	CHECK(!path.isFolder());

	CHECK(path.search() == "?");
	path.set("/path/file.txt?salut");
	CHECK(path.name() == "file.txt?salut");
	CHECK(path.baseName() == "file");
	CHECK(path.extension() == "txt?salut");
	CHECK(path.search() == "?salut");
	CHECK(path.name() == "file.txt");
	CHECK(path.baseName() == "file");
	CHECK(path.extension() == "txt");

	path.set("/path?url=/file.txt?salut");
	CHECK(path.name() == "file.txt?salut");
	CHECK(path.baseName() == "file");
	CHECK(path.extension() == "txt?salut");
	CHECK(path.search() == "?url=/file.txt?salut");
	CHECK(path.name() == "path");
	CHECK(path.baseName() == "path");
	CHECK(path.extension() == "");
}

ADD_TEST(Setters) {

	Path path("home/name");
	CHECK(path.parent()=="home/" && path.name() == "name" && !path.isFolder());
	CHECK(path.setName("test.txt") && path=="home/test.txt");
	CHECK(path.setExtension("t.") && path.name()=="test.");
	CHECK(!path.setBaseName("") && path.name()=="test.");
	Path file(MAKE_FILE(path));
	CHECK(!file.isFolder() && file == "home/test.");
	Path directory(MAKE_FOLDER(path));
	CHECK(directory.isFolder() && directory == "home/test./");

	Path absolute(MAKE_ABSOLUTE(path.parent()));
	CHECK(absolute.isAbsolute() && absolute.parent()=="/" && absolute=="/home/");
	Path relative(MAKE_RELATIVE(absolute));
	CHECK(!relative.isAbsolute() && relative.parent().empty() && relative=="home/");
	Path resolved(RESOLVE("./../////"));
	CHECK(resolved == Path::CurrentDir().parent());
}

ADD_TEST(Attributes) {
	
	Exception ex;
	CHECK(Path::CurrentApp().exists());
	Path path("temp.mona");
	CHECK(File(path, File::MODE_WRITE).write(ex, EXPAND("t")) && !ex);
	CHECK(path.exists(true));
	CHECK(path.lastChange());
	CHECK(path.size()==1);
	CHECK(FileSystem::Delete(ex, path) && !ex);
	CHECK(path.exists());
	CHECK(path.lastChange());
	CHECK(!path.exists(true));
	CHECK(!path.lastChange());

}

}
