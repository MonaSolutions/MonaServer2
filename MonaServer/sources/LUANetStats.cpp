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

#include "LUANetStats.h"

using namespace std;
using namespace Mona;

int	LUANetStats::RecvTime(lua_State *pState) {
	SCRIPT_CALLBACK(NetStats, netStats)
		SCRIPT_WRITE_NUMBER(netStats.recvTime())
	SCRIPT_CALLBACK_RETURN
}

/*
virtual UInt64	bandwidth() const = 0;

virtual Time	recvTime() const = 0;
virtual UInt64	recvByteRate() const = 0;
virtual UInt64  recvLostRate() const { return 0; }

virtual Time    sendTime() const = 0;
virtual UInt64	sendByteRate() const = 0;
virtual UInt64  sendLostRate() const { return 0; }*/


int	LUAUDPSocket::Bandwidth(lua_State* pState) { SCRIPT_WRITE_BOOL(udp.connected());
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

int LUANetStats::IndexConst(lua_State *__pState, const char* name, NetStats& netStats) {
	if (strcmp(name, "bandwidth") == 0) {
		SCRIPT_WRITE_NUMBER(netStats.bandwidth());
	} else if (strcmp(name, "recvTime") == 0) {
		SCRIPT_WRITE_NUMBER(listener.receiveVideo);
	} else if (strcmp(name, "recvByteRate") == 0) {
		SCRIPT_WRITE_NUMBER(listener.receiveVideo);
	} else if (strcmp(name, "recvLostRate") == 0) {
		SCRIPT_WRITE_NUMBER(listener.receiveVideo);
	} else if (strcmp(name, "sendTime") == 0) {
		SCRIPT_WRITE_NUMBER(listener.receiveVideo);
	} else if (strcmp(name, "sendByteRate") == 0) {
		SCRIPT_WRITE_NUMBER(listener.receiveVideo);
	} else if (strcmp(name, "sendLostRate") == 0) {
		SCRIPT_WRITE_NUMBER(listener.receiveVideo);
	}
}

int LUANetStats::IndexConst(lua_State *__pState, const char* name, NetStats& netStats) {
	if (strcmp(name, "audioQOS") == 0) {
		Script::AddObject<LUAQualityOfService>(pState, listener.audioQOS());
	} else if (strcmp(name, "videoQOS") == 0) {
		Script::AddObject<LUAQualityOfService>(pState, listener.videoQOS());
	} else if (strcmp(name, "dataQOS") == 0) {
		Script::AddObject<LUAQualityOfService>(pState, listener.dataQOS());
	} else if (strcmp(name, "setReceiveAudio") == 0) {
		SCRIPT_WRITE_FUNCTION(LUAListener::SetReceiveAudio)
	} else if (strcmp(name, "setReceiveVideo") == 0) {
		SCRIPT_WRITE_FUNCTION(LUAListener::SetReceiveVideo)
	} else if (strcmp(name, "setReceiveData") == 0) {
		SCRIPT_WRITE_FUNCTION(LUAListener::SetReceiveData)
	} else if (strcmp(name, "client") == 0) {
		Script::AddObject<LUAClient>(pState, listener.client);
	} else if (strcmp(name, "parameters") == 0) {
		Script::Collection(pState, 1, "parameters");
	} else {
		Script::Collection(pState, 1, "parameters");
		lua_getfield(pState, -1, name);
		lua_replace(pState, -2);
	}
}

