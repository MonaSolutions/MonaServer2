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

using namespace std;

namespace Mona {

thread_local int	Script::_AddTop = 0;
thread_local UInt32	Script::_RemoveTop = 0;

static int LoadFile(lua_State *pState) {
	// 1 - name
	string file;
	if (!Script::Resolve(pState, lua_tostring(pState, 1), file))
		return 0;
	if (FileSystem::IsFolder(file)) {
		SCRIPT_BEGIN(pState)
			SCRIPT_ERROR("Impossible to load ", file, " folder");
		SCRIPT_END
		return 0;
	}
	int top = lua_gettop(pState);
	lua_pushvalue(pState, lua_upvalueindex(1)); // lua loadfile
	lua_pushlstring(pState, file.data(), file.size());
	lua_call(pState, 1, LUA_MULTRET); // lua_call rather lua_pcall because is called from LUA (callback, not CPP) => stop the script proprely without execute the rest (useless)
	return lua_gettop(pState) - top;
}

static int DoFile(lua_State *pState) {
	// 1 - name
	int top = lua_gettop(pState);
	if (LoadFile(pState))
		lua_call(pState, 0, LUA_MULTRET);  // lua_call rather lua_pcall because is called from LUA (callback, not CPP) => stop the script proprely without execute the rest (useless)
	return lua_gettop(pState) - top;
}

static int Require(lua_State *pState) {
	// 1 - name
	string file;
	if (!Script::Resolve(pState, lua_tostring(pState, 1), file, false))
		return 0;
	if (FileSystem::IsFolder(file)) {
		SCRIPT_BEGIN(pState)
			SCRIPT_ERROR("Impossible to include ", file, " folder");
		SCRIPT_END
		return 0;
	}
	// replace / by . (LUA works like that for modules)
	size_t found = file.find_first_of("\\/");
	while (found != std::string::npos) {
		file[found] = '.';
		found = file.find_first_of("\\/", found + 1);
	}
	int top = lua_gettop(pState);
	lua_pushvalue(pState, lua_upvalueindex(1)); // lua require
	lua_pushlstring(pState, file.data(), file.size());
	lua_call(pState, 1, LUA_MULTRET); // lua_call rather lua_pcall because is called from LUA (callback, not CPP) => stop the script proprely without execute the rest (useless)
	return lua_gettop(pState) - top;
}

// fix luajit which doesn't support __pairs methametods without lua 5.2 activation
int Pairs(lua_State* pState) {
	// 1 table
	if (lua_getmetatable(pState, 1)) {
		lua_pushliteral(pState, "__pairs");
		lua_rawget(pState, -2);
		lua_replace(pState, -2); // replace metatable!
		if (lua_isfunction(pState, -1)) {
			lua_pushvalue(pState, 1);
			lua_call(pState, 1, 3); // lua_call rather lua_pcall because is called from LUA (callback, not CPP) => stop the script proprely without execute the rest (useless)
			return 3;
		}
		lua_pop(pState, 1); // remove __pairs value
	}
	lua_getglobal(pState, "next");
	lua_pushvalue(pState, 1);
	lua_pushnil(pState);
	return 3;
}

// Change 'getmetatable' when __metatable is set to flat __index + __metatable array (forbid __index + __metatable table and allows to list meta methods)
int GetMetatable(lua_State* pState) {
	// 1 table
	// [2 table response]
	lua_settop(pState, 2);
	if (!lua_istable(pState, 2)) {
		lua_newtable(pState);
		lua_replace(pState, 2);
	}

	if (!lua_getmetatable(pState, 1))
		return 0;

	lua_pushliteral(pState, "__metatable");
	lua_rawget(pState, 3);
	if (!lua_istable(pState, -1)) {
		if (lua_isnil(pState, -1)) {
			// normal behavior!
			lua_pop(pState, 1); // remove __metatable table
			return 1; // return metatable
		}
		lua_pop(pState, 1); // remove __metatable table
	}

	for(;;) {
		lua_pushliteral(pState, "__index");
		lua_rawget(pState, 3);
		if (!lua_istable(pState, -1)) {
			if (lua_isfunction(pState, -1)) {
				lua_pcall(pState, 0, 1, 0);
				if (!lua_istable(pState, -1))
					lua_pop(pState, 1);
			} else
				lua_pop(pState, 1); // remove __index table
			break;
		}
		if (!lua_getmetatable(pState, -1))
			break;
		lua_replace(pState, 3);
	}

	// flat __metatable + __index
	while (lua_gettop(pState) > 3) {
		lua_pushnil(pState);  // first key 
		while (lua_next(pState, -2)) {
			// uses 'key' (at index -2) and 'value' (at index -1) 
			// remove the raw!
			lua_pushvalue(pState, -2); // duplicate key
			lua_insert(pState, -2);
			lua_rawset(pState, 2);
		}
		lua_pop(pState, 1);
	};
	lua_pop(pState, 1); // remove metatable!
	return 1;
}

void Script::NewMetatable(lua_State *pState) {
	lua_newtable(pState);
	lua_pushliteral(pState, "__metatable");
	lua_pushliteral(pState, "Prohibited access to metatable of mona object");
	lua_rawset(pState, -3);
}

bool Script::Resolve(lua_State *pState, const char* value, string& file, bool withExtension) {
	SCRIPT_BEGIN(pState)
		if (!value) {
			SCRIPT_ERROR("Takes a path string as first parameter");
			return false;
		}
		Path path;
		if (!FileSystem::IsAbsolute(value)) {
			thread_local lua_Debug	Debug;
			if (lua_getstack(pState, 1, &Debug) && lua_getinfo(pState, "S", &Debug) && strlen(Debug.short_src) && strcmp(Debug.short_src, "[C]"))
				path.set(Debug.short_src);
			path.set(RESOLVE(String(path.parent(), value)));
			// make it relatives and remove extension if withExtension=false
			if (path.parent().length() <= Path::CurrentDir().length()) {
				file = withExtension ? path.name() : path.baseName();
				if (path.isFolder())
					file += '/';
			} else
				file.assign(path, Path::CurrentDir().length(), path.length() - Path::CurrentDir().length() - ((withExtension || path.extension().empty()) ? 0 : (path.extension().size() + 1)));
		} else
			path.set(RESOLVE(value));
	SCRIPT_END
	return true;
}

struct LogWriter : String::Writer<std::string>, virtual Object {
	LogWriter(lua_State* pState) : _pState(pState), _top(lua_gettop(pState)), _args(0) {}
	bool write(std::string& out) {
		if(_args++ < _top) {
			Script::PushString(_pState, _args);
			size_t len;
			const char* text = lua_tolstring(_pState, -1, &len);
			out.append(text, len);
			lua_remove(_pState, -1);
		}
		return _args < _top;
	}
private:
	lua_State*  _pState;
	int			_top;
	int			_args;
};


static int Fatal(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_FATAL, false, writer);
	SCRIPT_END
	return 0;
}
static int Error(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_ERROR, false, writer);
	SCRIPT_END
	return 0;
}
static int Warn(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_WARN, false, writer);
	SCRIPT_END
	return 0;
}
static int Note(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_NOTE, false, writer);
	SCRIPT_END
	return 0;
}
static int Info(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_INFO, false, writer);
	SCRIPT_END
	return 0;
}
static int Debug(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_DEBUG, false, writer);
	SCRIPT_END
	return 0;
}
static int Trace(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		SCRIPT_LOG(LOG_TRACE, false, writer);
	SCRIPT_END
	return 0;
}
static int Log(lua_State *pState) {
	LogWriter writer(pState);
	SCRIPT_BEGIN(pState)
		UInt8 level = (UInt8)max(min(UInt8(LOG_TRACE), lua_tointeger(pState, 1)), LOG_ERROR);
		SCRIPT_LOG(level ? level : LOG_DEFAULT, false, writer);
	SCRIPT_END
	return 0;
}

const char* Script::LastError(lua_State *pState) {
	size_t size;
	const char* error = lua_tolstring(pState, -1, &size);
	if(!error)
		return "No error";
	lua_pop(pState, 1);
	return size ? error : "Unknown error";
}

lua_State* Script::CreateState() {
	lua_State* pState = luaL_newstate();
	luaL_openlibs(pState);
	lua_atpanic(pState,&Fatal);

	lua_pushcfunction(pState,&Error);
	lua_setglobal(pState,"ERROR");
	lua_pushcfunction(pState,&Warn);
	lua_setglobal(pState,"WARN");
	lua_pushcfunction(pState,&Note);
	lua_setglobal(pState,"NOTE");
	lua_pushcfunction(pState,&Info);
	lua_setglobal(pState,"INFO");
	lua_pushcfunction(pState,&Debug);
	lua_setglobal(pState,"DEBUG");
	lua_pushcfunction(pState, &Trace);
	lua_setglobal(pState,"TRACE");
	lua_pushcfunction(pState, &Mona::Log);
	lua_setglobal(pState, "LOG");
	lua_pushinteger(pState, LOG_ERROR);
	lua_setglobal(pState, "LOG_ERROR");
	lua_pushinteger(pState, LOG_WARN);
	lua_setglobal(pState, "LOG_WARN");
	lua_pushinteger(pState, LOG_NOTE);
	lua_setglobal(pState, "LOG_NOTE");
	lua_pushinteger(pState, LOG_INFO);
	lua_setglobal(pState, "LOG_INFO");
	lua_pushinteger(pState, LOG_DEBUG);
	lua_setglobal(pState, "LOG_DEBUG");
	lua_pushinteger(pState, LOG_TRACE);
	lua_setglobal(pState, "LOG_TRACE");
	lua_pushcfunction(pState, &Pairs);
	lua_setglobal(pState, "pairs");
	lua_pushcfunction(pState, &GetMetatable);
	lua_setglobal(pState, "getmetatable");

	// set require, loadFile, dofile to fix relative path!
	lua_getglobal(pState, "require");
	lua_pushcclosure(pState, &Require, 1);
	lua_setglobal(pState, "require");
	lua_getglobal(pState, "loadfile");
	lua_pushvalue(pState, -1);
	lua_pushcclosure(pState, &LoadFile, 1);
	lua_setglobal(pState, "loadfile");
	lua_pushcclosure(pState, &DoFile, 1);
	lua_setglobal(pState, "dofile");
	
	return pState;
}

void Script::CloseState(lua_State* pState) {
	if(pState)
		lua_close(pState);
}

void Script::PushValue(lua_State* pState, const void* value, size_t size) {
	switch (size) {
		case 4:
			if (String::ICompare(STR value, "true") == 0)
				return lua_pushboolean(pState, 1);
			if (String::ICompare(STR value, "null") == 0)
				return lua_pushnil(pState);
			break;
		case 5:
			if (String::ICompare(STR value, "false") == 0)
				return lua_pushboolean(pState, 0);
		default:;
	}
	lua_pushlstring(pState, STR value, size); // in LUA number or string are assimilable!
}
void Script::PushValue(lua_State* pState, const Parameters& parameters, const string& prefix) {
	lua_newtable(pState);
	for (const auto& it : parameters.range(prefix)) {
		lua_pushlstring(pState, it.first.data() + prefix.size(), it.first.size() - prefix.size());
		PushValue(pState, it.second.data(), it.second.size());
		lua_rawset(pState, -3);
	}
}


void Script::PushString(lua_State* pState, int index) {
	int type = lua_type(pState, index);
	switch (type) {
		case LUA_TTABLE:
			if (!lua_getmetatable(pState, index))
				break; // display table address!
			lua_pushliteral(pState, "__tostring");
			lua_rawget(pState, -2);
			lua_replace(pState, -2); // remove metatable
			if (!lua_isnil(pState, -1)) {
				if(lua_isfunction(pState, -1)) {
					lua_pushvalue(pState, index);
					SCRIPT_BEGIN(pState)
						if (lua_pcall(pState, 1, 1, 0) == 0) { // consume arg + function
							if (lua_isstring(pState, -1))
								return;
							SCRIPT_ERROR("__tostring must return a string");
						} else
							SCRIPT_ERROR(LastError(pState));
					SCRIPT_END
				} else {
					SCRIPT_BEGIN(pState)
						SCRIPT_ERROR("call a no-function __tostring ", lua_typename(pState, lua_type(pState, -1)));
					SCRIPT_END
				}
			}
			lua_pop(pState, 1); // remove __tostring or no-string value
			break;
		case LUA_TBOOLEAN:
			return lua_pushstring(pState, lua_toboolean(pState,index) ? "true" : "false");
		case LUA_TNIL:
			return lua_pushlstring(pState, EXPAND("null"));
		case LUA_TNUMBER:
		case LUA_TSTRING:
			return lua_pushvalue(pState, index);
		default:;
	}
	String str(lua_typename(pState, type), '_', lua_topointer(pState, index));
	lua_pushlstring(pState, str.data(), str.size());
}

int Script::Concat(lua_State *pState) {
	PushString(pState, 1);
	PushString(pState, 2);
	lua_concat(pState, 2);
	return 1;
}

void Script::Test(lua_State* pState) {
	int i;
	int top = lua_gettop(pState);

	printf("total in stack %d\n", top);

	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(pState, i);
		switch (t) {
			case LUA_TNONE:  /* strings */
				printf("none\n");
				break;
			case LUA_TNIL:  /* strings */
				printf("nil\n");
				break;
			case LUA_TSTRING:  /* strings */
				printf("string: '%s'\n", lua_tostring(pState, i));
				break;
			case LUA_TBOOLEAN:  /* booleans */
				printf("boolean: %s\n", lua_toboolean(pState, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:  /* numbers */
				printf("number: %g\n", lua_tonumber(pState, i));
				break;
			default:  /* other values */
				PushString(pState, i);
				printf("%s: %s\n", lua_typename(pState, t), lua_tostring(pState, -1));
				lua_pop(pState, 1);
				break;
		}
	}
	printf("\n");  /* end the listing */

}


} // namespace Mona
