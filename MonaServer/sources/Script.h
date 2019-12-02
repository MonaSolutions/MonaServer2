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

#include "Mona/Logs.h"
#include "Mona/Client.h"
#include "Mona/Packet.h"

// Automatically link Base library
#if defined(_MSC_VER)
#pragma comment(lib, "lua51.lib")
#endif

extern "C" {
#if defined(_WIN32)
#include "luajit/lua.h"
#include "luajit/lauxlib.h"
#include "luajit/lualib.h"
#else
// take in priority luajit-2.1 if available!
#if __has_include("luajit-2.0/lua.h") && !__has_include("luajit-2.1/lua.h")
#include "luajit-2.0/lua.h"
#include "luajit-2.0/lauxlib.h"
#include "luajit-2.0/lualib.h"
#else
#include "luajit-2.1/lua.h"
#include "luajit-2.1/lauxlib.h"
#include "luajit-2.1/lualib.h"
#endif
#endif

#if !defined(LUA_OK) // luajit <2.1
#define lua_tointegerx(STATE, INDEX, ISNUM) ((*ISNUM=lua_isnumber(STATE, INDEX)) ? lua_tointeger(STATE, INDEX) : 0 )
#define lua_tonumberx(STATE, INDEX, ISNUM) ((*ISNUM=lua_isnumber(STATE, INDEX)) ? lua_tonumber(STATE, INDEX) : 0 )
#endif
}

#define SCRIPT_LOG(LEVEL, DISPLAYSCALLER, ...)	 { if (Logs::GetLevel() >= LEVEL)  { Script::Log(__pState, LEVEL, __FILE__, __LINE__, DISPLAYSCALLER, __VA_ARGS__); } }

#define SCRIPT_FATAL(...)	SCRIPT_LOG(LOG_FATAL, true, __VA_ARGS__)
#define SCRIPT_CRITIC(...)	SCRIPT_LOG(LOG_CRITIC, true, __VA_ARGS__)
#define SCRIPT_ERROR(...)	SCRIPT_LOG(LOG_ERROR, true, __VA_ARGS__)
#define SCRIPT_WARN(...)	SCRIPT_LOG(LOG_WARN, true, __VA_ARGS__)
#define SCRIPT_NOTE(...)	SCRIPT_LOG(LOG_NOTE, true,__VA_ARGS__)
#define SCRIPT_INFO(...)	SCRIPT_LOG(LOG_INFO, true, __VA_ARGS__)
#define SCRIPT_DEBUG(...)	SCRIPT_LOG(LOG_DEBUG, true, __VA_ARGS__)
#define SCRIPT_TRACE(...)	SCRIPT_LOG(LOG_TRACE, true, __VA_ARGS__)

#define SCRIPT_AUTO_FATAL(FUNCTION, ...) { if(FUNCTION) { if(ex)  SCRIPT_WARN(ex); } else { FATAL_ERROR( __VA_ARGS__,", ", ex) } }
#define SCRIPT_AUTO_CRITIC(FUNCTION) { if(FUNCTION) { if(ex)  SCRIPT_WARN(ex); } else { SCRIPT_CRITIC(ex) } }
#define SCRIPT_AUTO_ERROR(FUNCTION) { if(FUNCTION) { if(ex)  SCRIPT_WARN(ex); } else { SCRIPT_ERROR(ex) } }
#define SCRIPT_AUTO_WARN(FUNCTION) { if(FUNCTION) { if(ex)  SCRIPT_WARN(ex); } else { SCRIPT_WARN(ex) } }

#define SCRIPT_CALLBACK(TYPE,OBJ)								{int __arg=1;lua_State* __pState = pState; TYPE* __pObj = Script::ToObject<TYPE>(__pState,1,true); if(!__pObj) return 0; TYPE& OBJ = *__pObj; int __lastArg=lua_gettop(__pState); (void)OBJ; /* void cast allow to remove warning "unused variable"*/
#define SCRIPT_CALLBACK_TRY(TYPE,OBJ)							SCRIPT_CALLBACK(TYPE,OBJ) bool __canRaise(SCRIPT_NEXT_TYPE == LUA_TFUNCTION); if(__canRaise) ++__arg;

#define SCRIPT_CALLBACK_THROW(...)								if (__canRaise) { lua_pushvalue(__pState,2); Script::PushString(__pState, __VA_ARGS__); if(lua_pcall(__pState, 1, 0, 0)) SCRIPT_ERROR(Script::LastError(__pState)); }

#define SCRIPT_CALLBACK_RETURN									__arg = __lastArg -__arg; if(__arg>0) SCRIPT_WARN(__arg," useless argument") else if(__arg<0) SCRIPT_WARN(-__arg," missing argument"); __lastArg = lua_gettop(__pState)-__lastArg; return (__lastArg>0 ? __lastArg : 0);}
#define SCRIPT_FIX_RESULT										{ lua_pushvalue(__pState,2); lua_pushvalue(__pState, -2); lua_rawset(__pState, 1);}

#define SCRIPT_BEGIN(STATE)										if(lua_State* __pState = STATE) {

#define SCRIPT_MEMBER_FUNCTION_BEGIN(OBJ,MEMBER,...)			if(Script::FromObject(__pState,OBJ)) { for(const char* __name : {MEMBER, __VA_ARGS__}) { lua_pushstring(__pState, __name); lua_rawget(__pState,-2); if(!lua_isfunction(__pState,-1)) { lua_pop(__pState,1); continue; } lua_getfenv(__pState, -1); lua_rawseti(__pState, LUA_REGISTRYINDEX, LUA_ENVIRONINDEX); int __top=lua_gettop(__pState); lua_pushvalue(__pState,-2); const std::string& __type = typeof<std::remove_const<decltype(OBJ)>::type>();
#define SCRIPT_FUNCTION_BEGIN(NAME,REFERENCE)					{ thread_local lua_Debug __Debug; if (REFERENCE==LUA_ENVIRONINDEX && lua_getstack(_pState, 0, &__Debug)==1) lua_pushvalue(__pState, LUA_ENVIRONINDEX); else lua_rawgeti(__pState,LUA_REGISTRYINDEX, REFERENCE);  const char* __name = NAME; if (lua_istable(__pState, -1)) lua_getfield(__pState, -1, __name); else lua_pushvalue(__pState, -1); if(!lua_isfunction(__pState,-1)) lua_pop(__pState,1); else for(;;) { lua_getfenv(__pState, -1); lua_rawseti(__pState, LUA_REGISTRYINDEX, LUA_ENVIRONINDEX); int __top=lua_gettop(__pState); const std::string& __type = Mona::String::Empty();
#define SCRIPT_FUNCTION_CALL_WITHOUT_LOG						const char* __err=NULL; if(lua_pcall(__pState,lua_gettop(__pState)-__top,LUA_MULTRET,0)) { __err = lua_tostring(__pState,-1); lua_pop(__pState,1); } int __lastArg=lua_gettop(__pState); int __arg=--__top; 
#define SCRIPT_FUNCTION_CALL									const char* __err=NULL; if(lua_pcall(__pState,lua_gettop(__pState)-__top,LUA_MULTRET,0)) SCRIPT_ERROR(__err = Script::LastError(__pState)); int __lastArg=lua_gettop(__pState); int __arg=--__top; 
#define SCRIPT_FUNCTION_NULL_CALL								lua_pop(__pState,lua_gettop(__pState)-__top+1); int __lastArg=lua_gettop(__pState);int __arg=--__top;
#define SCRIPT_FUNCTION_ERROR									__err
#define SCRIPT_FUNCTION_END										lua_pop(__pState,__lastArg-__top); __arg = __lastArg-__arg; if(__arg>0) SCRIPT_WARN(__arg," useless argument on ",__type, (__type.empty() ? "" : "."), __name," result") else if(__arg<0) SCRIPT_WARN(-__arg," missing argument on ", __type, __name," result") break; } lua_pop(__pState,1); }
#define SCRIPT_FUNCTION_NAME									__name

#define SCRIPT_WRITE_VALUE(...)									Script::PushValue(__pState, __VA_ARGS__);
#define SCRIPT_WRITE_STRING(...)								Script::PushString(__pState, __VA_ARGS__);
#define SCRIPT_WRITE_DATA(DATA, SIZE)							lua_pushlstring(__pState,STR DATA, SIZE);
#define SCRIPT_WRITE_BOOLEAN(VALUE)								lua_pushboolean(__pState,VALUE);
#define SCRIPT_WRITE_FUNCTION(VALUE)							lua_pushcfunction(__pState,VALUE);
#define SCRIPT_WRITE_DOUBLE(VALUE)								lua_pushnumber(__pState,(lua_Number)VALUE);
#define SCRIPT_WRITE_INT(VALUE)									lua_pushinteger(__pState,(lua_Integer)VALUE);
#define SCRIPT_WRITE_NIL										lua_pushnil(__pState);
#define SCRIPT_WRITE_PTR(VALUE)									lua_pushlightuserdata(__pState,VALUE);
#define SCRIPT_WRITE_PACKET(PACKET)								Script::NewObject(__pState,new Packet(PACKET));

#define SCRIPT_TABLE_BEGIN(index)								{ bool __new = index==0; if(__new) lua_newtable(__pState); else lua_pushvalue(pState, index); int __top = lua_gettop(__pState);
#define SCRIPT_TABLE_END										while(lua_gettop(__pState)>__top) { if((lua_gettop(__pState)-1)==__top) lua_rawseti(__pState, __top, lua_objlen(__pState, __top)+1); else lua_rawset(__pState, __top);} if(!__new) lua_pop(pState, 1); }				

#define SCRIPT_DEFINE(NAME, ...)								{ lua_pushliteral(__pState, NAME); __VA_ARGS__; }
#define SCRIPT_DEFINE_FUNCTION(NAME, VALUE)						{ lua_pushliteral(__pState, NAME); SCRIPT_WRITE_FUNCTION(VALUE); }
#define SCRIPT_DEFINE_DATA(NAME, DATA, SIZE)					{ lua_pushliteral(__pState, NAME); SCRIPT_WRITE_DATA(DATA, SIZE); }
#define SCRIPT_DEFINE_STRING(NAME, ...)							{ lua_pushliteral(__pState, NAME); SCRIPT_WRITE_STRING(__VA_ARGS__); }
#define SCRIPT_DEFINE_BOOLEAN(NAME, VALUE)						{ lua_pushliteral(__pState, NAME); SCRIPT_WRITE_BOOLEAN(VALUE); }
#define SCRIPT_DEFINE_DOUBLE(NAME, VALUE)						{ lua_pushliteral(__pState, NAME); SCRIPT_WRITE_DOUBLE(VALUE); }
#define SCRIPT_DEFINE_INT(NAME, VALUE)							{ lua_pushliteral(__pState, NAME); SCRIPT_WRITE_INT(VALUE); }

#define SCRIPT_NEXT_READABLE									(__lastArg-__arg)
#define SCRIPT_NEXT_ARG											((__lastArg-__arg) ? (__arg+1) : 0)
#define SCRIPT_NEXT_TYPE										((__lastArg-__arg) ? lua_type(__pState,__arg+1) : LUA_TNONE)
#define SCRIPT_NEXT_SHRINK(COUNT)								{ int __count=COUNT; while((__lastArg-__arg) > __count--) { lua_remove(__pState, __lastArg--); }}

#define SCRIPT_READ_NEXT(COUNT)									( (__arg += COUNT) > __lastArg ? 0 : __arg)
#define SCRIPT_READ_NIL											{ ++__arg; }
#define SCRIPT_READ_BOOLEAN(DEFAULT)							((__lastArg-__arg++)>0 ? lua_toboolean(__pState,__arg)==1 : DEFAULT)
#define SCRIPT_READ_STRING(DEFAULT)								((__lastArg-__arg++)>0 && lua_isstring(__pState,__arg) ? lua_tostring(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_UINT8(DEFAULT)								Mona::range<UInt8>((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tointeger(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_UINT16(DEFAULT)								Mona::range<UInt16>((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tointeger(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_UINT32(DEFAULT)								Mona::range<UInt32>((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tointeger(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_INT8(DEFAULT)								Mona::range<Int8>((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tointeger(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_INT16(DEFAULT)								Mona::range<Int16>((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tointeger(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_INT32(DEFAULT)								((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tointeger(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_DOUBLE(DEFAULT)								((__lastArg-__arg++)>0 && lua_isnumber(__pState,__arg) ? lua_tonumber(__pState,__arg) : DEFAULT)
#define SCRIPT_READ_PACKET(VALUE)								Packet VALUE; if((__lastArg - __arg++) > 0) { if (const Packet* pPacket = Script::ToObject<Packet>(pState, __arg)) { VALUE.set(*pPacket); } else { size_t size; if (const char* data = lua_tolstring(pState, __arg, &size)) VALUE.set(data, size); } }
#define SCRIPT_READ_DATA(VALUE,SIZE)							std::size_t SIZE = 0; const UInt8* VALUE = NULL; if((__lastArg-__arg++)>0 && lua_isstring(__pState,__arg)) { VALUE = (const UInt8*)lua_tolstring(__pState,__arg, &SIZE);}
#define SCRIPT_READ_PTR(TYPE)									((__lastArg-__arg++)>0 && lua_islightuserdata(__pState,__arg) ? (TYPE*)lua_touserdata(__pState,__arg) : NULL)

#define SCRIPT_END												}


namespace Mona {

struct Script : virtual Static {
	struct Pop : virtual Object {
		template<typename T>
		Pop(lua_State* pState, T&& t) : _count(1), _pState(pState) {}
		Pop(lua_State* pState) : _count(0), _pState(pState) {}
		~Pop() { lua_pop(_pState, _count); }
		template<typename T>
		T& operator+=(T&& t) { ++_count; return t; }
		operator UInt32() const { return _count; }
	private:
		int		_count;
		lua_State*	_pState;
	};
	static Mona::Client& Client() { static Mona::Client LUAClient("LUA", SocketAddress(IPAddress::Broadcast(),0)); return LUAClient; }

	static const char*	 LastError(lua_State *pState);
	static void			 Test(lua_State *pState);

	static void			 CloseState(lua_State* pState);
	static lua_State*	 CreateState();

	static void			 NewMetatable(lua_State* pState);

	static bool Resolve(lua_State *pState, const char* value, std::string& file, bool withExtension = true);

	static void PushValue(lua_State* pState, const std::string& value) { PushValue(pState, value.data(), value.size()); }
	static void PushValue(lua_State* pState, const void* value, std::size_t size);
	static void PushValue(lua_State* pState, const Parameters& parameters, const std::string& prefix = String::Empty());
	template <std::size_t size>
	static void PushValue(lua_State* pState, const char(&data)[size]) { PushValue(pState, data, size); }

	static void PushString(lua_State* pState, int index);
	static void PushString(lua_State* pState, const std::string& value) { lua_pushlstring(pState, value.data(), value.size()); }
	static void PushString(lua_State* pState, const char* data, std::size_t size) { lua_pushlstring(pState, data, size); }
	template <std::size_t size>
	static void PushString(lua_State* pState, const char(&data)[size]) { lua_pushlstring(pState, data, size); }

	template <typename Type>
	static Type& NewObject(lua_State *pState, Type* pType) {
		DEBUG_ASSERT(pType != NULL);
		return AddObject<Type>(pState, *pType, -1);
	}

	template<typename Type>
	static Type& AddObject(lua_State *pState, Type& object, Int8 autoRemove=0) {
		//NOTE("add ", typeof<Type>(), " ", &object);

		// __index table
		lua_newtable(pState);

		// Create metatable
		NewMetatable(pState);

		// set __index
		lua_pushliteral(pState, "__index");
		lua_pushvalue(pState, -3);
		lua_rawset(pState, -3);

		// set toString
		lua_pushliteral(pState, "__tostring");
		lua_pushcfunction(pState, &ToString<Type>);
		lua_rawset(pState, -3);

		// define __concat to use __tostring
		lua_pushliteral(pState, "__concat");
		lua_pushcfunction(pState, &Concat);
		lua_rawset(pState, -3);

		// set __call, by default without argument => tostring, with arguments => multiple index in one call
		lua_pushliteral(pState, "__call");
		lua_pushcfunction(pState, &(Call<Type, true>));
		lua_rawset(pState, -3);

		// Create table to represent our object + set metatable
		lua_newtable(pState);
		lua_pushvalue(pState, -2);
		lua_setmetatable(pState, -2);

		// -3 index
		// -2 metatable
		// -1 object

		// attach destructor
		if (autoRemove) {
			lua_pushliteral(pState, "__gc");
			lua_pushvalue(pState, -2); // table object in upvalue 1
			lua_pushlightuserdata(pState, autoRemove < 0 ? (void*)&object : NULL); // object if has to be deleted in upvalue2
			lua_newuserdata(pState, 0); // userdata in upvalue 3 (to keep it alive)
			lua_pushvalue(pState, -6); // metatable
			lua_setmetatable(pState, -2); // metatable of user data, as metatable as object
			lua_pushcclosure(pState, &DeleteObject<Type>, 3);
			lua_rawset(pState, -4); // function in metatable
			Util::Scoped<int> scopedTop(_AddTop, lua_gettop(pState));
			AddType<Type>(pState, object);
		} else {
			Util::Scoped<int> scopedTop(_AddTop, -lua_gettop(pState)); // persistent!
			AddType<Type>(pState, object);
		}

		lua_replace(pState, -2); // remove metatable!
		lua_replace(pState, -2); // remove index table!
		return object;
	}

	template<typename Type, int index = 0>
	static void RemoveObject(lua_State *pState, Type& object) {
		//NOTE("remove ", typeof<Type>(), " " , &object);
		if (index)
			lua_pushvalue(pState, index);
		else if (!FromObject<Type>(pState, object))
			return; // already removed!
		Util::Scoped<UInt32> scopedTop(_RemoveTop, lua_gettop(pState));
		RemoveType<Type>(pState, object);
#if defined(_DEBUG)
		// check in debug than all the AddType have been removed by a RemoveType!
		lua_getmetatable(pState, -1);
		lua_pushnil(pState);  // first key 
		while (lua_next(pState, -2)) {
			// uses 'key' (at index -2) and 'value' (at index -1) 
			if (lua_type(pState, -2) == LUA_TNUMBER)
				DEBUG_ASSERT(lua_touserdata(pState, -1)==NULL);
			lua_pop(pState, 1);
		}
		lua_pop(pState, 1); // remove metatable
#endif
		lua_pop(pState, 1); // remove object
	}

	/*!
	Get the table from object, returns 0 if fails, +1 if it's a persistent object, or +2 if it's a volatile object */
	template<typename Type>
	static bool FromObject(lua_State* pState, Type& object) {
		lua_rawgeti(pState, LUA_REGISTRYINDEX, TypeRef<typename std::remove_cv<Type>::type>(pState));
		// get Types[1] (all)
		lua_rawgeti(pState, -1, 1);
		if (lua_istable(pState, -1)) {
			lua_pushlightuserdata(pState, (void*)&object);
			lua_rawget(pState, -2);
			lua_replace(pState, -2); // remove Types[1]
			if (lua_istable(pState, -1)) {
				lua_replace(pState, -2); // removes Types
				return true;
			}
		}
		lua_pop(pState, 2); // remove Types[1] and Types
		return false;
	}

	template<typename Type>
	static Type* ToObject(lua_State *pState, int index=-1, bool withLogs=false) {
		if (!lua_istable(pState, index)) {
			if (!withLogs)
				return NULL;
			SCRIPT_BEGIN(pState)
				SCRIPT_ERROR(index, " argument not present, call method with ':' colon operator")
			SCRIPT_END
			return NULL;
		}
		if(!lua_getmetatable(pState,index)) {
			if(!withLogs)
				return NULL;
			SCRIPT_BEGIN(pState)
				SCRIPT_ERROR(index, " argument invalid, call method with ':' colon operator")
			SCRIPT_END
			return NULL;
		}

		lua_rawgeti(pState, -1, TypeRef<typename  std::remove_cv<Type>::type>(pState));
		if (!lua_islightuserdata(pState, -1)) {
			lua_pop(pState, 2); // remove this + metatable
			if (withLogs) {
				SCRIPT_BEGIN(pState)
					SCRIPT_ERROR(typeof<typename std::remove_cv<Type>::type>(), " argument invalid, call method with ':' colon operator")
				SCRIPT_END
			}
			return NULL;
		}
		Type* pThis = (Type*)lua_touserdata(pState, -1);
		lua_pop(pState, 2); // remove this + metatable
		if (pThis)
			return pThis;
		if (withLogs) {
			SCRIPT_BEGIN(pState)
				SCRIPT_ERROR(typeof<typename std::remove_cv<Type>::type>(), " object deleted")
			SCRIPT_END
		}
		return NULL;
	}

	template<typename Type, bool checkArgs=false>
	static int Call(lua_State *pState) {
		if (checkArgs) {
			int top = lua_gettop(pState);
			if (top > 1) { // with argument = setter
				SCRIPT_BEGIN(pState)
					SCRIPT_ERROR("No setter for ", typeof<typename std::remove_cv<Type>::type>());
				SCRIPT_END
				return 0;
			}
		}
		// without argument = tostring
		PushString(pState, 1);
		return 1;
	}
	
	template<typename Type>
	static void AddComparator(lua_State *pState) {
		lua_getmetatable(pState, -1);

		lua_pushliteral(pState, "__le");
		lua_pushcfunction(pState, &Le<Type>);
		lua_rawset(pState, -3);

		lua_pushliteral(pState, "__lt");
		lua_pushcfunction(pState, &Lt<Type>);
		lua_rawset(pState, -3);

		lua_pushliteral(pState, "__eq");
		lua_pushcfunction(pState, &Eq<Type>);
		lua_rawset(pState, -3);

		lua_pop(pState, 1);
	}

	template <typename ...Args>
	static void Log(lua_State* pState, LOG_LEVEL level, const char* file, long line, bool displaysCaller, Args&&... args) {
		// I stop remonting level stack at the sub level where I have gotten the name
		int stack(0);
		bool nameGotten(false);
		thread_local lua_Debug	debug;
		debug.name = NULL;
		while (lua_getstack(pState, stack++, &debug) && lua_getinfo(pState, nameGotten ? "Sl" : "nSl", &debug) && !nameGotten) {
			if (debug.name)
				nameGotten = true;
		}

		if (nameGotten) {
			if (strlen(debug.short_src) && strcmp(debug.short_src, "[C]"))
				file = debug.short_src;
			if (debug.currentline)
				line = debug.currentline;

			if (displaysCaller) {
				if (debug.namewhat)
					return Logs::Log(level, file, line, "(", debug.namewhat, " ", debug.name, ") ", args ...);
				return Logs::Log(level, file, line, "(", debug.name, ") ", args ...);
			}
		}
		Logs::Log(level, file, line, args ...);
	}

private:
	/*!
	-3 index
	-2 metatable
	-1 object */
	template<typename Type> static void ObjInit(lua_State *pState, Type& object);
	/*!
	-1 => object */
	template<typename Type> static void ObjClear(lua_State *pState, Type& object);


	template<typename Type>
	static void AddType(lua_State* pState, Type& object) {
		//DEBUG("ADD ", typeof<Type>(),' ', &object);
		if (FromObject(pState, object))
			FATAL_ERROR("A LUA", typeof<Type>() ,' ', &object, " was not released properly");
		DEBUG_ASSERT(!FromObject(pState, object));
		DEBUG_ASSERT(_AddTop != 0); // else is a call in a bad location (not encapsulated in AddObject)
		int top = abs(_AddTop);
		bool fixStack = lua_gettop(pState) > top;
		if(fixStack) {
			lua_pushvalue(pState, top - 2); // index
			lua_pushvalue(pState, top - 1); // metatable
			lua_pushvalue(pState, top); // object
		}
		Util::Scoped<int> scopedTop(_AddTop, _AddTop<0 ? -lua_gettop(pState) : lua_gettop(pState)); // negative if persistent!
		// -3 index
		// -2 metatable
		// -1 object

		int iType = TypeRef<typename std::remove_cv<Type>::type>(pState);

		// record type in metatable
		lua_pushlightuserdata(pState, (void*)&object);
		lua_rawseti(pState, -3, iType);

		// record in registry
		/// get Types table
		lua_rawgeti(pState, LUA_REGISTRYINDEX, iType);
		/// record in Types[2] if persistent
		if (_AddTop<0) { // is persistent!
			lua_rawgeti(pState, -1, 2);
			if (!lua_istable(pState, -1)) {
				lua_pop(pState, 1);
				lua_newtable(pState);
				lua_pushvalue(pState, -1);
				lua_rawseti(pState, -3, 2);
			}
			lua_pushlightuserdata(pState, (void*)&object);
			lua_pushvalue(pState, -4); // object
			lua_rawset(pState, -3);
			lua_pop(pState, 1); // remove Types[2]
		}
		/// record in Types[1] (all = volatile)
		lua_rawgeti(pState, -1, 1);
		if (!lua_istable(pState, -1)) {
			lua_pop(pState, 1);
			lua_newtable(pState);

			NewMetatable(pState); // metatable!
			lua_pushliteral(pState, "__mode");
			lua_pushliteral(pState, "v");
			lua_rawset(pState, -3);
			lua_setmetatable(pState, -2);

			lua_pushvalue(pState, -1);
			lua_rawseti(pState, -3, 1);
		}
		lua_pushlightuserdata(pState, (void*)&object);
		lua_pushvalue(pState, -4); // object
		lua_rawset(pState, -3);
		lua_pop(pState, 2); // remove Types[1] + Types

		top = lua_gettop(pState);
		ObjInit(pState, object);
		int newTop = lua_gettop(pState);
		while (newTop-- > top) {
			if (--newTop < top)
				lua_pushnil(pState);
			const char* key = lua_tostring(pState, -2);
			if (key && memcmp(key, EXPAND("__")) == 0) // NULL if not a string!
				lua_rawset(pState, top - 1); // set in metatable!
			else
				lua_rawset(pState, top - 2); // set in index!
		}
		if (fixStack)
			lua_pop(pState, 3);
	}

	template<typename Type>
	static void RemoveType(lua_State* pState, Type& object) {
		//DEBUG("REM ", typeof<Type>(), ' ', &object);
		DEBUG_ASSERT(_RemoveTop != 0); // else is a call in a bad location (not encapsulated in RemoveObject pr DeleteObject)
		bool fixStack = UInt32(lua_gettop(pState)) > _RemoveTop;
		if (fixStack)
			lua_pushvalue(pState, _RemoveTop); // object
		Util::Scoped<UInt32> scopedTop(_RemoveTop, lua_gettop(pState)); // negative if persistent!
		// -1 object
		// Erase object of registry in first to avoid a recursive call => example a gc deletion call an ServerAPI event which remove also the object with a explicitly call!
		/// get Types table
		int iType = TypeRef<typename std::remove_cv<Type>::type>(pState);
		lua_rawgeti(pState, LUA_REGISTRYINDEX, iType);
		/// remove from Types[1] (all)
		lua_rawgeti(pState, -1, 1);
		lua_pushlightuserdata(pState, (void*)&object);
		lua_pushnil(pState);
		lua_rawset(pState, -3);
		/// remove from Types[2] (persistent)
		lua_rawgeti(pState, -2, 2);
		if (lua_istable(pState, -1)) {
			lua_pushlightuserdata(pState, (void*)&object);
			lua_pushnil(pState);
			lua_rawset(pState, -3);
		}
		lua_pop(pState, 3); // remove Types[1] + Types[2] + Types


		// -1 is object
		ObjClear(pState, object);

		// signal object destruction in settings its types to a lightuserdata to NULL
		lua_getmetatable(pState, -1);
		lua_pushlightuserdata(pState, NULL);
		lua_rawseti(pState, -2, iType);
		lua_pop(pState, fixStack ? 2 : 1); // remove metatable
	}


	template<typename Type>
	static int ToString(lua_State* pState) {
		String str(typeof<typename std::remove_cv<Type>::type>(), '_', lua_topointer(pState, 1));
		lua_pushlstring(pState, str.data(), str.size());
		return 1;
	}

	// fix Concat to use __string on table!
	static int Concat(lua_State *pState);
	
	template<typename Type>
	static int TypeRef(lua_State* pState) {
		static struct Ref : virtual Object {
			Ref(lua_State *pState) {
				lua_newtable(pState);
				_ref = luaL_ref(pState, LUA_REGISTRYINDEX);
			}
			operator int() const { return _ref; };
		private:
			int _ref;
		} Ref(pState);
		return Ref;
	}

	template<typename Type>
	static int DeleteObject(lua_State *pState) {
		Type* pObject = ToObject<Type>(pState, lua_upvalueindex(1));
		if(pObject) // else already manually removed!
			RemoveObject<Type, lua_upvalueindex(1)>(pState, *pObject);
		pObject = (Type*)lua_touserdata(pState, lua_upvalueindex(2));
		if (pObject)
			delete pObject;
		return 0;
	}


	template<typename Type>
	static int Le(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object)
			Type* pObject = ToObject<Type>(pState, 2);
			if (pObject)
				SCRIPT_WRITE_BOOLEAN(object <= *pObject)
			else if (lua_isnil(pState, 2))
				SCRIPT_WRITE_BOOLEAN(!object)
			else
				SCRIPT_ERROR("Comparison impossible between one ", typeof<Type>()," and one ",lua_typename(pState,lua_type(pState,2)))
		SCRIPT_CALLBACK_RETURN
	}

	template<typename Type>
	static int Lt(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object)
			Type* pObject = ToObject<Type>(pState, 2);
			if (pObject)
				SCRIPT_WRITE_BOOLEAN(object < *pObject)
			else
				SCRIPT_ERROR("Comparison impossible between one ", typeof<Type>()," and one ",lua_typename(pState,lua_type(pState,2)))
		SCRIPT_CALLBACK_RETURN
	}

	template<typename Type>
	static int Eq(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object)
			Type* pObject = ToObject<Type>(pState, 2);
			if (pObject)
				SCRIPT_WRITE_BOOLEAN(object == *pObject)
			else if (lua_isnil(pState, 2))
				SCRIPT_WRITE_BOOLEAN(!object)
			else
				SCRIPT_ERROR("Comparison impossible between one ", typeof<Type>()," and one ",lua_typename(pState,lua_type(pState,2)))
		SCRIPT_CALLBACK_RETURN
	}

	static thread_local int		_AddTop; // negative if persistent
	static thread_local UInt32	_RemoveTop;

};


} // namespace Mona
