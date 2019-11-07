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

#include "LUABit.h"

using namespace std;

namespace Mona {

#define OP_BIN(OP)

template<typename Type>
static bool ReadNumber(lua_State* pState, UInt32 index, Type& number) {
	SCRIPT_BEGIN(pState);
		int isNum;
		lua_Integer value = lua_tointegerx(pState, index, &isNum);
		if (isNum) {
			number = range<Type>(value);
			return true;
		}
		SCRIPT_ERROR("Take a number in ", index, " argument");
	SCRIPT_END;
	return false;
}

static int tohex(lua_State* pState) {
	Int64 value; Int8 chars;
	if (!ReadNumber(pState, 1, value) || !ReadNumber(pState, 2, chars))
		return 0;
	value = Byte::ToBigEndian(value);
	String hex(String::Hex(BIN &value, sizeof(value), chars<0 ? HEX_UPPER_CASE : 0));
	chars = abs(chars);
	if (UInt8(chars) > hex.size())
		hex.append(chars - hex.size(), '0');
	else
		hex.resize(chars);
	lua_pushlstring(pState, hex.data(), hex.size());
	return 1;
}
static int bnot(lua_State* pState) {
	Int64 value;
	if (!ReadNumber(pState, 1, value))
		return 0;
	lua_pushinteger(pState, lua_Integer(~value));
	return 1;
}
static int band(lua_State* pState) {
	Int64 result=0, arg;
	int index=0;
	while(index++<lua_gettop(pState) && ReadNumber(pState, 1, arg))
		result &= arg;
	lua_pushinteger(pState, lua_Integer(result));
	return 1;
}
static int bor(lua_State* pState) {
	Int64 result = 0, arg;
	int index = 0;
	while (index++<lua_gettop(pState) && ReadNumber(pState, 1, arg))
		result |= arg;
	lua_pushinteger(pState, lua_Integer(result));
	return 1;
}
static int bxor(lua_State* pState) {
	Int64 result = 0, arg;
	int index = 0;
	while (index++<lua_gettop(pState) && ReadNumber(pState, 1, arg))
		result ^= arg;
	lua_pushinteger(pState, lua_Integer(result));
	return 1;
}
static int lshift(lua_State* pState) {
	Int64 value; UInt8 bits;
	if (!ReadNumber(pState, 1, value) || !ReadNumber(pState, 2, bits))
		return 0;
	lua_pushinteger(pState, lua_Integer(value << bits));
	return 1;
}
static int rshift(lua_State* pState) {
	Int64 value; UInt8 bits;
	if (!ReadNumber(pState, 1, value) || !ReadNumber(pState, 2, bits))
		return 0;
	lua_pushinteger(pState, lua_Integer((UInt64)value >> bits)); // cast to guarantee logical right shift
	return 1;
}
static int arshift(lua_State* pState) {
	Int64 value; UInt8 bits;
	if (!ReadNumber(pState, 1, value) || !ReadNumber(pState, 2, bits))
		return 0;
	lua_pushinteger(pState, lua_Integer((value >> bits) | (value < 0 ? ~(~0U >> bits) : 0)));
	return 1;
}
static int rol(lua_State* pState) {
	Int64 value; UInt8 bits;
	if (!ReadNumber(pState, 1, value) || !ReadNumber(pState, 2, bits))
		return 0;
	lua_pushinteger(pState, lua_Integer((value << bits) | (value >> min(64 - bits, 0))));
	return 1;
}
static int ror(lua_State* pState) {
	Int64 value; UInt8 bits;
	if (!ReadNumber(pState, 1, value) || !ReadNumber(pState, 2, bits))
		return 0;
	lua_pushinteger(pState, lua_Integer((value >> bits) | (value << min(64 - bits, 0))));
	return 1;
}
static int bswap(lua_State* pState) {
	Int64 value;
	if (!ReadNumber(pState, 1, value))
		return 0;
	lua_pushinteger(pState, lua_Integer(Byte::Flip(value)));
	return 1;
}

void LUABit::AddOperators(lua_State* pState) {
	SCRIPT_BEGIN(pState);
		SCRIPT_TABLE_BEGIN(-1)
			SCRIPT_DEFINE_FUNCTION("tohex", tohex);
			SCRIPT_DEFINE_FUNCTION("bnot", bnot);
			SCRIPT_DEFINE_FUNCTION("band", band);
			SCRIPT_DEFINE_FUNCTION("bor", bor);
			SCRIPT_DEFINE_FUNCTION("bxor", bxor);
			SCRIPT_DEFINE_FUNCTION("lshift", lshift);
			SCRIPT_DEFINE_FUNCTION("rshift", rshift);
			SCRIPT_DEFINE_FUNCTION("arshift", arshift);
			SCRIPT_DEFINE_FUNCTION("rol", rol);
			SCRIPT_DEFINE_FUNCTION("ror", ror);
			SCRIPT_DEFINE_FUNCTION("bswap", bswap);
		SCRIPT_TABLE_END
	SCRIPT_END(pState);
}

}
