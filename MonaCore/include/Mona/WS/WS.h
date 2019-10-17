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

#include "Mona/Mona.h"
#include "Mona/Session.h"


namespace Mona {


struct WS : virtual Static {
	enum Type {
		//FRAME_OP_CONT    = 0x00, /// Continuation frame.
		TYPE_NIL		= 0,
		TYPE_TEXT		= 0x01, /// Text frame.
		TYPE_BINARY		= 0x02, /// Binary frame.
		TYPE_CLOSE		= 0x08, /// Close connection.
		TYPE_PING		= 0x09, /// Ping frame.
		TYPE_PONG		= 0x0a /// Pong frame.
	};

	enum Code : UInt16 {
		CODE_NORMAL_CLOSE				= 1000,
		CODE_ENDPOINT_GOING_AWAY		= 1001,
		CODE_PROTOCOL_ERROR				= 1002,
		CODE_PAYLOAD_NOT_ACCEPTABLE		= 1003,
		CODE_RESERVED					= 1004,
		CODE_RESERVED_NO_STATUS_CODE	= 1005,
		CODE_RESERVED_ABNORMAL_CLOSE	= 1006,
		CODE_MALFORMED_PAYLOAD			= 1007,
		CODE_POLICY_VIOLATION			= 1008,
		CODE_PAYLOAD_TOO_BIG			= 1009,
		CODE_EXTENSION_REQUIRED			= 1010,
		CODE_UNEXPECTED_CONDITION		= 1011
	};

	static UInt16			ErrorToCode(Int32 error);

	static BinaryWriter&	WriteKey(BinaryWriter& writer, const std::string& key);
	static BinaryReader&	Unmask(BinaryReader& reader);

	struct Message : virtual Object, Packet {
		Message(UInt8 type, const Packet& packet, bool flush) : flush(flush), Packet(std::move(packet)), type(type) {}
		const UInt8	type;
		const bool  flush;
	};
};


} // namespace Mona
