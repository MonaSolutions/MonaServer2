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
#include "Mona/Timer.h"


namespace Mona {

struct LUATimer : Timer::OnTimer {
	LUATimer(lua_State* pState, const Timer& timer, int index);
	~LUATimer();

	const Timer& timer;

	/*!
	Pin the LUA table at index to this timer in the registry to avoid its deletion! */
	void pin(int index);

private:
	
	void unpin();

	lua_State*		_pState;
	int				_refFunc;
	int				_ref;
};
	

}
