/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once


#include "Mona/Mona.h"
#include "Mona/UDProtocol.h"

namespace Mona {

struct STUNProtocol : UDProtocol, virtual Object {
	STUNProtocol(const char* name, ServerAPI& api, Sessions& sessions) : UDProtocol(name, api, sessions) {
		setNumber("port", 3478);
	}

private:
	Socket::Decoder* newDecoder();
};



} // namespace Mona
