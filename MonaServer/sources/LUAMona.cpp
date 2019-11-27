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

#include "Script.h"
#include "Mona/ServerAPI.h"
#include "Mona/UDPSocket.h"
#include "Mona/WS/WSClient.h"
#include "Mona/SRT.h"
#include "LUAMap.h"
#include "LUAIPAddress.h"
#include "LUASocketAddress.h"
#include "LUAXML.h"
#include "LUASubscription.h"
#include "LUATimer.h"


using namespace std;

namespace Mona {

static int time(lua_State *pState) {
	SCRIPT_CALLBACK(ServerAPI, api)
		SCRIPT_WRITE_DOUBLE(Time::Now())
	SCRIPT_CALLBACK_RETURN
}
static int newIPAddress(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		if (SCRIPT_NEXT_READABLE) {
			Exception ex;
			IPAddress ip;
			if (SCRIPT_READ_IP(ip)) {
				if (ex)
					SCRIPT_CALLBACK_THROW(ex);
				SCRIPT_WRITE_IP(ip);
			} else
				SCRIPT_CALLBACK_THROW(ex);
		} else
			SCRIPT_WRITE_IP();
	SCRIPT_CALLBACK_RETURN
}
static int newSocketAddress(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		if (SCRIPT_NEXT_READABLE) {
			Exception ex;
			SocketAddress address;
			if (SCRIPT_READ_ADDRESS(address)) {
				if (ex)
					SCRIPT_CALLBACK_THROW(ex);
				SCRIPT_WRITE_ADDRESS(address);
			} else
				SCRIPT_CALLBACK_THROW(ex)
		} else
			SCRIPT_WRITE_ADDRESS();
	SCRIPT_CALLBACK_RETURN
}
static int newPath(lua_State *pState) {
	SCRIPT_CALLBACK(ServerAPI, api)
		// for security reason and to allow a MakeAbsolute/MakeRelative always resolve the relative path relating with the file location
		string file;
		if(Script::Resolve(pState, SCRIPT_NEXT_READABLE ? SCRIPT_READ_STRING("") : "", file))
			Script::NewObject(pState, new const Path(move(file)));
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int newIOSocket(lua_State *pState) {
	SCRIPT_CALLBACK(ServerAPI, api)
		Script::NewObject(pState, new Type(api.ioSocket));
	SCRIPT_CALLBACK_RETURN
}
static int newMediaWriter(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		const char* subMime = SCRIPT_READ_STRING(NULL);
	if (subMime) {
		unique<MediaWriter> pWriter = MediaWriter::New(subMime);
		if (pWriter)
			Script::NewObject(pState, pWriter.release());
		else
			SCRIPT_CALLBACK_THROW(String("Unknown ", subMime, " MIME type"));
	} else
		SCRIPT_CALLBACK_THROW("Require a MIME media string");
	SCRIPT_CALLBACK_RETURN
}
static int newMediaReader(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		const char* subMime = SCRIPT_READ_STRING(NULL);
		if (subMime) {
			unique<MediaReader> pReader = MediaReader::New(subMime);
			if (pReader)
				Script::NewObject(pState, pReader.release());
			else
				SCRIPT_CALLBACK_THROW(String("Unknown ", subMime, " MIME type"));
		} else
			SCRIPT_CALLBACK_THROW("Require a MIME media string");
	SCRIPT_CALLBACK_RETURN
}
static int setTimer(lua_State *pState) {
	SCRIPT_CALLBACK(ServerAPI, api)
		int arg = SCRIPT_READ_NEXT(1); // LUA function for a new event OR TimerID to modify/remove timer
		LUATimer* pTimer;
		if (lua_isfunction(pState, arg)) {
			// new timer!
			Script::NewObject(pState, pTimer = new LUATimer(pState, api.timer, arg)); // return timer!
		} else {
			pTimer = Script::ToObject<LUATimer>(pState, arg);
			if (pTimer)
				lua_pushvalue(pState, arg);
			else
				SCRIPT_ERROR("Require a callback LUA function or a timer to modify");
		}
		if (pTimer) {
			// a mona:setTimer(timer) execute the timer immediatly, just an explicit setTimer(timer, 0) stop it!
			UInt32 timeout;
			if (!lua_isnumber(pState, SCRIPT_NEXT_ARG)) {
				// execute now
				lua_pushvalue(pState, arg); // function
				lua_pushinteger(pState, 0); // delay = 0
				lua_call(pState, 1, 1); // return timeout
				timeout = Mona::range<UInt32>(lua_tointeger(pState, -1));
				lua_pop(pState, 1); //remove timeout
			} else
				timeout = Mona::range<UInt32>(lua_tointeger(pState, SCRIPT_READ_NEXT(1)));
			api.timer.set(*pTimer, timeout);
			pTimer->pin(-1);
		}
	SCRIPT_CALLBACK_RETURN
}

static int newPublication(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		string stream = SCRIPT_READ_STRING("");
		Exception ex;
		Publication* pPublication = api.publish(ex, Script::Client(), stream);
		// ex.error already displayed!
		if (pPublication) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			Script::AddObject(pState, *pPublication, 1); // no new because no deletion require!
			lua_getmetatable(pState, -1);
			lua_pushliteral(pState, "|api");
			lua_pushlightuserdata(pState, &api);
			lua_rawset(pState, -3);
			lua_pop(pState, 1); // remove metatable
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int newSubscription(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		bool subscription = false;
		Exception ex;
		unique<Subscription> pSubscription(new LUASubscription(pState));
		ScriptReader reader(pState, SCRIPT_NEXT_READABLE); // before the read_next!
		int arg = SCRIPT_READ_NEXT(1);
		Publication* pPublication = Script::ToObject<Publication>(pState, arg);
		if (pPublication) {
			if (!(subscription = api.subscribe(ex, Script::Client(), pPublication->name(), *pSubscription)) || ex)
				SCRIPT_CALLBACK_THROW(ex);
		} else {
			string name;
			size_t size;
			const char* value = lua_tolstring(pState, arg, &size);
			if(value) {
				if (!(subscription = api.subscribe(ex, Script::Client(), name.append(value, size), *pSubscription)) || ex)
					SCRIPT_CALLBACK_THROW(ex);
			} else {
				// is a subscription without explicit source linking, usefull for example to use with MediaReader!
				// Args are subscriptions params!
				MapWriter<Parameters> mapWriter(*pSubscription);
				SCRIPT_READ_NEXT(reader.read(mapWriter)-1);
				Script::NewObject(pState, pSubscription.release());
			}
		}
		if (subscription) {
			Script::NewObject(pState, pSubscription.release());
			lua_getmetatable(pState, -1);
			lua_pushliteral(pState, "|api");
			lua_pushlightuserdata(pState, &api);
			lua_rawset(pState, -3);
			lua_pop(pState, 1); // remove metatable
		}
	SCRIPT_CALLBACK_RETURN
}

static int fromXML(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		SCRIPT_READ_DATA(data, size)
		Exception ex;
		if (!LUAXML::XMLToLUA(ex, pState, STR data, size) || ex)
			SCRIPT_CALLBACK_THROW(ex)
	SCRIPT_CALLBACK_RETURN
}
static int toXML(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		// TODO compress option
		Exception ex;
		if (!LUAXML::LUAToXML(ex, pState, SCRIPT_READ_NEXT(1)) || ex)
			SCRIPT_CALLBACK_THROW(ex)
	SCRIPT_CALLBACK_RETURN
}
static int fromData(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		int isNum;
		Media::Data::Type type = Media::Data::Type(lua_tointegerx(pState, lua_upvalueindex(1), &isNum));
		if (!isNum)
			type = Media::Data::ToType(SCRIPT_READ_STRING(""));
		SCRIPT_READ_PACKET(packet);
		unique<DataReader> pReader = Media::Data::NewReader(type, packet);
		if (pReader) {
			ScriptWriter writer(pState);
			pReader->read(writer);
		} else
			SCRIPT_CALLBACK_THROW(String("Unsupported type ", Media::Data::TypeToString(type)));
	SCRIPT_CALLBACK_RETURN
}
static int toData(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(ServerAPI, api)
		int isNum;
		Media::Data::Type type = Media::Data::Type(lua_tointegerx(pState, lua_upvalueindex(1), &isNum));
		if(!isNum)
			type = Media::Data::ToType(SCRIPT_READ_STRING(""));
		shared<Buffer> pBuffer(SET);
		unique<DataWriter> pWriter = Media::Data::NewWriter(type, *pBuffer);
		if (pWriter) {
			SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(*pWriter));
			SCRIPT_WRITE_PACKET(pBuffer);
		} else
			SCRIPT_CALLBACK_THROW(String("Unsupported type ", Media::Data::TypeToString(type)));
	SCRIPT_CALLBACK_RETURN
}
static int hash(lua_State *pState) {
	SCRIPT_CALLBACK(ServerAPI, api)
		EVP_MD* pEVP = (EVP_MD*)lua_touserdata(pState, SCRIPT_READ_NEXT(1));
		if(pEVP) {
			SCRIPT_READ_PACKET(packet);
			shared<Buffer> pBuffer(SET, EVP_MD_size(pEVP));
			Crypto::Hash::Compute(pEVP, packet.data(), packet.size(), pBuffer->data());
			SCRIPT_WRITE_PACKET(pBuffer);
		} else
			SCRIPT_ERROR("Give hash algorithm in first argument as mona.md5, mona.sha256, etc...");
	SCRIPT_CALLBACK_RETURN
}
static int hmac(lua_State *pState) {
	SCRIPT_CALLBACK(ServerAPI, api)
		EVP_MD* pEVP = (EVP_MD*)lua_touserdata(pState, SCRIPT_READ_NEXT(1));
		if(pEVP) {
			SCRIPT_READ_PACKET(key);
			SCRIPT_READ_PACKET(packet);
			shared<Buffer> pBuffer(SET, EVP_MD_size(pEVP));
			Crypto::HMAC::Compute(pEVP, key.data(), key.size(), packet.data(), packet.size(), pBuffer->data());
			SCRIPT_WRITE_PACKET(pBuffer);
		} else
			SCRIPT_ERROR("Give hmac algorithm in first argument as mona.md5, mona.sha256, etc...");
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, ServerAPI& api) {
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE("__tab", AddObject<const Parameters>(pState, api));
		SCRIPT_DEFINE_FUNCTION("__call", &(LUAMap<ServerAPI, const Parameters>::Call<>));
		if(Byte::ORDER_NATIVE == Byte::ORDER_BIG_ENDIAN)
			SCRIPT_DEFINE_BOOLEAN("bigEndian", true)
		else
			SCRIPT_DEFINE_BOOLEAN("littleEndian", true)
		SCRIPT_DEFINE_FUNCTION("time", &time);
		SCRIPT_DEFINE_FUNCTION("newPath", &newPath);
		SCRIPT_DEFINE_FUNCTION("newIPAddress", &newIPAddress);
		SCRIPT_DEFINE_FUNCTION("newSocketAddress", &newSocketAddress);
		SCRIPT_DEFINE_FUNCTION("newUDPSocket", &newIOSocket<UDPSocket>);
		SCRIPT_DEFINE_FUNCTION("newTCPClient", &newIOSocket<TCPClient>);
		SCRIPT_DEFINE_FUNCTION("newTCPServer", &newIOSocket<TCPServer>);
		SCRIPT_DEFINE_FUNCTION("newWSClient", &newIOSocket<WSClient>);
#if defined(SRT_API)
		SCRIPT_DEFINE_FUNCTION("newSRTClient", &newIOSocket<SRT::Client>);
		SCRIPT_DEFINE_FUNCTION("newSRTServer", &newIOSocket<SRT::Server>);
#endif
		SCRIPT_DEFINE_FUNCTION("newSubscription", &newSubscription);
		SCRIPT_DEFINE_FUNCTION("newPublication", &newPublication);
		SCRIPT_DEFINE_FUNCTION("newMediaWriter", &newMediaWriter);
		SCRIPT_DEFINE_FUNCTION("newMediaReader", &newMediaReader);
		SCRIPT_DEFINE_FUNCTION("setTimer", &setTimer);
		SCRIPT_DEFINE("environment", AddObject(pState, Util::Environment()));
		SCRIPT_DEFINE("publications", AddObject(pState, api.publications()));

		SCRIPT_DEFINE_FUNCTION("toXML", &toXML);
		SCRIPT_DEFINE_FUNCTION("toData", toData);
		SCRIPT_DEFINE("toJSON", lua_pushinteger(pState, Media::Data::TYPE_JSON); lua_pushcclosure(pState, &toData, 1));
		SCRIPT_DEFINE("toAMF", lua_pushinteger(pState, Media::Data::TYPE_AMF); lua_pushcclosure(pState, &toData, 1));
		SCRIPT_DEFINE("toAMF0", lua_pushinteger(pState, Media::Data::TYPE_AMF0); lua_pushcclosure(pState, &toData, 1));
		SCRIPT_DEFINE("toQuery", lua_pushinteger(pState, Media::Data::TYPE_QUERY); lua_pushcclosure(pState, &toData, 1));
		SCRIPT_DEFINE("toXMLRPC", lua_pushinteger(pState, Media::Data::TYPE_XMLRPC); lua_pushcclosure(pState, &toData, 1));
		SCRIPT_DEFINE_FUNCTION("fromXML", &fromXML);
		SCRIPT_DEFINE_FUNCTION("fromData", fromData);
		SCRIPT_DEFINE("fromJSON", lua_pushinteger(pState, Media::Data::TYPE_JSON); lua_pushcclosure(pState, &fromData, 1));
		SCRIPT_DEFINE("fromAMF", lua_pushinteger(pState, Media::Data::TYPE_AMF); lua_pushcclosure(pState, &fromData, 1));
		SCRIPT_DEFINE("fromAMF0", lua_pushinteger(pState, Media::Data::TYPE_AMF0); lua_pushcclosure(pState, &fromData, 1));
		SCRIPT_DEFINE("fromQuery", lua_pushinteger(pState, Media::Data::TYPE_QUERY); lua_pushcclosure(pState, &fromData, 1));
		SCRIPT_DEFINE("fromXMLRPC", lua_pushinteger(pState, Media::Data::TYPE_XMLRPC); lua_pushcclosure(pState, &fromData, 1));

		SCRIPT_DEFINE("MD5", lua_pushlightuserdata(pState, (void*)EVP_md5()));
		SCRIPT_DEFINE("SHA1", lua_pushlightuserdata(pState, (void*)EVP_md5()));
		SCRIPT_DEFINE("SHA256", lua_pushlightuserdata(pState, (void*)EVP_md5()));
		SCRIPT_DEFINE_INT("MD5_SIZE", Crypto::MD5_SIZE);
		SCRIPT_DEFINE_INT("SHA1_SIZE", Crypto::SHA1_SIZE);
		SCRIPT_DEFINE_INT("SHA256_SIZE", Crypto::SHA256_SIZE);

		SCRIPT_DEFINE("hash", &hash);
		SCRIPT_DEFINE("hmac", &hmac);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, ServerAPI& api) {
	RemoveObject<const Parameters>(pState, api);
	RemoveObject(pState, Util::Environment());
	RemoveObject(pState, api.publications());
}

/*

int	LUAInvoker::Split(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		const char* expression = SCRIPT_READ_STRING("");
		const char* separator = SCRIPT_READ_STRING("");
		String::ForEach forEach([__pState](UInt32 index,const char* value) {
			SCRIPT_WRITE_STRING(value);
			return true;
		});
		String::Split(expression, separator, forEach,SCRIPT_READ_UINT(0));
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::Dump(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker, invoker)
		SCRIPT_READ_BINARY(name, sizeName)
		const UInt8* data(NULL);
		UInt32 sizeData(0);
		if (SCRIPT_NEXT_TYPE == LUA_TSTRING) {
			data = BIN lua_tostring(pState, ++__args);
			sizeData = lua_objlen(pState, __args);
		}
		if (name) {
			if (data) {
				UInt32 count(SCRIPT_READ_UINT(sizeData));
				const char* header(STR name);
				while (*header && !isspace(*header++));
				const char* endName(header);
				while (*header && isspace(*header++));
				if (*header) {
					SCOPED_STRINGIFY(name,header-endName,Logs::Dump(STR name, data, count > sizeData ? sizeData : count,header))
				} else
					Logs::Dump(STR name, data, count > sizeData ? sizeData : count);
			} else {
#if defined(_DEBUG)
				UInt32 count(SCRIPT_READ_UINT(sizeName));
				Logs::Dump(name, count > sizeName ? sizeName : count);
#else
				SCRIPT_WARN("debugging mona:dump(",name,",...) not removed")
#endif
			}
		}
	SCRIPT_CALLBACK_RETURN
}


int	LUAInvoker::Md5(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		while(SCRIPT_READ_AVAILABLE) {
			SCRIPT_READ_BINARY(data,size)
			if(data) {
				UInt8 result[16];
				EVP_Digest(data,size,result,NULL,EVP_md5(),NULL);
				SCRIPT_WRITE_BINARY(result, 16);
			} else {
				SCRIPT_ERROR("Input MD5 value have to be a string expression")
				SCRIPT_WRITE_NIL
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAInvoker::ListFiles(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(Invoker, invoker)
	
		UInt32 index = 0;
		lua_newtable(pState);
		FileSystem::ForEach forEach([&pState, &index](const string& path, UInt16 level){
			Script::NewObject<LUAFile>(pState, *new File(path));
			lua_rawseti(pState,-2,++index);
		});
		
		Exception ex;
		FileSystem::ListFiles(ex, SCRIPT_READ_STRING(invoker.wwwPath().c_str()), forEach);
		if (ex)
			SCRIPT_CALLBACK_THROW(ex)
	SCRIPT_CALLBACK_RETURN
}

int	LUAInvoker::Sha256(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		while(SCRIPT_READ_AVAILABLE) {
			SCRIPT_READ_BINARY(data,size)
			if(data) {
				UInt8 result[32];
				EVP_Digest(data,size,result,NULL,EVP_sha256(),NULL);
				SCRIPT_WRITE_BINARY(result,32);
			} else {
				SCRIPT_ERROR("Input SHA256 value have to be a string expression")
				SCRIPT_WRITE_NIL
			}
		}
	SCRIPT_CALLBACK_RETURN
}


int LUAInvoker::IndexConst(lua_State *pState) {
	SCRIPT_CALLBACK(Invoker,invoker)
		const char* name = SCRIPT_READ_STRING(NULL);
		if (name) {
			if(strcmp(name,"clients")==0) {
				Script::Collection(pState,1,"clients");
			} else if (strcmp(name, "dump") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Dump)
			}
				SCRIPT_WRITE_FUNCTION(LUAInvoker::FromData<QueryReader>)
			} else if (strcmp(name, "split") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Split)
			} else if(strcmp(name,"md5")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Md5)
			} else if(strcmp(name,"sha256")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::Sha256)
			} else if (strcmp(name,"listFiles")==0) {
				SCRIPT_WRITE_FUNCTION(LUAInvoker::ListFiles)
			}
		}
	SCRIPT_CALLBACK_RETURN
}
*/

}
