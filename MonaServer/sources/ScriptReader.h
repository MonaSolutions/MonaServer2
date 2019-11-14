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

#include "Mona/ReferableReader.h"
#include "ScriptWriter.h"

namespace Mona {


struct ScriptReader : ReferableReader, virtual Object {
	/*!
	Read LUA stack from the beginning */
	ScriptReader(lua_State *pState) : _pState(pState), _end(range<UInt32>(lua_gettop(pState))+1), _begin(1), _current(1) {}
	/*!
	Read LUA stack from count elements before the end */
	ScriptReader(lua_State *pState, UInt32 available) : _pState(pState), _end(range<UInt32>(lua_gettop(pState))+1) {
		_begin = _current = available>_end ? 1 : (_end - available);
	}
	ScriptReader(lua_State *pState, UInt32 available, UInt32 count) : _pState(pState), _end(range<UInt32>(lua_gettop(pState)) + 1) {
		_begin = _current = available>_end ? 1 : (_end - available);
		if ((_end -_begin) > count)
			_end = _begin + count;
	}
	lua_State* lua() { return _pState; }
	UInt32	position() const { return _current-_begin; }
	UInt32	current() const { return _current; }
	UInt32  available() const { return _end - _current; }
	void	reset() { _current = _begin; }

	static ScriptReader&	Null() { static ScriptReader Null; return Null; }

#if defined(_DEBUG)
	UInt32 read(DataWriter& writer, UInt32 count = END) {
		// DEBUG causes this method is used a lot and dynamic_cast is cpu expensive
		if (dynamic_cast<ScriptWriter*>(&writer))
			CRITIC("A ScriptReader is writing to a ScriptWriter, behavior undefined (unsafe)");
		return ReferableReader::read(writer, count);
	}
#endif


private:
	ScriptReader() : _pState(NULL), _end(0), _begin(0), _current(0) {}

	UInt8	followingType();
	bool	readOne(UInt8 type, DataWriter& writer);

	bool	writeNext(DataWriter& writer);

	lua_State*	_pState;	
	UInt32		_begin;
	UInt32		_current;
	UInt32		_end;
	Packet*		_pPacket;
};


} // namespace Mona
