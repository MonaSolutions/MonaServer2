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


#include "Mona/DataWriter.h"
#include "Script.h"
#include <vector>

namespace Mona {

struct ScriptWriter : DataWriter, virtual Object {
	ScriptWriter(lua_State *pState);
	virtual ~ScriptWriter();

	lua_State* lua() { return _pState; }

	UInt64	beginObject(const char* type=NULL);
	void	writePropertyName(const char* name) { lua_pushstring(_pState, name); }
	void	endObject() { endComplex(); }

	UInt64	beginArray(UInt32 size);
	void	endArray() { endComplex(); }

	UInt64	beginObjectArray(UInt32 size);

	void	writeNumber(double value) { begin();  lua_pushnumber(_pState, value); end(); }
	void	writeString(const char* value, UInt32 size) { begin(); lua_pushlstring(_pState,value,size); end(); }
	void	writeBoolean(bool value) { begin(); lua_pushboolean(_pState, value); end(); }
	void	writeNull() { begin(); lua_pushnil(_pState); end(); }
	UInt64	writeDate(const Date& date);
	UInt64	writeByte(const Packet& bytes);

	UInt64	beginMap(Exception& ex, UInt32 size, bool weakKeys = false);
	void	endMap() { endComplex(); }

	void	reset();

	bool	repeat(UInt64 reference);

private:
	UInt64	reference();
	bool	begin();
	void	end();
	void	endComplex();

	int			_top;
	lua_State*	_pState;

	std::vector<int>	_layers; // int>0 means array, int==0 means object, int==-1 means mixed, int=-3/-2 means map
	std::vector<int>	_references;
};


} // namespace Mona
