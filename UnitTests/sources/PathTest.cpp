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
#include "Mona/File.h"

using namespace std;
using namespace Mona;

namespace PathTest {

ADD_TEST(Getters) {

	Path path;
	CHECK(!path);
	path.set("");
	CHECK(path);
	CHECK(!path.length());
	CHECK(!path.isAbsolute());
	CHECK(path.name() == Path::CurrentDir().name());
	CHECK(path.baseName() == Path::CurrentDir().baseName());
	CHECK(path.extension() == Path::CurrentDir().extension());
	CHECK(path.parent() == Path::CurrentDir().parent());
	CHECK(path.isFolder() == Path::CurrentDir().isFolder());
	
}

ADD_TEST(Setters) {

	Path path("home/name");
	CHECK(path.parent()=="home/" && path.name() == "name" && !path.isFolder());
	CHECK(path.setName("test.txt") && path=="home/test.txt");
	CHECK(path.setExtension("t.") && path.name()=="test.");
	CHECK(!path.setBaseName("") && path.name()=="test.");
	Path file(MAKE_FILE(path));
	CHECK(!file.isFolder() && file == "home/test.");
	Path folder(MAKE_FOLDER(path));
	CHECK(folder.isFolder() && folder == "home/test./");

	Path absolute(MAKE_ABSOLUTE(path.parent()));
	CHECK(absolute.isAbsolute() && absolute.parent()=="/" && absolute=="/home/");
	Path relative(MAKE_RELATIVE(absolute));
	CHECK(!relative.isAbsolute() && relative.parent()==Path::CurrentDir() && relative=="home/");
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
