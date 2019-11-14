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

#include "LUAMap.h"

using namespace std;

namespace Mona {

template<> void Script::ObjInit(lua_State *pState, Object& object) {
	struct Mapper : virtual Object {
		Mapper(Object& object, lua_State* pState) : _pState(pState), _object(object) {}

		UInt32 size(string* pPrefix) const {
			getTable();
			UInt32 size = 0;
			lua_pushnil(_pState);  // first key 
			while (lua_next(_pState, -2)) {
				// uses 'key' (at index -2) and 'value' (at index -1) 
				if (!pPrefix || pPrefix->compare(0, std::string::npos, lua_tostring(_pState, -2), pPrefix->size()) == 0)
					++size;
				lua_pop(_pState, 1);
			}
			lua_pop(_pState, 1); // remove table
			return size;
		}

		bool get(const string& key) {
			getTable();
			lua_pushlstring(_pState, key.data(), key.size());
			lua_rawget(_pState, -2);
			lua_replace(_pState, -2); // remove table
			if(!lua_isnil(_pState, -1))
				return true;
			lua_pop(_pState, 1); // remove object
			return false;
		}

		bool next(const char* key, std::string* pPrefix) {
			getTable();
			if (key)
				lua_pushstring(_pState, key);
			else
				lua_pushnil(_pState);
			while (lua_next(_pState, -2)) {
				// uses 'key' (at index -2) and 'value' (at index -1) 
				const char* k = lua_tostring(_pState, -2);
				if (!pPrefix || pPrefix->compare(0, std::string::npos, k, pPrefix->size()) == 0) {
					lua_remove(_pState, -3); // remove object
					return true;
				}
				lua_pop(_pState, 1);
			}
			lua_pop(_pState, 1); // remove table
			return false;
		}

		Int8 erase(const string& key) {
			getTable();
			lua_pushlstring(_pState, key.data(), key.size());
			lua_pushnil(_pState);
			lua_rawset(_pState, -3);
			lua_pop(_pState, 1); // remove table
			return 1; // impossible to determinate if the key excited without losing performance with a pre-find!
		}
		int clear(string* pPrefix) {
			getTable();
			lua_pushnil(_pState);
			int count = 0;
			while (lua_next(_pState, -2)) {
				// uses 'key' (at index -2) and 'value' (at index -1) 
				if (!pPrefix || pPrefix->compare(0, std::string::npos, lua_tostring(_pState, -2), pPrefix->size()) == 0) {
					// remove the raw!
					lua_pushvalue(_pState, -2); // duplicate key
					lua_pushnil(_pState);
					lua_rawset(_pState, -5);
					++count;
				}
				lua_pop(_pState, 1);
			}
			lua_pop(_pState, 1); // remove table
			return count;
		}
		bool set(const string& key, ScriptReader& reader, DataReader& parameters) {
			lua_pop(_pState, 1);
			lua_pushlstring(_pState, key.data(), key.size());
			lua_pushvalue(_pState, reader.current());
			reader.next();
			lua_rawset(_pState, -3);
			lua_pop(_pState, 1); // remove table
			return true;
		}
	private:
		void getTable() const {
			Script::FromObject(_pState, _object);
			lua_getmetatable(_pState, -1);
			lua_replace(_pState, -2); // remove object
			lua_rawgeti(_pState, -1, 0);
			lua_replace(_pState, -2); // remove metatable
		}
		lua_State* _pState;
		Object&  _object;
	};
	LUAMap<Object>::Define<Mapper>(pState, -1);
}
template<> void Script::ObjClear(lua_State *pState, Object& object) {
}

}
