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

#include "Script.h"
#include "Mona/Path.h"


using namespace std;

namespace Mona {

static int __tostring(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		SCRIPT_WRITE_STRING(path)
	SCRIPT_CALLBACK_RETURN
}
static int name(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		const char* value = SCRIPT_READ_STRING(NULL);
		if(value)
			path.setName(value);
		SCRIPT_WRITE_STRING(path.name())
	SCRIPT_CALLBACK_RETURN
}
static int baseName(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		const char* value = SCRIPT_READ_STRING(NULL);
		if(value)
			path.setBaseName(value);
		SCRIPT_WRITE_STRING(path.name())
	SCRIPT_CALLBACK_RETURN
}
static int extension(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		const char* value = SCRIPT_READ_STRING(NULL);
		if (value)
			path.setExtension(value);
		SCRIPT_WRITE_STRING(path.name())
	SCRIPT_CALLBACK_RETURN
}
static int parent(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		SCRIPT_WRITE_STRING(path.parent())
	SCRIPT_CALLBACK_RETURN
}
static int isFolder(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		SCRIPT_WRITE_BOOLEAN(path.isFolder())
	SCRIPT_CALLBACK_RETURN
}
static int isAbsolute(lua_State *pState) {
	SCRIPT_CALLBACK(Path, path)
		SCRIPT_WRITE_BOOLEAN(path.isAbsolute())
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, const Path& path) {
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("__tostring", &__tostring);
		SCRIPT_DEFINE_FUNCTION("name", &name);
		SCRIPT_DEFINE_FUNCTION("baseName", &baseName);
		SCRIPT_DEFINE_FUNCTION("extension", &extension);
		SCRIPT_DEFINE_FUNCTION("parent", &parent);
		SCRIPT_DEFINE_FUNCTION("isFolder", &isFolder);
		SCRIPT_DEFINE_FUNCTION("isAbsolute", &isAbsolute);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, const Path& path) {

}

}

