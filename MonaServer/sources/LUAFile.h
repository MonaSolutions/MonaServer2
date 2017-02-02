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

#pragma once

#include "Script.h"
#include "Mona/File.h"

class LUAFile {
public:
	static void Init(lua_State *pState, Mona::File& file) {}
	static void	Clear(lua_State* pState, Mona::File& file) {}
	static void	Delete(lua_State* pState, Mona::File& file) {delete &file;}
	static int	Index(lua_State *pState);
	static int	IndexConst(lua_State *pState);

private:
	static int	Size(lua_State *pState);
	static int	Exists(lua_State *pState);
	static int	LastModified(lua_State *pState);

	static int	SetPath(lua_State *pState);
	static int	SetParent(lua_State *pState);
	static int	SetName(lua_State *pState);
	static int	SetBaseName(lua_State *pState);
	static int	SetExtension(lua_State *pState);
	static int	MakeFolder(lua_State *pState);
	static int	MakeFile(lua_State *pState);
	static int	MakeAbsolute(lua_State *pState);
	static int	MakeRelative(lua_State *pState);
	static int	Resolve(lua_State *pState);
};
