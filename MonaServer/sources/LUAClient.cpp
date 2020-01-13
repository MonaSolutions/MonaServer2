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

#include "LUAMap.h"
#include "Mona/SplitReader.h"
#include "Mona/Client.h"
#include "LUASocketAddress.h"
#include "LUAWriter.h"


using namespace std;

namespace Mona {

static int __tostring(lua_State *pState) {
	SCRIPT_CALLBACK(Client, client);
		SCRIPT_WRITE_DATA(client.id, Entity::SIZE);
	SCRIPT_CALLBACK_RETURN;
}

static int ping(lua_State *pState) {
	SCRIPT_CALLBACK(Client, client);
		SCRIPT_WRITE_INT(client.ping());
	SCRIPT_CALLBACK_RETURN;
}
static int rto(lua_State *pState) {
	SCRIPT_CALLBACK(Client, client);
		SCRIPT_WRITE_INT(client.rto());
	SCRIPT_CALLBACK_RETURN;
}

template<> Mona::Writer& LUAWriter<Client>::Writer(Client& client) { return client.writer(); }

template<> void Script::ObjInit(lua_State* pState, Mona::Client& client) {
	struct Mapper : LUAMap<const Parameters>::Mapper<>, virtual Object {
		Mapper(Mona::Client& client, lua_State* pState) : LUAMap<const Parameters>::Mapper<>(client.properties(), pState), _client(client) {}
		Int8 erase(const string& key) { return _client.eraseProperty(key); }
		int clear(string* pPrefix) {
			const auto& itBegin = pPrefix ? map.lower_bound(*pPrefix) : map.begin();
			if (pPrefix)
				++pPrefix->back();
			const auto& itEnd = pPrefix ? map.lower_bound(*pPrefix) : map.end();
			auto it = itBegin;
			while (it != itEnd) {
				if(_client.eraseProperty((it++)->first)<0)
					return -distance(itBegin, it); // removed elements + 1
			}
			return distance(itBegin, it);
		}
		bool set(const string& key, ScriptReader& reader, DataReader& parameters) {
			SplitReader splitReader(reader, parameters);
			const string* pValue = _client.setProperty(key, splitReader);
			if (!pValue)
				return false;
			Script::PushValue(pState, pValue->data(), pValue->size()); // push set value!
			return true;
		}
	private:
		Mona::Client& _client;
	};
	AddType<Net::Stats>(pState, client);
	AddComparator<Mona::Client>(pState);

	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE("properties", AddObject(pState, client.properties()));
		SCRIPT_DEFINE("__tab", lua_pushvalue(pState, -2));
		SCRIPT_DEFINE_FUNCTION("__call", &(LUAMap<Mona::Client, const Parameters>::Call<Mapper>));
		
		SCRIPT_DEFINE_DATA("id", client.id, Entity::SIZE)
		SCRIPT_DEFINE_STRING("protocol", client.protocol);
		SCRIPT_DEFINE_DOUBLE("connection", client.connection);
		SCRIPT_DEFINE("path", Script::NewObject(pState, new const Path(client.path)))
		SCRIPT_DEFINE_STRING("query", client.query);
	
		SCRIPT_DEFINE("address", SCRIPT_WRITE_ADDRESS(client.address));
		SCRIPT_DEFINE("serverAddress", SCRIPT_WRITE_ADDRESS(client.serverAddress));
		
		SCRIPT_DEFINE_FUNCTION("__tostring", &__tostring);
		SCRIPT_DEFINE_FUNCTION("ping", &ping);
		SCRIPT_DEFINE_FUNCTION("rto", &rto);
		SCRIPT_DEFINE_WRITER(Mona::Client);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, Mona::Client& client) {
	RemoveObject(pState, client.properties());
	RemoveType<Net::Stats>(pState, client);
}

}
