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
	ScriptReader(lua_State *pState, UInt32 count) : _pState(pState), _end(range<UInt32>(lua_gettop(pState))+1) {
		_begin = _current = count>_end ? 1 : (_end - count);
	}

	UInt32	position() const { return _current-_begin; }
	void	reset() { _current = _begin; }

#if defined(_DEBUG)
	UInt32 read(DataWriter& writer, UInt32 count = END) {
		// DEBUG causes this method is used a lot and dynamic_cast is cpu expensive
		if (dynamic_cast<ScriptWriter*>(&writer))
			CRITIC("A ScriptReader is writing to a ScriptWriter, behavior undefined (unsafe)");
		return ReferableReader::read(writer, count);
	}
#endif

private:
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
