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
#include "Mona/UDPSocket.h"
#include "Mona/FlashMainStream.h"
#include "Mona/RTMFP/RTMFPWriter.h"

namespace Mona {

struct RTMFProtocol;
struct RTMFPSession : RTMFP::Output, Session, Net::Stats, virtual Object {
	RTMFPSession(RTMFProtocol& protocol, ServerAPI& api, const shared<Peer>& pPeer);

	shared<RTMFPSender::Session> pSenderSession;

	RTMFP::Session::OnAddress	onAddress;
	RTMFP::Session::OnMessage	onMessage;
	RTMFP::Session::OnFlush		onFlush;

	// Implementation of Net::Stats
	Time				recvTime() const { return _recvTime; }
	UInt64				recvByteRate() const { return _recvByteRate; }
	double				recvLostRate() const { return _recvLostRate; }
	Time				sendTime() const { return pSenderSession->sendTime.load(); }
	UInt64				sendByteRate() const { return pSenderSession->sendByteRate; }
	double				sendLostRate() const { return pSenderSession->sendLostRate; }
	UInt64				queueing() const;

	void				kill(Int32 error = 0, const char* reason = NULL);

private:
	struct Flow : virtual Object {
		Flow(Peer& peer) : pGroup(NULL), streamId(0), peer(peer) {}
		~Flow() { if (pGroup) peer.unjoinGroup(*pGroup); }
		shared<RTMFPWriter>	pWriter;
		Group*				pGroup;
		UInt16				streamId;
		Peer&				peer;
	};

	void				onParameters(const Parameters& parameters);
	bool				keepalive();
	bool				manage();
	void				flush();
	

	// void				writeP2PHandshake(const std::string& tag, const SocketAddress& address, RTMFP::AddressType type);

	// Implementation of RTMFPOutput
	shared<RTMFPWriter>	newWriter(UInt64 flowId, const Packet& signature);
	UInt64				resetWriter(UInt64 id);
	UInt32				rto() const { return peer.rto(); }
	void				send(const shared<RTMFPSender>& pSender);

	UInt8									_killing;
	UInt8									_timesKeepalive;

	FlashMainStream							_mainStream;
	std::map<UInt64,Flow>					_flows;
	std::map<UInt64, shared<RTMFPWriter>>	_writers;
	UInt64									_nextWriterId;
	UInt16									_senderTrack;
	Flow*									_pFlow;

	Time									_recvTime;
	ByteRate								_recvByteRate;
	LostRate								_recvLostRate;
};


} // namespace Mona
