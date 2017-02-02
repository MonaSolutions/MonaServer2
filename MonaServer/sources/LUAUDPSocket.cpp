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

#include "LUAUDPSocket.h"
#include "LUASocketAddress.h"
#include "LUANetStats.h"
#include "Service.h"

using namespace std;
using namespace Mona;

map<string, pair<bool, lua_CFunction>> LUAUDPSocket::Methods({
	{ "connected",	 { false, &LUAUDPSocket::Connected} },
	{ "bound",		 { false, &LUAUDPSocket::Bound } },
	{ "address",	 { false, &LUAUDPSocket::Address } },
	{ "bandwidth",   { false, &LUANetStats<LUAUDPSocket>::PBandwidth } },
	{ "recvByteRate",{ false, &LUANetStats<LUAUDPSocket>::PRecvByteRate } },
	{ "recvTime",    { false, &LUANetStats<LUAUDPSocket>::PRecvTime } },
	{ "sendByteRate",{ false, &LUANetStats<LUAUDPSocket>::PSendByteRate } },
	{ "sendTime",    { false, &LUANetStats<LUAUDPSocket>::PSendTime } },

	{ "connect",	{ true, &LUAUDPSocket::Connect } },
	{ "disconnect", { true, &LUAUDPSocket::Disconnect } },
	{ "bind",		{ true, &LUAUDPSocket::Bind } },
	{ "send",		{ true, &LUAUDPSocket::Send } },
	{ "close",		{ true, &LUAUDPSocket::Close } }
});

LUAUDPSocket::LUAUDPSocket(SocketEngine& engine,bool allowBroadcast,lua_State* pState) : _pState(pState), UDPSocket(engine,allowBroadcast) {
	
	onError = [this](const Exception& ex) {
		SCRIPT_BEGIN(_pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(LUAUDPSocket, *this, "onError")
				SCRIPT_WRITE_STRING(ex)
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};

	onFlush = [this]() {
		SCRIPT_BEGIN(_pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(LUAUDPSocket, *this, "onFlush")
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};

	onPacket = [this](const Time& time, shared<Buffer>& pBuffer, const SocketAddress& address) {
		SCRIPT_BEGIN(_pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(LUAUDPSocket, *this, "onPacket")
				SCRIPT_WRITE_NUMBER(time)
				SCRIPT_WRITE_BINARY(pBuffer->data(),pBuffer->size())
				SCRIPT_WRITE_STRING(address.c_str())
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};

	
}

LUAUDPSocket::~LUAUDPSocket() {
	onPacket = nullptr;
	onFlush = nullptr;
	onError = nullptr;
}

void LUAUDPSocket::Clear(lua_State* pState, LUAUDPSocket& udp) {
	Script::ClearObject<LUASocketAddress>(pState, udp->address());
	Script::ClearObject<LUASocketAddress>(pState, udp->peerAddress());
}

int	LUAUDPSocket::Connected(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket, udp)
		SCRIPT_WRITE_BOOL(udp.connected());
	SCRIPT_CALLBACK_RETURN
}
int	LUAUDPSocket::Bound(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket, udp)
		SCRIPT_WRITE_BOOL(udp.bound());
	SCRIPT_CALLBACK_RETURN
}
int	LUAUDPSocket::Address(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket, udp)
		Script::AddObject<LUASocketAddress>(pState, udp->address());
	SCRIPT_CALLBACK_RETURN
}
int	LUAUDPSocket::PeerAddress(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket, udp)
		Script::AddObject<LUASocketAddress>(pState, udp->peerAddress());
	SCRIPT_CALLBACK_RETURN
}

int	LUAUDPSocket::Connect(lua_State* pState) {
	SCRIPT_CALLBACK_TRY(LUAUDPSocket,udp)
		Exception ex;
		SocketAddress address(IPAddress::Loopback(), 0);
		if (!LUASocketAddress::Read(ex, pState, SCRIPT_READ_NEXT(1), address) || !udp.connect(ex, address))
			SCRIPT_CALLBACK_THROW(ex)
		else
			SCRIPT_WRITE_BOOL(true)
		if(ex)
			SCRIPT_CALLBACK_THROW(ex)
	SCRIPT_CALLBACK_RETURN
}

int	LUAUDPSocket::Disconnect(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket,udp)
		udp.disconnect();
	SCRIPT_CALLBACK_RETURN
}

int	LUAUDPSocket::Bind(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket,udp)
		Exception ex;
		SocketAddress address;
		if (LUASocketAddress::Read(ex, pState, SCRIPT_READ_NEXT(1), address) && udp.bind(ex, address)) {
			if (ex)
				SCRIPT_WARN(ex)
			SCRIPT_WRITE_BOOL(true)
		} else {
			SCRIPT_ERROR(ex)
			SCRIPT_WRITE_BOOL(false)
		}
	SCRIPT_CALLBACK_RETURN
}

int	LUAUDPSocket::Send(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket,udp)
		SCRIPT_READ_BINARY(data,size)

		Exception ex;

		if (SCRIPT_READ_AVAILABLE) {
			SocketAddress address(IPAddress::Loopback(),0);
			if (LUASocketAddress::Read(ex, pState, SCRIPT_READ_NEXT(1), address) && udp.send(ex, data,size,address)) {
				if (ex)
					SCRIPT_WARN(ex)
				SCRIPT_WRITE_BOOL(true)
			} else {
				SCRIPT_ERROR(ex)
				SCRIPT_WRITE_BOOL(false)
			}
		} else if (udp.send(data, size)) {
			if (ex)
				SCRIPT_WARN(ex)
				SCRIPT_WRITE_BOOL(true)
		} else {
			SCRIPT_ERROR(ex)
			SCRIPT_WRITE_BOOL(false)
		}

	SCRIPT_CALLBACK_RETURN
}

int	LUAUDPSocket::Close(lua_State* pState) {
	SCRIPT_CALLBACK(LUAUDPSocket, udp)
		udp.close();
	SCRIPT_CALLBACK_RETURN
}
