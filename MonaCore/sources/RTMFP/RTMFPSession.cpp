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

#include "Mona/RTMFP/RTMFPSession.h"
#include "Mona/RTMFP/RTMFProtocol.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

RTMFPSession::RTMFPSession(RTMFProtocol& protocol, ServerAPI& api, const shared<Peer>& pPeer) : 
		_recvLostRate(_recvByteRate), _pFlow(NULL), _mainStream(api, *pPeer), _killing(0), _senderTrack(0), Session(protocol, pPeer), _nextWriterId(0), _timesKeepalive(0) {

	_mainStream.onStart = [this](UInt16 id, FlashWriter& writer) {
		// Stream Begin signal
		writer.writeRaw().write16(0).write32(id);
	};
	_mainStream.onStop = [this](UInt16 id, FlashWriter& writer) {
		// Stream EOF signal
		writer.writeRaw().write16(1).write32(id);
	};

	onAddress = [this](SocketAddress& address) {
		peer.setAddress(address);
	};
	onMessage = [this](RTMFP::Message& message) {
		_recvLostRate += message.lost;
		_recvByteRate += message.size();
		_recvTime.update();
		if (died || _killing)
			return;

		if (!message) {
			// flow end!
			DEBUG("Flow ", message.flowId, " consumed on session ", name());
			_flows.erase(message.flowId);
			return;
		}

		// flush previous flow 
		if (_pFlow)
			_pFlow->pWriter->flush();
		BinaryReader reader(message.data(), message.size());
		_pFlow = &_flows.emplace(message.flowId, peer).first->second;

		UInt8 type = reader.read8();
		Packet signature;
		if (type & RTMFP::MESSAGE_OPTIONS) {
			signature.set(reader.current(), type & 0x7F);
			_pFlow->streamId = BinaryReader(signature.data() + 4, signature.size() - 4).read7BitValue();
			reader.next(signature.size());
			if (reader.read8()>0) {
				// Fullduplex header part
				if (reader.read8() == 0x0A) {
					UInt64 idWriter = reader.read7BitLongValue();
					const auto& it = _writers.find(idWriter);
					if (it != _writers.end()) {
						// link the associated writer JUST if was not attached to a existing flow (writier init from server), to create duplex exchange
						if(it->second.unique())
							_pFlow->pWriter = it->second;
					} else
						WARN("Unknown writer ", idWriter, " associated to flow ", message.flowId, " on session ", name());
				} else
					WARN("Unknown fullduplex header part for flow ", message.flowId, " on session ", name());
				while (UInt8 length = reader.read8()) {
					WARN("Unknown options for flow ", message.flowId, " on session ", name());
					reader.next(length);
				}
			}
			type = reader.read8();
		}
		if (!_pFlow->pWriter)
			new RTMFPWriter(message.flowId, signature, *this);

		UInt32 time(0);
		switch (type) {
		case AMF::TYPE_AUDIO:
		case AMF::TYPE_VIDEO:
			time = reader.read32();
		case AMF::TYPE_CHUNKSIZE:
			break;
		default:
			reader.next(4);
			break;
		}

		// join group
		if (type == AMF::TYPE_CHUNKSIZE) { // trick to catch group in RTMFP (CHUNCKSIZE format doesn't exist in RTMFP)
			if (_pFlow->pGroup)
				peer.unjoinGroup(*_pFlow->pGroup);
			UInt32 size = reader.read7BitValue();
			if (size) {
				--size;
				if (reader.read8() == 0x10) {
					if (size > reader.available()) {
						ERROR("Bad header size for RTMFP group id");
						size = reader.available();
					}
					UInt8 groupId[Entity::SIZE];
					Crypto::Hash::SHA256(reader.current(), size, (unsigned char *)groupId);
					_pFlow->pGroup = &peer.joinGroup(groupId, _pFlow->pWriter.get());
				} else
					_pFlow->pGroup = &peer.joinGroup(reader.current(), _pFlow->pWriter.get());
			}
		} else {
			FlashStream* pStream = _mainStream.getStream(_pFlow->streamId);
			if (!pStream) {
				ERROR(name(), " indicates a non-existent ", _pFlow->streamId, " FlashStream");
				kill(ERROR_PROTOCOL);
				return;
			}
			pStream->process((AMF::Type)type, time, Packet(message,reader.current(),reader.available()), *_pFlow->pWriter, *this);
		}
	};
	onFlush = [this](RTMFP::Flush& flush) {
		_recvTime.update();
		if (died)
			return;
		// continue even if killing to get ACK!
		if (flush.died)
			return kill(flush.echoTime);

		// PING
		if (flush.echoTime != 0xFFFFFFFF) {
			UInt16 time = RTMFP::TimeNow() - flush.echoTime;
			if (time < 0x8000) { // otherwise invalid, see RTMFP spec.
				time *= RTMFP::TIMESTAMP_SCALE; // do it before to get the modulo UInt16
				peer.setPing(time);
			}
		}

		// KEEPALIVE
		if (flush.keepalive)
			_timesKeepalive = 0;
		// Writer ACK + FAIL
		for (auto& it : flush.acks) {
			auto& itWriter = _writers.find(it.first);
			if (itWriter == _writers.end()) {
				DEBUG("Writer ", it.first, " unfound on session ", name(), ", certainly an obsolete message (writer closed)");
				continue;
			}
			const Packet& ack(it.second);
			if (ack) {
				// ACK
				BinaryReader reader(ack.data(), ack.size());
				UInt64 stage(reader.read7BitLongValue());
				UInt32 lostCount(0);
				if (reader.available())
					lostCount += UInt32(reader.read7BitLongValue()) + 1;
				itWriter->second->acquit(stage, lostCount);
			} else // FAIL
				itWriter->second->fail("Writer rejected on session ", name());
		}

		if (_pFlow) {
			_pFlow->pWriter->flush();
			_pFlow = NULL;
		}
		// publication flush
		_mainStream.flush();
	};
}

UInt64 RTMFPSession::queueing() const {
	UInt64 queueing(pSenderSession->queueing);
	UInt32 bufferSize(pSenderSession->socket.sendBufferSize());
	// superior to buffer 0xFFFF to limit onFlush usage!
	return queueing > bufferSize ? queueing - bufferSize : 0;
}


void RTMFPSession::onParameters(const Parameters& parameters) {
	parameters.getNumber("keepalivePeer", _mainStream.keepalivePeer);
	parameters.getNumber("keepaliveServer", _mainStream.keepaliveServer);
}

void RTMFPSession::kill(Int32 error, const char* reason) {
	if (died)
		return;

	if (_mainStream.onStart) { // else if RTMFPHandshaking (no real peer)

		// no more reception, delete flows
		_flows.clear();
	
		// unpublish and unsubscribe
		_mainStream.onStart = nullptr;
		_mainStream.onStop = nullptr;
		_mainStream.clearStreams();

		peer.onDisconnection(); // keep it before _flowWriters deletion

		// close writers after disconnection but don't clear list to continue to try to relay last messages during killing steps!
		for (auto& it : _writers)
			it.second->close(error, reason);

		// start killing signal (after onDisconnection and writer close to allow to send last messages!)
		_killing = 10;
	}

	if ((error>0 || error==ERROR_CONGESTED) && pSenderSession) {  // If error supporting a last message AND not come from client (0)
		// If CONGESTED in RTMFP => we have erase queue messages, signal the death anyway!
		if (!manage()) // to start immediatly killing signal
			return;
		flush();
		if (error != ERROR_SERVER)
			return; // Now can check that dies message is received excepting on server close
	}

	Session::kill(error, reason);
	_writers.clear();
}

bool RTMFPSession::keepalive() {
	DEBUG("Keepalive server");
	if(_timesKeepalive==10) {
		WARN("Client failed, keepalive attempts timeout");
		kill(ERROR_IDLE);
		return false;
	}
	++_timesKeepalive;
	send(make_shared<RTMFPCmdSender>(0x01));
	return true;
}

bool RTMFPSession::manage() {

	if (!Session::manage())
		return false;

	if (_killing) {
		// killing signal!
		// no other message, just fail message, so I erase all data in first
		send(make_shared<RTMFPCmdSender>(0x0C));
		if (--_killing)
			return true;
		kill();
		return false;
	}

	// After 6 mn without any message we can considerate than the session has failed
	if(_recvTime.isElapsed(360000)) {
		WARN(name()," failed, reception timeout");
		kill(ERROR_IDLE);
		return false;
	}
	
	// To accelerate the deletion of peer ghost (mainly for netgroup efficient), starts a keepalive server after 2 mn
	if (_recvTime.isElapsed(120000)) {
		if (!pSenderSession) {
			kill(ERROR_CONGESTED); // timeout connection without response possible, client congested? client canceled?
			return false;
		}
		if (!peer.connected) {
			WARN(name(), " failed, connection client timeout");
			kill(ERROR_IDLE);
			return false;
		}
		if (!keepalive())
			return false;
	}

	// After Session::manage, Session ::flush is called!
	return true;
}

void RTMFPSession::flush() {
	// publication flush
	_mainStream.flush();

	// Raise RTMFPWriter
	auto& it(_writers.begin());
	Exception ex;
	while (it != _writers.end()) {
		shared<RTMFPWriter>& pWriter(it->second);
		pWriter->flush();
		if (pWriter->consumed()) {
			DEBUG("Delete writer ", pWriter->id(), " on session ", name());
			it = _writers.erase(it);
		} else
			++it;
	}
}

void RTMFPSession::send(const shared<RTMFPSender>& pSender) {
	// continue even on _killing to repeat writers messages to flush it (reliable)
	pSender->address = peer.address;
	pSender->pSession = pSenderSession;
	Exception ex;
	AUTO_ERROR(api.threadPool.queue(ex, pSender, _senderTrack), name());
}

UInt64 RTMFPSession::newWriter(RTMFPWriter* pWriter) {
	// emplace with a new id
	map<UInt64, shared<RTMFPWriter>>::iterator it;
	for (;;) {
		if (++_nextWriterId == 0)
			continue;
		auto& it = _writers.emplace(_nextWriterId, pWriter);
		if (it.second) {
			if (_pFlow && !_pFlow->pWriter)
				_pFlow->pWriter = it.first->second;
			break;
		}
	}
	DEBUG("New writer ", _nextWriterId, " on session ", name());
	return _nextWriterId;
}

UInt64 RTMFPSession::resetWriter(UInt64 id) {
	// remove previous version
	const auto& it(_writers.find(id));
	while (++_nextWriterId == 0 || !_writers.emplace(_nextWriterId, it->second).second);
	_writers.erase(it);
	return _nextWriterId;
}


} // namespace Mona
