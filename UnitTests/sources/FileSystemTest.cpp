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
#include "Mona/Util.h"

using namespace std;
using namespace Mona;

namespace FileSystemTest {

static string Home;
static string Path1;
static string Path2;
static size_t ExtPos;

ADD_TEST(Static) {
	CHECK(FileSystem::GetCurrentApp(Home) && FileSystem::GetCurrentApp());
	CHECK(!Home.empty() && FileSystem::IsAbsolute(Home));
	CHECK(FileSystem::IsAbsolute(FileSystem::GetCurrentDir()));
	CHECK(FileSystem::GetHome(Home) && FileSystem::GetHome());
	CHECK(!Home.empty() && FileSystem::IsAbsolute(Home));
}

#define CHECK_FILE(PATH,TYPE, PARENT,NAME,EXTENSION_POS) CHECK(FileSystem::GetFile(PATH,Path1,ExtPos, Path2)==TYPE && EXTENSION_POS==ExtPos && Path1==NAME && Path2==PARENT);


ADD_TEST(Resolve) {

	Path parent(Path::CurrentDir().parent());

	// ABSOLUTE
#if defined(_WIN32)
	CHECK_FILE("C:/", FileSystem::TYPE_FOLDER, "C:/", "", string::npos);
	CHECK_FILE("C:/..", FileSystem::TYPE_FILE, "C:/", "", string::npos);
	CHECK_FILE("C:/../", FileSystem::TYPE_FOLDER, "C:/", "", string::npos);
	CHECK_FILE("C:/../..", FileSystem::TYPE_FILE, "C:/", "", string::npos);
	CHECK_FILE("C:/../../", FileSystem::TYPE_FOLDER, "C:/", "", string::npos);
	CHECK_FILE("C:/../.", FileSystem::TYPE_FILE, "C:/", "", string::npos);
	CHECK_FILE("C:/.././", FileSystem::TYPE_FOLDER, "C:/", "", string::npos);
	CHECK_FILE("C:/../test/.", FileSystem::TYPE_FILE, "C:/../", "test", string::npos);
	CHECK_FILE("C://salut.txt", FileSystem::TYPE_FILE, "C:/", "salut.txt", 5);
#endif
	CHECK_FILE("/", FileSystem::TYPE_FOLDER, "/", "", string::npos);
	CHECK_FILE("/..", FileSystem::TYPE_FILE, "/", "", string::npos);
	CHECK_FILE("/../", FileSystem::TYPE_FOLDER, "/", "", string::npos);
	CHECK_FILE("/../..", FileSystem::TYPE_FILE, "/", "", string::npos);
	CHECK_FILE("/../../", FileSystem::TYPE_FOLDER, "/", "", string::npos);
	CHECK_FILE("/../.", FileSystem::TYPE_FILE, "/", "", string::npos);
	CHECK_FILE("/.././", FileSystem::TYPE_FOLDER, "/", "", string::npos);
	CHECK_FILE("/../test/.", FileSystem::TYPE_FILE, "/../", "test", string::npos);
	CHECK_FILE("/../test/./", FileSystem::TYPE_FOLDER, "/../", "test", string::npos);
	CHECK_FILE("//salut.txt", FileSystem::TYPE_FILE, "/", "salut.txt", 5);
	CHECK_FILE("//salut.txt/", FileSystem::TYPE_FOLDER, "/", "salut.txt", 5);

	CHECK_FILE("/te/main.cpp/t", FileSystem::TYPE_FILE, "/te/main.cpp/", "t", string::npos);
	CHECK_FILE("/te/main.cpp/..", FileSystem::TYPE_FILE, "/", "te", string::npos);
	CHECK_FILE("/te/main.cpp/../", FileSystem::TYPE_FOLDER, "/", "te", string::npos);
	CHECK_FILE("/te/main.cpp/...", FileSystem::TYPE_FILE, "/te/main.cpp/", "...", 2);
	CHECK_FILE("/te/main.cpp/.../", FileSystem::TYPE_FOLDER, "/te/main.cpp/", "...", 2);
	CHECK_FILE("/./main.cpp", FileSystem::TYPE_FILE, "/./", "main.cpp", 4);
	CHECK_FILE("/./main.cpp/.", FileSystem::TYPE_FILE, "/./", "main.cpp", 4);
	CHECK_FILE("/./main.cpp/./", FileSystem::TYPE_FOLDER, "/./", "main.cpp", 4);
	CHECK_FILE("/./main.cpp/test/..", FileSystem::TYPE_FILE, "/./", "main.cpp", 4);
	CHECK_FILE("/./main.cpp/test/../", FileSystem::TYPE_FOLDER, "/./", "main.cpp", 4);
	CHECK_FILE("/salut.txt", FileSystem::TYPE_FILE, "/", "salut.txt", 5);
	CHECK_FILE("C:/../salut", FileSystem::TYPE_FILE, "C:/../", "salut", string::npos);
	CHECK_FILE("C:/../salut", FileSystem::TYPE_FILE, "C:/../", "salut", string::npos);


	// RELATIVE
	CHECK_FILE("", FileSystem::TYPE_FOLDER, parent, Path::CurrentDir().name(), string::npos);
	CHECK_FILE("C:", FileSystem::TYPE_FILE, Path::CurrentDir(), "C:", string::npos);
	CHECK_FILE("salut.txt", FileSystem::TYPE_FILE, Path::CurrentDir(), "salut.txt", 5);
	CHECK_FILE("../", FileSystem::TYPE_FOLDER, parent.parent(), parent.name(), string::npos);
	CHECK_FILE("..", FileSystem::TYPE_FILE, parent.parent(), parent.name(), string::npos);
	CHECK_FILE("./", FileSystem::TYPE_FOLDER, parent, Path::CurrentDir().name(), string::npos);
	CHECK_FILE(".", FileSystem::TYPE_FILE, parent, Path::CurrentDir().name(), string::npos);
	CHECK_FILE("../salut", FileSystem::TYPE_FILE, "../", "salut", string::npos);


	CHECK(!FileSystem::IsAbsolute(""));
	CHECK(!FileSystem::IsAbsolute("."));
	CHECK(!FileSystem::IsAbsolute(".."));
	CHECK(FileSystem::IsAbsolute("/."));
	CHECK(FileSystem::IsAbsolute("/.."));
	CHECK(FileSystem::IsAbsolute("/"));
	CHECK(FileSystem::IsAbsolute("/f/"));
#if defined(_WIN32)
	CHECK(FileSystem::IsAbsolute("C:/"));
#else
	CHECK(!FileSystem::IsAbsolute("C:/"));
#endif
	CHECK(!FileSystem::IsAbsolute("C:"));


	CHECK(FileSystem::MakeAbsolute(Path1.assign(""))=="/");
	CHECK(FileSystem::MakeAbsolute(Path1.assign("."))=="/.");
	CHECK(FileSystem::MakeAbsolute(Path1.assign(".."))=="/..");
	CHECK(FileSystem::MakeAbsolute(Path1.assign("f"))=="/f");
	CHECK(FileSystem::MakeAbsolute(Path1.assign("/."))=="/.");
	CHECK(FileSystem::MakeAbsolute(Path1.assign("/.."))=="/..");
	CHECK(FileSystem::MakeAbsolute(Path1.assign("/"))=="/");
	CHECK(FileSystem::MakeAbsolute(Path1.assign("/f/"))=="/f/");
#if defined(_WIN32)
	CHECK(FileSystem::MakeAbsolute(Path1.assign("C:/"))=="C:/");
#else
	CHECK(FileSystem::MakeAbsolute(Path1.assign("C:/"))=="/C:/");
#endif
	CHECK(FileSystem::MakeAbsolute(Path1.assign("C:")) == "/C:");


	CHECK(FileSystem::MakeRelative(Path1.assign(""))=="");
	CHECK(FileSystem::MakeRelative(Path1.assign("."))==".");
	CHECK(FileSystem::MakeRelative(Path1.assign(".."))=="..");
	CHECK(FileSystem::MakeRelative(Path1.assign("f"))=="f");
	CHECK(FileSystem::MakeRelative(Path1.assign("/."))==".");
	CHECK(FileSystem::MakeRelative(Path1.assign("/.."))=="..");
	CHECK(FileSystem::MakeRelative(Path1.assign("/"))=="");
	CHECK(FileSystem::MakeRelative(Path1.assign("/f/"))=="f/");
#if defined(_WIN32)
	CHECK(FileSystem::MakeRelative(Path1.assign("C:/"))=="");
#else
	CHECK(FileSystem::MakeRelative(Path1.assign("C:/"))=="C:/");
#endif
	CHECK(FileSystem::MakeRelative(Path1.assign("C:")) == "C:");


	CHECK(FileSystem::Resolve(Path1.assign(Path::CurrentDir())) == Path::CurrentDir());
	CHECK(FileSystem::Resolve(Path1.assign("/")) == "/");
#if defined(_WIN32)
	CHECK(FileSystem::Resolve(Path1.assign("C:/")) == "C:/");
	CHECK(FileSystem::Resolve(Path1.assign("C:/..")) == "C:/."); // hard
#else
	CHECK(FileSystem::Resolve(Path1.assign("C:/")) == Path2.assign(Path::CurrentDir()).append("C:/"));
#endif
	CHECK(FileSystem::Resolve(Path1.assign(".")) == FileSystem::MakeFile(Path2.assign(Path::CurrentDir())));
	CHECK(FileSystem::Resolve(Path1.assign("./")) == Path::CurrentDir());
	CHECK(FileSystem::Resolve(Path1.assign("..")) == FileSystem::MakeFile(Path2.assign(parent)));
	CHECK(FileSystem::Resolve(Path1.assign("../")) == parent);
	CHECK(FileSystem::Resolve(Path1.assign("../..")) == FileSystem::MakeFile(Path2.assign(parent.parent())));
	CHECK(FileSystem::Resolve(Path1.assign("../../")) == parent.parent());
	CHECK(FileSystem::Resolve(Path1.assign("./.././../.")) == FileSystem::MakeFile(Path2.assign(parent.parent())));
	CHECK(FileSystem::Resolve(Path1.assign("./.././.././")) == parent.parent());
	CHECK(FileSystem::Resolve(Path1.assign("./test/../.")) == FileSystem::MakeFile(Path2.assign(Path::CurrentDir())));
	CHECK(FileSystem::Resolve(Path1.assign("./test/.././")) == Path::CurrentDir());
	CHECK(FileSystem::Resolve(Path1.assign("/..")) == "/."); // hard
	CHECK(FileSystem::Resolve(Path1.assign("/../")) == "/");
	CHECK(FileSystem::Resolve(Path1.assign("/../.")) == "/."); // hard
	CHECK(FileSystem::Resolve(Path1.assign("/.././")) == "/");
	CHECK(FileSystem::Resolve(Path1.assign("/test/../../.")) == "/."); // hard
	CHECK(FileSystem::Resolve(Path1.assign("/test/../.././")) == "/");
	CHECK(FileSystem::Resolve(Path1.assign("/test/../../hi.txt")) == "/hi.txt");
	CHECK(FileSystem::Resolve(Path1.assign("/test/../../hi")) == "/hi");
	CHECK(FileSystem::Resolve(Path1.assign("www")) == Path2.assign(Path::CurrentDir()).append("www"));
	CHECK(FileSystem::Resolve(Path1.assign("www/")) == Path2.assign(Path::CurrentDir()).append("www/"));
	CHECK(FileSystem::Resolve(Path1.assign("www/Peer")) == Path2.assign(Path::CurrentDir()).append("www/Peer"));
	CHECK(FileSystem::Resolve(Path1.assign("../Peer")) == Path2.assign(Path::CurrentDir().parent()).append("Peer"));

	CHECK(FileSystem::IsFolder(""));
	CHECK(FileSystem::IsFolder("/"));
	CHECK(!FileSystem::IsFolder("."));
	CHECK(FileSystem::IsFolder("./"));
	CHECK(!FileSystem::IsFolder(".."));
	CHECK(FileSystem::IsFolder("../"));
	CHECK(FileSystem::IsFolder("/./"));
	CHECK(!FileSystem::IsFolder("/."));
	CHECK(FileSystem::IsFolder("/../"));
	CHECK(!FileSystem::IsFolder("/.."));
	CHECK(FileSystem::IsFolder("/f/"));
	CHECK(FileSystem::IsFolder("C:/"));
	CHECK(!FileSystem::IsFolder("C:"));
	CHECK(FileSystem::IsFolder("\\"));
	CHECK(!FileSystem::IsFolder("\\."));
	CHECK(FileSystem::IsFolder("./C:/"));
	CHECK(!FileSystem::IsFolder("C:/."));

	CHECK(FileSystem::MakeFolder(Path1.assign("")).empty());
	CHECK(FileSystem::MakeFolder(Path1.assign("/"))=="/");
	CHECK(FileSystem::MakeFolder(Path1.assign("."))=="./");
	CHECK(FileSystem::MakeFolder(Path1.assign("./"))=="./");
	CHECK(FileSystem::MakeFolder(Path1.assign(".."))=="../");
	CHECK(FileSystem::MakeFolder(Path1.assign("../"))=="../");
	CHECK(FileSystem::MakeFolder(Path1.assign("/./"))=="/./");
	CHECK(FileSystem::MakeFolder(Path1.assign("/."))=="/./");
	CHECK(FileSystem::MakeFolder(Path1.assign("/../"))=="/../");
	CHECK(FileSystem::MakeFolder(Path1.assign("/.."))=="/../");
	CHECK(FileSystem::MakeFolder(Path1.assign("/f/"))=="/f/");
	CHECK(FileSystem::MakeFolder(Path1.assign("C:/"))=="C:/");
#if defined(_WIN32)
	CHECK(FileSystem::MakeFolder(Path1.assign("C:"))=="./C:/");
#else
	CHECK(FileSystem::MakeFolder(Path1.assign("C:"))=="C:/");
#endif
	CHECK(FileSystem::MakeFolder(Path1.assign("\\"))=="\\");
	CHECK(FileSystem::MakeFolder(Path1.assign("\\."))=="\\./");

	CHECK(FileSystem::MakeFile(Path1.assign(""))==".");
	CHECK(FileSystem::MakeFile(Path1.assign("/"))=="/.");
	CHECK(FileSystem::MakeFile(Path1.assign("."))==".");
	CHECK(FileSystem::MakeFile(Path1.assign("./"))==".");
	CHECK(FileSystem::MakeFile(Path1.assign(".."))=="..");
	CHECK(FileSystem::MakeFile(Path1.assign("../"))=="..");
	CHECK(FileSystem::MakeFile(Path1.assign("/./"))=="/.");
	CHECK(FileSystem::MakeFile(Path1.assign("/."))=="/.");
	CHECK(FileSystem::MakeFile(Path1.assign("/../"))=="/..");
	CHECK(FileSystem::MakeFile(Path1.assign("/.."))=="/..");
	CHECK(FileSystem::MakeFile(Path1.assign("/f/"))=="/f");
#if defined(_WIN32)
	CHECK(FileSystem::MakeFile(Path1.assign("C:/"))=="C:/.");
#else
	CHECK(FileSystem::MakeFile(Path1.assign("C:/"))=="C:");
#endif
	CHECK(FileSystem::MakeFile(Path1.assign("C:"))=="C:");
	CHECK(FileSystem::MakeFile(Path1.assign("\\"))=="/.");
	CHECK(FileSystem::MakeFile(Path1.assign("\\."))=="\\.");

	CHECK(FileSystem::GetParent("",Path1) == parent);
	CHECK(FileSystem::GetParent("C:",Path1) == Path::CurrentDir());
#if defined(_WIN32)
	CHECK(FileSystem::GetParent("C:/",Path1) == "C:/");
#else
	CHECK(FileSystem::GetParent("C:/",Path1) == Path::CurrentDir());
#endif
	CHECK(FileSystem::GetParent("f",Path1) == Path::CurrentDir());
	CHECK(FileSystem::GetParent("f/",Path1) == Path::CurrentDir());
	CHECK(FileSystem::GetParent("/f",Path1) == "/");
	CHECK(FileSystem::GetParent("/f/",Path1) == "/");
	CHECK(FileSystem::GetParent(".",Path1) == parent);
	CHECK(FileSystem::GetParent("./",Path1) == parent);
	CHECK(FileSystem::GetParent("..",Path1) == parent.parent());
	CHECK(FileSystem::GetParent("../",Path1) == parent.parent());
	CHECK(FileSystem::GetParent("...",Path1) == Path::CurrentDir());
	CHECK(FileSystem::GetParent(".../",Path1) == Path::CurrentDir());
	CHECK(FileSystem::GetParent("/.",Path1) == "/");
	CHECK(FileSystem::GetParent("/./",Path1) == "/");
	CHECK(FileSystem::GetParent("/..",Path1) == "/");
	CHECK(FileSystem::GetParent("/../",Path1) == "/");
	CHECK(FileSystem::GetParent("/...",Path1) == "/");
	CHECK(FileSystem::GetParent("/.../",Path1) == "/");
	CHECK(FileSystem::GetParent("/.././../test/salut/../..",Path1) == "/");
	

	CHECK(FileSystem::GetExtension("",Path1) == Path::CurrentDir().extension());
	CHECK(FileSystem::GetExtension("f",Path1).empty());
	CHECK(FileSystem::GetExtension(".",Path1) == Path::CurrentDir().extension());
	CHECK(FileSystem::GetExtension("..",Path1) == parent.extension());
	CHECK(FileSystem::GetExtension("/.",Path1).empty());
	CHECK(FileSystem::GetExtension("/..",Path1).empty());
	CHECK(FileSystem::GetExtension("/...",Path1).empty());
	CHECK(FileSystem::GetExtension("f.e",Path1)=="e");
	CHECK(FileSystem::GetExtension(".e",Path1)=="e");
	CHECK(FileSystem::GetExtension("..e",Path1)=="e");
	CHECK(FileSystem::GetExtension("/.e",Path1)=="e");
	CHECK(FileSystem::GetExtension("/..e",Path1)=="e");
	CHECK(FileSystem::GetExtension("f.e/",Path1)=="e");
	CHECK(FileSystem::GetExtension(".e/",Path1)=="e");
	CHECK(FileSystem::GetExtension("..e/",Path1)=="e");
	CHECK(FileSystem::GetExtension("/.e/",Path1)=="e");
	CHECK(FileSystem::GetExtension("/..e/",Path1)=="e");
	CHECK(FileSystem::GetExtension("C:/",Path1).empty());

	CHECK(FileSystem::GetName("",Path1) == Path::CurrentDir().name());
	CHECK(FileSystem::GetName("f",Path1)=="f");
	CHECK(FileSystem::GetName(".",Path1) == Path::CurrentDir().name());
	CHECK(FileSystem::GetName("..",Path1) == parent.name());
	CHECK(FileSystem::GetName("/.",Path1).empty());
	CHECK(FileSystem::GetName("/..",Path1).empty());
	CHECK(FileSystem::GetName("/...",Path1)=="...");
	CHECK(FileSystem::GetName("f.e",Path1)=="f.e");
	CHECK(FileSystem::GetName(".e",Path1)==".e");
	CHECK(FileSystem::GetName("..e",Path1)=="..e");
	CHECK(FileSystem::GetName("/.e",Path1)==".e");
	CHECK(FileSystem::GetName("/..e",Path1)=="..e");
	CHECK(FileSystem::GetName("f.e/",Path1)=="f.e");
	CHECK(FileSystem::GetName(".e/",Path1)==".e");
	CHECK(FileSystem::GetName("..e/",Path1)=="..e");
	CHECK(FileSystem::GetName("/.e/",Path1)==".e");
	CHECK(FileSystem::GetName("/..e/",Path1)=="..e");
#if defined(_WIN32)
	CHECK(FileSystem::GetName("C:/",Path1).empty());
#else
	CHECK(FileSystem::GetName("C:/",Path1)=="C:");
#endif

	CHECK(FileSystem::GetBaseName("",Path1) == Path::CurrentDir().baseName());
	CHECK(FileSystem::GetBaseName("f",Path1)=="f");
	CHECK(FileSystem::GetBaseName(".",Path1) == Path::CurrentDir().baseName());
	CHECK(FileSystem::GetBaseName("..",Path1) == parent.baseName());
	CHECK(FileSystem::GetBaseName("/.",Path1).empty());
	CHECK(FileSystem::GetBaseName("/..",Path1).empty());
	CHECK(FileSystem::GetBaseName("/...",Path1)=="..");
	CHECK(FileSystem::GetBaseName("f.e",Path1)=="f");
	CHECK(FileSystem::GetBaseName(".e",Path1).empty());
	CHECK(FileSystem::GetBaseName("..e",Path1)==".");
	CHECK(FileSystem::GetBaseName("/.e",Path1).empty());
	CHECK(FileSystem::GetBaseName("/..e",Path1)==".");
	CHECK(FileSystem::GetBaseName("f.e/",Path1)=="f");
	CHECK(FileSystem::GetBaseName(".e/",Path1).empty());
	CHECK(FileSystem::GetBaseName("..e/",Path1)==".");
	CHECK(FileSystem::GetBaseName("/.e/",Path1).empty());
	CHECK(FileSystem::GetBaseName("/..e/",Path1)==".");
#if defined(_WIN32)
	CHECK(FileSystem::GetBaseName("C:/",Path1).empty());
#else
	CHECK(FileSystem::GetName("C:/",Path1)=="C:");
#endif

#if defined(_WIN32)
	if (!FileSystem::Find(Path1.assign("notepad.exe")))
		CHECK(FileSystem::Exists("/Windows/System32/notepad.exe"));
#else
	if (!FileSystem::Find(Path1.assign("ls")))
		CHECK(FileSystem::Exists("/bin/ls"));
#endif

}

ADD_TEST(Creation) {
	Path1.assign(Home).append(".MonaFileSystemTest/");
	Path2.assign(Path1).append("/SubFolder/Folder/");

	Exception ex;
	CHECK(FileSystem::CreateDirectory(ex,Path2,FileSystem::MODE_HEAVY) && !ex);
	CHECK(FileSystem::Exists(Path2));

	CHECK(!FileSystem::Delete(ex,Path1) && ex);
	CHECK(FileSystem::Exists(Path1));
	ex = NULL;

	CHECK(FileSystem::Delete(ex,Path1,FileSystem::MODE_HEAVY) && !ex);
	CHECK(!FileSystem::Exists(Path1));

	CHECK(FileSystem::CreateDirectory(ex,Path1) && !ex);
	CHECK(FileSystem::Exists(Path1));
	CHECK(FileSystem::MakeFile(Path1).back() == 't');
	CHECK(!FileSystem::Exists(Path1));

	Path2.assign(Path1).append("s/");
	if (FileSystem::Exists(Path2))
		CHECK(FileSystem::Delete(ex,Path2,FileSystem::MODE_HEAVY) && !ex);
	CHECK(FileSystem::Rename(Path1, Path2));
	CHECK(!FileSystem::Exists(Path1));
	CHECK(FileSystem::Exists(Path2));

	CHECK(File(Path2.append("temp.mona"), File::MODE_WRITE).write(ex, EXPAND("t")) && !ex);

	CHECK(FileSystem::Delete(ex,FileSystem::MakeFile(Path2)) && !ex);
}

ADD_TEST(Attributes) {
	FileSystem::Attributes attributes;
	CHECK(!FileSystem::GetAttributes(Path1.assign(Home).append(".MonaFileSystemTests"), attributes) && !attributes && !attributes.lastChange);
	CHECK(FileSystem::GetAttributes(Path1.append("/"), attributes) && attributes && attributes.lastChange);

	Exception ex;
#if defined(_WIN32)
	String::Append(Path1, L"Приве́т नमस्ते שָׁלוֹם");
#else
	String::Append(Path1, "Приве́т नमस्ते שָׁלוֹם");
#endif
	
	// file
	CHECK(File(Path1, File::MODE_WRITE).write(ex, EXPAND("t")) && !ex);

	CHECK(FileSystem::GetAttributes(Path1, attributes) && attributes && attributes.size==1)
	CHECK(FileSystem::Delete(ex, Path1) && !ex);
	CHECK(!FileSystem::GetAttributes(Path1, attributes) && !attributes && attributes.size==0)

	// folder
	Path1 += '/';
	if (FileSystem::Exists(Path1))
		CHECK(FileSystem::Delete(ex, Path1) && !ex);
	CHECK(FileSystem::CreateDirectory(ex,Path1) && !ex);
	CHECK(FileSystem::Exists(Path1))
	CHECK(FileSystem::Delete(ex, Path1) && !ex);
	CHECK(!FileSystem::Exists(Path1))
}

ADD_TEST(Deletion) {
	Exception ex;
	CHECK(FileSystem::Delete(ex,Path1.assign(Home).append(".MonaFileSystemTests"),FileSystem::MODE_HEAVY) && !ex);
}

}

