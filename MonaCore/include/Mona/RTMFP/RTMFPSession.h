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
	RTMFPSession(RTMFProtocol& protocol, ServerAPI& api, shared<Peer>& pPeer);

	void init(const shared<RTMFP::Session>& pSession);


	void kill(Int32 error = 0, const char* reason = NULL);

private:
	struct Flow : virtual Object, RTMFP::Member {
		Flow(Client& client, const UInt8* memberId) : _pGroup(NULL), streamId(0), _client(client), RTMFP::Member(memberId) {}
		~Flow() { unjoin(); pWriter->close(); }

		UInt16				streamId;
		shared<RTMFPWriter>	pWriter;

		bool  join(RTMFP::Group& group);
		void  unjoin();
	private:
		Client&			client() { return _client; }
		RTMFPWriter&	writer() { return *pWriter; }
		RTMFP::Group*	_pGroup;
		Client&			_client;
	};

	// Implementation of Net::Stats
	Time				recvTime() const { return _recvTime; }
	UInt64				recvByteRate() const { return _recvByteRate; }
	double				recvLostRate() const { return _recvLostRate; }
	Time				sendTime() const { return _pSenderSession->sendTime.load(); }
	UInt64				sendByteRate() const { return _pSenderSession->sendByteRate; }
	double				sendLostRate() const { return _pSenderSession->sendLostRate; }
	UInt64				queueing() const;

	void				onParameters(const Parameters& parameters);
	bool				keepalive();
	bool				manage();
	void				flush();
	

	// Implementation of RTMFPOutput
	shared<RTMFPWriter>	newWriter(UInt64 flowId, const Packet& signature);
	UInt64				resetWriter(UInt64 id);
	UInt32				rto() const { return peer.rto(); }
	void				send(shared<RTMFPSender>&& pSender);

	UInt8									_killing;
	UInt8									_timesKeepalive;

	FlashMainStream							_mainStream;
	std::map<UInt64,Flow>					_flows;
	std::map<UInt64, shared<RTMFPWriter>>	_writers;
	UInt64									_nextWriterId;
	UInt16									_senderTrack;
	Flow*									_pFlow;

	shared<RTMFP::Session>					_pSession;
	shared<RTMFPSender::Session>			_pSenderSession;

	Time									_recvTime;
	ByteRate								_recvByteRate;
	LostRate								_recvLostRate;
};


} // namespace Mona
