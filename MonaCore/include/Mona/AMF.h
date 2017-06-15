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

namespace Mona {

struct AMF : virtual Static {
	enum Type {
		TYPE_EMPTY				=0x00,
		TYPE_CHUNKSIZE			=0x01,
		TYPE_ABORT				=0x02,
		TYPE_ACK				=0x03,
		TYPE_RAW				=0x04,
		TYPE_WIN_ACKSIZE		=0x05,
		TYPE_BANDWIDTH			=0x06,
		TYPE_AUDIO				=0x08,
		TYPE_VIDEO				=0x09,
		TYPE_DATA_AMF3			=0x0F,
		TYPE_INVOCATION_AMF3	=0x11,
		TYPE_DATA				=0x12,
		TYPE_INVOCATION			=0x14
	};

	enum AMF3 {
		AMF3_UNDEFINED	= 0x00,
		AMF3_NULL		= 0x01,
		AMF3_FALSE		= 0x02,
		AMF3_TRUE		= 0x03,
		AMF3_INTEGER	= 0x04,
		AMF3_NUMBER		= 0x05,
		AMF3_STRING		= 0x06,
		AMF3_DATE		= 0x08,
		AMF3_ARRAY		= 0x09,
		AMF3_OBJECT		= 0x0A,
		AMF3_BYTEARRAY	= 0x0C,
		AMF3_DICTIONARY	= 0x11
	};

	enum AMF0 {
		AMF0_NULL				= 0x05,
		AMF0_UNDEFINED			= 0x06,
		AMF0_UNSUPPORTED		= 0x0D,
		AMF0_AMF3_OBJECT		= 0x11,

		AMF0_NUMBER				= 0x00,
		AMF0_BOOLEAN			= 0x01,
		AMF0_STRING				= 0x02,
		AMF0_DATE				= 0x0B,

		AMF0_BEGIN_OBJECT		= 0x03,
		AMF0_BEGIN_TYPED_OBJECT	= 0x10,
		AMF0_END_OBJECT			= 0x09,
		AMF0_REFERENCE			= 0x07,

		AMF0_MIXED_ARRAY		= 0x08,
		AMF0_STRICT_ARRAY		= 0x0A,

		AMF0_LONG_STRING		= 0x0C
	};

};

}
