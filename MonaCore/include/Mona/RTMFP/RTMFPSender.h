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
#include "Mona/Socket.h"
#include "Mona/Runner.h"
#include "Mona/AMFWriter.h"
#include "Mona/LostRate.h"
#include "Mona/RTMFP/RTMFP.h"

namespace Mona {

struct RTMFPSender : Runner, virtual Object {
	struct Packet : Mona::Packet, virtual Object {
		Packet(shared<Buffer>& pBuffer, UInt32 fragments, bool reliable) : fragments(fragments), Mona::Packet(pBuffer), reliable(reliable), _sizeSent(0) {}
		bool setSent() {
			if (_sizeSent)
				return false;
			_sizeSent = size();
			if (!reliable)
				Mona::Packet::reset(); // release immediatly an unreliable packet!
			return true;
		}
		const bool   reliable;
		const UInt32 fragments;
		UInt32 sizeSent() const { return _sizeSent; }
	private:
		UInt32 _sizeSent;
	};
	struct Session : virtual Object {
		Session(const shared<RTMFP::Session>& pSession, const shared<Socket>& pSocket) : _pSocket(pSocket), _pSession(pSession), sendLostRate(sendByteRate), sendTime(0) {}
		UInt32			id() const { return _pSession->id; }
		UInt32			farId() const { return _pSession->farId; }
		RTMFP::Engine&	encoder() { return *_pSession->pEncoder; }
		Socket&			socket() { return *_pSocket; }
		std::atomic<Int64>			sendTime;
		ByteRate					sendByteRate;
		LostRate					sendLostRate;
	private:
		shared<RTMFP::Session>		_pSession;
		shared<Socket>				_pSocket;
	};
	struct Queue : virtual Object, std::deque<Packet> {
		template<typename SignatureType>
		Queue(UInt64 id, UInt64 flowId, const SignatureType& signature) : id(id), stage(0), stageQueue(0), signature(STR signature.data(), signature.size()), flowId(flowId) {}
		const UInt64		id;
		const UInt64		flowId;
		const std::string	signature;
		// used by RTMFPSender
		/// stageQueue <= stageAck <= stage
		Congestion			congestion;
		UInt64				stage;
		UInt64				stageQueue;
	};

	// Repeat usage!
	RTMFPSender(const shared<Queue>& pQueue, UInt8 fragments=0) : Runner("RTMFPSender"), _pQueue(pQueue), _fragments(fragments), _sendable(RTMFP::SENDABLE_MAX) {}
	// Command usage!
	RTMFPSender(UInt8 command = 0) : Runner("RTMFPCmdSender"), _sendable(command) {}

	SocketAddress	address; // can change!
	shared<Session>	pSession;
	UInt64			stageAck;

protected:
	// Messenger usage!
	RTMFPSender(const char* name, const shared<Queue>& pQueue) : Runner(name), _fragments(0), _sendable(pQueue->empty() ? RTMFP::SENDABLE_MAX : 0), _pQueue(pQueue) {}

	bool send(Packet& packet);
private:
	bool run(Exception& ex);
	virtual void run(Queue& queue) {}
	void sendAbandon(UInt64 stage);

	shared<Queue>	_pQueue;
	UInt8			_sendable;
	UInt8			_fragments;
};


struct RTMFPMessenger : RTMFPSender, virtual Object {
	RTMFPMessenger(const shared<RTMFPSender::Queue>& pQueue) :
		RTMFPSender("RTMFPMessenger", pQueue) {}

	AMFWriter&	newMessage(bool reliable, const Mona::Packet& packet) { _messages.emplace_back(reliable, packet); return _messages.back().writer; }

private:
	struct Message : private shared<Buffer>, virtual NullableObject {
		Message(bool reliable, const Mona::Packet& packet) : reliable(reliable), packet(std::move(packet)), writer(*new Buffer()) { reset(&writer->buffer()); }
		bool				reliable;
		AMFWriter			writer; // data
		const Mona::Packet	packet; // footer
		explicit operator bool() const { return packet || writer ? true : false; }
	};
	UInt32	headerSize(Queue& queue);
	void	run(Queue& queue);
	void	write(Queue& queue, const Message& message);
	void	flush(Queue& queue);

	std::deque<Message>	_messages;

	// current buffer =>
	shared<Buffer>		_pBuffer;
	UInt32				_fragments;
	UInt8				_flags;
};


} // namespace Mona
