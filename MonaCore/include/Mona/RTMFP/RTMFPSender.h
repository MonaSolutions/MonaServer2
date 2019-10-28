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
		void setSent() {
			if (_sizeSent)
				return;
			_sizeSent = size();
			if (!reliable)
				Mona::Packet::reset(); // release immediatly an unreliable packet!
		}
		const bool   reliable;
		const UInt32 fragments;
		UInt32		 sizeSent() const { return _sizeSent; }
	private:
		UInt32 _sizeSent;
	};
	struct Session : virtual Object {
		Session(const shared<RTMFP::Session>& pSession, const shared<Socket>& pSocket) :
			sendable(RTMFP::SENDABLE_MAX), socket(*pSocket), pEncoder(SET, *pSession->pEncoder),
			queueing(0), _pSocket(pSocket), _pSession(pSession), sendLostRate(sendByteRate), sendTime(0) {}
		UInt32					id() const { return _pSession->id; }
		UInt32					farId() const { return _pSession->farId; }
		std::atomic<Int64>&		initiatorTime() { return _pSession->initiatorTime; }
		shared<RTMFP::Engine>	pEncoder;
		Socket&					socket;
		std::atomic<Int64>		sendTime;
		ByteRate				sendByteRate;
		LostRate				sendLostRate;
		std::atomic<UInt64>		queueing;
		UInt8					sendable;
	private:
		shared<Socket>			_pSocket;
		shared<RTMFP::Session>	_pSession;
	};
	struct Queue : virtual Object, std::deque<shared<Packet>> {
		template<typename SignatureType>
		Queue(UInt64 id, UInt64 flowId, const SignatureType& signature) : id(id), stage(0), stageSending(0), stageAck(0), signature(STR signature.data(), signature.size()), flowId(flowId) {}

		const UInt64				id;
		const UInt64				flowId;
		const std::string			signature;
		// used by RTMFPSender
		/// stageAck <= stageSending <= stage
		UInt64						stage;
		UInt64						stageSending;
		UInt64						stageAck;
		std::deque<shared<Packet>>	sending;
	};

	// Flush usage!
	RTMFPSender(const shared<Queue>& pQueue) : Runner("RTMFPSender"), pQueue(pQueue) {}

	SocketAddress	address; // can change!
	shared<Session>	pSession;

protected:
	RTMFPSender(const char* name, const shared<Queue>& pQueue) : Runner(name), pQueue(pQueue) {}
	RTMFPSender(const char* name) : Runner(name) {}

	shared<Queue>	pQueue;

private:
	bool		 run(Exception& ex);
	virtual void run() {}
};

struct RTMFPCmdSender : RTMFPSender, virtual Object {
	RTMFPCmdSender(UInt8 cmd) : RTMFPSender("RTMFPCmdSender"), _cmd(cmd) {}
private:
	void	run();

	UInt8	_cmd;
};

struct RTMFPAcquiter : RTMFPSender, virtual Object {
	RTMFPAcquiter(const shared<RTMFPSender::Queue>& pQueue, UInt64 stageAck) : RTMFPSender("RTMFPAcquiter", pQueue), _stageAck(stageAck) {}
private:
	void	run();

	UInt64	_stageAck;
};

struct RTMFPRepeater : RTMFPSender, virtual Object {
	RTMFPRepeater(const shared<RTMFPSender::Queue>& pQueue, UInt8 fragments = 0) : RTMFPSender("RTMFPRepeater", pQueue), _fragments(fragments) {}
private:
	void	run();
	void	sendAbandon(UInt64 stage);

	UInt8	_fragments;
};


struct RTMFPMessenger : RTMFPSender, virtual Object {
	RTMFPMessenger(const shared<RTMFPSender::Queue>& pQueue) : RTMFPSender("RTMFPMessenger", pQueue), _flags(0) {} // _flags must be initialized to 0!

	AMFWriter&	newMessage(bool reliable, Media::Data::Type type, const Mona::Packet& packet) { _messages.emplace_back(reliable, type, packet); return _messages.back().writer; }

private:
	struct Message : private shared<Buffer>, virtual Object {
		NULLABLE(!packet && !writer)
		Message(bool reliable, Media::Data::Type type, const Mona::Packet& packet) : shared<Buffer>(SET), type(type), reliable(reliable), packet(std::move(packet)), writer(*self) {}
		bool				reliable;
		AMFWriter			writer; // data
		Media::Data::Type	type;
		Mona::Packet		packet; // footer
		void				convertToAMF() { type = writer.convert(type, packet); }
	};
	UInt32	headerSize();
	void	run();
	void	flush();

	std::deque<Message>	_messages;

	// current buffer (similar to variable local, trick to avoid to pass it in flush method as params) =>
	shared<Buffer>		_pBuffer;
	UInt32				_fragments;
	UInt8				_flags;
};


} // namespace Mona
