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

namespace Mona {

template<typename VectorType>
struct LUAVector : virtual Static {
	
	static int Pairs(lua_State *pState) {
		SCRIPT_CALLBACK(VectorType, vector);
			lua_pushcfunction(pState, &Next);
			lua_pushvalue(pState, 1);
		SCRIPT_CALLBACK_RETURN;
	}

	static int Index(lua_State *pState) {
		if (!lua_gettop(pState)) {
			SCRIPT_TABLE_BEGIN(0)
				SCRIPT_DEFINE_STRING("len", "length()");
			SCRIPT_TABLE_END
			return 1;
		}
		SCRIPT_CALLBACK(VectorType, vector)
			int arg = SCRIPT_READ_NEXT(1);
			int isNum;
			UInt32 index = lua_tointegerx(pState, arg, &isNum);
			if (isNum) {
				if (index > 0) { // else must returns nil (LUA array index starts to 1)
					if(index<=vector.size())
						PushValue(pState, vector, --index);
					else
						SCRIPT_ERROR("out of 1-", vector.size(), ' ', typeof<VectorType>(), " range");
				}
			} else if(const char* name = lua_tostring(pState, arg) && strcmp(name, "len") == 0)
				SCRIPT_WRITE_FUNCTION(&Len)
			else
				SCRIPT_ERROR(ex.set<Ex::Application::Argument>("Require a numeric index argument"));
		SCRIPT_CALLBACK_RETURN
	}

private:
	static int Next(lua_State* pState) {
		SCRIPT_CALLBACK(VectorType, vector);
			UInt32 index = SCRIPT_READ_UINT32(1);
			if (index && index <= vector.size()) {
				SCRIPT_WRITE_INT(index + 1); // next index
				PushValue(pState, vector, index-1); // value
			}
		SCRIPT_CALLBACK_RETURN;
	}

	static int Len(lua_State* pState) {
		SCRIPT_CALLBACK(VectorType, vector);
			SCRIPT_WRITE_INT(vector.size());
		SCRIPT_CALLBACK_RETURN;
	}

	static void PushValue(lua_State *pState, const VectorType& vector, UInt32 index);

};

}
