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

#include "LUAXML.h"
#include "Mona/BinaryWriter.h"
#include "Mona/Packet.h"


using namespace std;


namespace Mona {

//////////////   XML TO LUA   /////////////////////

bool LUAXML::LUAToXML(Exception& ex, lua_State* pState, int index) {
	// index - LUA XML table
	if (!lua_istable(pState, index)) {
		ex.set<Ex::Application::Error>("Just LUA table can be convert to XML");
		return false;
	}

	const char* name(NULL);
	bool result(false);
	shared<Buffer> pBuffer(SET);
	BinaryWriter writer(*pBuffer);
	
	lua_pushnil(pState);  // first key 
	while (lua_next(pState, index)) {
		// uses 'key' (at index -2) and 'value' (at index -1) 
		if (lua_type(pState, -2) == LUA_TSTRING) {

			name = lua_tostring(pState, -2);

			if (lua_istable(pState, -1)) {

				// information fields { xml = {version = 1.0}, ... }

				String::Append(writer, "<?", name, " ");

				lua_pushnil(pState);  // first key 
				while (lua_next(pState, -2)) {
					// uses 'key' (at index -2) and 'value' (at index -1) 
					if (lua_type(pState, -2) == LUA_TSTRING && lua_isstring(pState, -1))
						String::Append(writer, lua_tostring(pState, -2), "=\"", lua_tostring(pState, -1), "\" ");
					else
						ex.set<Ex::Application::Error>("Bad ", name, " XML attribute information, key must be string and value convertible to string");
					lua_pop(pState, 1);
				}

				writer.write(EXPAND("?>"));
			} else
				ex.set<Ex::Application::Error>("Bad ",name," XML information, value must be a table");

		} else if (lua_isnumber(pState, -2)) {
			if(lua_istable(pState, -1))
				result = LUAToXMLElement(ex, pState, writer);
			else
				ex.set<Ex::Application::Error>("Impossible to write inner XML data on the top of XML level");
		}  else
			ex.set<Ex::Application::Error>("Impossible to convert the key of type ",lua_typename(pState,lua_type(pState,-2)),"to a correct XML root element");

		lua_pop(pState, 1);
	}

	if (result)
		Script::NewObject(pState, new Packet(pBuffer));

	return result;
}


bool LUAXML::LUAToXMLElement(Exception& ex, lua_State* pState, BinaryWriter& writer) {
	// -1 => table

	lua_pushstring(pState, "__name");
	lua_rawget(pState, -2);
	const char* name(lua_tostring(pState, -1));
	lua_pop(pState, 1);
	if (!name) {
		ex.set<Ex::Application::Error>("Impossible to write a XML element without name (__name)");
		return false;
	}

	// write attributes
	String::Append(writer, "<", name, " ");
	lua_pushnil(pState);  // first key 
	while (lua_next(pState, -2)) {
		// uses 'key' (at index -2) and 'value' (at index -1) 
		if (lua_type(pState, -2) == LUA_TSTRING && strcmp(lua_tostring(pState, -2),"__name")!=0 && lua_isstring(pState, -1))
			String::Append(writer, lua_tostring(pState, -2), "=\"", lua_tostring(pState, -1), "\" ");
		lua_pop(pState, 1);
	}
	if (lua_objlen(pState, -1) == 0) {
		writer.write(EXPAND("/>"));
		return true;
	}

	writer.write(EXPAND(">"));

	// write elements
	lua_pushnil(pState);  // first key 
	while (lua_next(pState, -2)) {
		// uses 'key' (at index -2) and 'value' (at index -1) 
		if (lua_isnumber(pState, -2)) {
			if (lua_istable(pState, -1))
				LUAToXMLElement(ex, pState, writer); // table child
			else {
				size_t size;
				const char* value = lua_tolstring(pState, -1, &size);
				if (value)
					writer.write(value, size);
				else
					ex.set<Ex::Application::Error>("Impossible to convert the value of type ", lua_typename(pState, lua_type(pState, -1)), "to a correct XML value of ", name);
			}
		} else if (lua_type(pState, -2) != LUA_TSTRING)
			ex.set<Ex::Application::Error>("Impossible to convert the key of type ",lua_typename(pState,lua_type(pState,-2)),"to a correct XML element of ",name);
		lua_pop(pState, 1);
	}

	String::Append(writer, "</", name, ">");
	return true;
}


//////////////   LUA TO XML   /////////////////////


static int Index(lua_State* pState) {
	// 1 - table
	// 2 - key not found

	size_t size;
	const char* key = lua_tolstring(pState, 2, &size);
	if (!key)
		return 0;
	if (strcmp(key, "__value") == 0) {
		// return the first index value
		lua_rawgeti(pState, 1, 1);
		return 1;
	}

	lua_pushnil(pState);  // first key 
	while (lua_next(pState, 1)) {
		// uses 'key' (at index -2) and 'value' (at index -1) 
		if (lua_type(pState, -2) == LUA_TNUMBER && lua_type(pState, -1) == LUA_TTABLE) {
			lua_pushstring(pState, "__name");
			lua_rawget(pState, -2);
			size_t len;
			const char* name = lua_tolstring(pState, -1, &len);
			lua_pop(pState, 1); // remove __name
			if (name && len==size && memcmp(key, name, size) == 0) {
				lua_replace(pState, -2);
				return 1;
			}
		}
		lua_pop(pState, 1);
	}
	
	return 0; // not found!
}

static int NewIndex(lua_State* pState) {
	// 1 - table
	// 2 - key not found
	// 3 - value
	size_t size;
	const char* key = lua_tolstring(pState, 2, &size);
	if (key && strcmp(key, "__value") == 0) {
		// assign new value to __value!
		lua_pushvalue(pState, 3);
		lua_rawseti(pState, 1, 1);
	} else
		lua_rawset(pState, 1);
	return 0;
}
static int ToString(lua_State* pState) {
	// 1 - table
	Exception ex;
	SCRIPT_BEGIN(pState)
		if (LUAXML::LUAToXML(ex, pState, 1)) {
			if(ex)
				SCRIPT_WARN("LUAToXML, ", ex)
			return 1;
		}
		SCRIPT_ERROR("LUAToXML, ", ex)
	SCRIPT_END
	return 0;
}

bool LUAXML::onXMLInfos(const char* name, Parameters& attributes) {
	lua_pushstring(_pState, name);
	Script::PushValue(_pState, attributes);
	lua_rawset(_pState, _firstIndex);
	return true;
}

bool LUAXML::onStartXMLElement(const char* name, Parameters& attributes) {
	Script::PushValue(_pState, attributes);
	lua_pushliteral(_pState, "__name");
	lua_pushstring(_pState, name);
	lua_rawset(_pState, -3);
	return true;
}

bool LUAXML::onInnerXMLElement(const char* name, const char* data, UInt32 size) {
	lua_pushlstring(_pState,data,size);
	lua_rawseti(_pState,-2,lua_objlen(_pState,-2)+1);
	return true;
}

bool LUAXML::onEndXMLElement(const char* name) {
	addMetatable();
	lua_rawseti(_pState,-2,lua_objlen(_pState,-2)+1); // set in parent table
	return true;
}

void LUAXML::addMetatable() {
	// metatable
	Script::NewMetatable(_pState);
	lua_pushliteral(_pState, "__index");
	lua_pushcfunction(_pState, &Index);
	lua_rawset(_pState, -3);
	lua_pushliteral(_pState, "__newindex");
	lua_pushcfunction(_pState, &NewIndex);
	lua_rawset(_pState, -3);
	lua_pushliteral(_pState, "__tostring");
	lua_pushcfunction(_pState, &ToString);
	lua_rawset(_pState, -3);
	lua_pushliteral(_pState, "__call");
	lua_pushcfunction(_pState, &Script::Call<XMLParser>);
	lua_rawset(_pState, -3);
	lua_setmetatable(_pState, -2);
}


}
