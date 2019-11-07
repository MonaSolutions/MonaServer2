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
#include "Mona/SRT.h"

using namespace std;

namespace Mona {

#if defined(SRT_API)

template<bool isSend>
static int encryptionState(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		int state;
		if (isSend ? socket.getRecvEncryptionState(ex, (SRT_KM_STATE&)state) : socket.getSendEncryptionState(ex, (SRT_KM_STATE&)state)) {
			static const char* States[] = { "unsecured", "securing", "secured", "nosecret", "badsecret" };
			if (state < int(sizeof(States) / sizeof(States[0]))) {
				if (ex)
					SCRIPT_CALLBACK_THROW(ex);
				SCRIPT_WRITE_STRING(States[state]);
			} else
				SCRIPT_CALLBACK_THROW(String("Unknown SRT encryption state ", state));
		} else if (ex)
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int encryptionSize(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		if (SCRIPT_NEXT_READABLE) {
			if(!socket.setEncryptionSize(ex, SCRIPT_READ_UINT8(0)) || ex)
				SCRIPT_CALLBACK_THROW(ex);
		}
		UInt32 size;
		if (socket.getEncryptionSize(ex, size)) {
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_INT(size);
		} else if (ex)
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
template<bool isPeer=false>
static int latency(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		UInt32 latency;
		if (SCRIPT_NEXT_READABLE) {
			latency = SCRIPT_READ_UINT32(0);
			if (!(isPeer ? socket.setPeerLatency(ex, latency) : socket.setLatency(ex, latency)) || ex)
				SCRIPT_CALLBACK_THROW(ex);
		}
		if (socket.getLatency(ex, latency)) {
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_INT(latency);
		} else if (ex)
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int mss(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		if (SCRIPT_NEXT_READABLE && (!socket.setMSS(ex, SCRIPT_READ_UINT32(0xFFFF)) || ex))
			SCRIPT_CALLBACK_THROW(ex);
		UInt32 mss;
		if (socket.getMSS(ex, mss)) {
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_INT(mss);
		} else if (ex)
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int overheadBW(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		if (SCRIPT_NEXT_READABLE && (!socket.setOverheadBW(ex, SCRIPT_READ_UINT8(25)) || ex))
			SCRIPT_CALLBACK_THROW(ex);
		UInt32 overheadBW;
		if (socket.getOverheadBW(ex, overheadBW)) {
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_INT(overheadBW);
		} else if (ex)
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int maxBW(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		if (SCRIPT_NEXT_READABLE && (!socket.setMaxBW(ex, SCRIPT_READ_UINT32(-1)) || ex))
			SCRIPT_CALLBACK_THROW(ex);
		Int64 maxBW;
		if (socket.getMaxBW(ex, maxBW)) {
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_INT(maxBW);
		} else if (ex)
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int streamId(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		if (SCRIPT_NEXT_READABLE) {
			Exception ex;
			SCRIPT_READ_DATA(data, size);
			if (data && (!socket.setStreamId(ex, STR data, size) || ex))
				SCRIPT_CALLBACK_THROW(ex);
		}
		SCRIPT_WRITE_STRING(socket.streamId());
	SCRIPT_CALLBACK_RETURN;
}
static int passphrase(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		if (SCRIPT_NEXT_READABLE) {
			Exception ex;
			SCRIPT_READ_DATA(data, size);
			if (data && (!socket.setPassphrase(ex, STR data, size) || ex))
				SCRIPT_CALLBACK_THROW(ex);
		}
		SCRIPT_WRITE_STRING(socket.streamId());
	SCRIPT_CALLBACK_RETURN;
}
static int pktDrop(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		if(SCRIPT_NEXT_READABLE && (!socket.setPktDrop(ex, SCRIPT_READ_BOOLEAN(false)) || ex))
			SCRIPT_CALLBACK_THROW(ex);
		bool value;
		if (socket.getPktDrop(ex, value)) {
			SCRIPT_WRITE_BOOLEAN(value);
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int getStats(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(SRT::Socket, socket);
		Exception ex;
		SRT::Stats stats;
		if (socket.getStats(ex, stats)) {
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_TABLE_BEGIN(0)
				SCRIPT_DEFINE_DOUBLE("bandwidthEstimated", stats.bandwidthEstimated());
				SCRIPT_DEFINE_DOUBLE("bandwidthMaxUsed", stats.bandwidthMaxUsed());
				SCRIPT_DEFINE_INT("recvBufferTime", stats.recvBufferTime());
				SCRIPT_DEFINE_INT("recvNegotiatedDelay", stats.recvNegotiatedDelay());
				SCRIPT_DEFINE_DOUBLE("recvLostCount", stats.recvLostCount());
				SCRIPT_DEFINE_DOUBLE("recvLostRate", stats.recvLostRate());
				SCRIPT_DEFINE_INT("recvDropped", stats.recvDropped());
				SCRIPT_DEFINE_INT("sendBufferTime", stats.sendBufferTime());
				SCRIPT_DEFINE_INT("sendNegotiatedDelay", stats.sendNegotiatedDelay());
				SCRIPT_DEFINE_DOUBLE("sendLostCount", stats.sendLostCount());
				SCRIPT_DEFINE_DOUBLE("sendLostRate", stats.sendLostRate());
				SCRIPT_DEFINE_INT("sendDropped", stats.sendDropped());
				SCRIPT_DEFINE_DOUBLE("retransmitRate", stats.retransmitRate());
				SCRIPT_DEFINE_DOUBLE("rtt", stats.rtt());
				SCRIPT_DEFINE_DOUBLE("recvTime", stats.recvTime());
				SCRIPT_DEFINE_DOUBLE("recvByteRate", stats.recvByteRate());
				SCRIPT_DEFINE_DOUBLE("sendTime", stats.sendTime());
				SCRIPT_DEFINE_DOUBLE("sendByteRate", stats.sendByteRate());
				SCRIPT_DEFINE_DOUBLE("queueing", stats.queueing());
			SCRIPT_TABLE_END
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}

template<> void Script::ObjInit(lua_State *pState, SRT::Socket& socket) {
	AddType<Socket>(pState, socket);
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_STRING("type", "srt");
		SCRIPT_DEFINE_FUNCTION("streamId", &streamId);
		SCRIPT_DEFINE_FUNCTION("recvEncryptionState", &encryptionState<false>);
		SCRIPT_DEFINE_FUNCTION("sendEncryptionState", &encryptionState<true>);
		SCRIPT_DEFINE_FUNCTION("encryptionSize", &encryptionSize);
		SCRIPT_DEFINE_FUNCTION("latency", &latency);
		SCRIPT_DEFINE_FUNCTION("peerLatency", &latency<true>);
		SCRIPT_DEFINE_FUNCTION("mss", &mss);
		SCRIPT_DEFINE_FUNCTION("overheadBW", &overheadBW);
		SCRIPT_DEFINE_FUNCTION("maxBW", &maxBW);
		SCRIPT_DEFINE_FUNCTION("passphrase", &passphrase);
		SCRIPT_DEFINE_FUNCTION("pktDrop", &pktDrop);
		SCRIPT_DEFINE_FUNCTION("getStats", &getStats);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, SRT::Socket& socket) {
	RemoveType<Socket>(pState, socket);
}

#endif

}
