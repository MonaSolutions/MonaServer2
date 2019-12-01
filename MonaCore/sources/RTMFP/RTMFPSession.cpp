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
#include "Mona/BinaryReader.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

bool RTMFPSession::Flow::join(RTMFP::Group& group) {
	if (_pGroup == &group)
		return false;
	unjoin();
	_pGroup = &group;
	group.join(*this);
	return true;
}
void RTMFPSession::Flow::unjoin() {
	if (!_pGroup)
		return;
	_pGroup->unjoin(*this);
	_pGroup = NULL;
}

RTMFPSession::RTMFPSession(RTMFProtocol& protocol, ServerAPI& api, shared<Peer>& pPeer) : 
		_recvLostRate(_recvByteRate), _pFlow(NULL), _mainStream(api, peer), _killing(0), _senderTrack(0), Session(protocol, pPeer), _nextWriterId(0), _timesKeepalive(0) {

	_mainStream.onStart = [](UInt16 id, FlashWriter& writer) {
		// Stream Begin signal
		writer.writeRaw().write16(0).write32(id);
	};
	_mainStream.onStop = [](UInt16 id, FlashWriter& writer) {
		// Stream EOF signal
		writer.writeRaw().write16(1).write32(id);
	};
}

void RTMFPSession::init(const shared<RTMFP::Session>& pSession) {
	memcpy(BIN peer.id, pSession->peerId, Entity::SIZE);
	_pSession = pSession;
	_pSenderSession.set(pSession, protocol<RTMFProtocol>().socket());

	_pSession->onAddress = [this](SocketAddress& address) {
		// onAddress is defined on _pSession so 
		peer.setAddress(address);
		_pSession->pRendezVous->set<RTMFP::Session>(peer.id, peer.address, peer.serverAddress, _pSession.get());
	};
	_pSession->onMessage = [this](RTMFP::Message& message) {
		_recvLostRate += message.lost;
		_recvByteRate += message.size();
		_recvTime.update();
	
		// flush previous flow 
		if (_pFlow)
			_pFlow->pWriter->flush();
		if (!message) {
			_pFlow = NULL;
			// flow end!
			DEBUG("Flow ", message.flowId, " consumed on session ", name());
			_flows.erase(message.flowId);
			return;
		}
		// find or create flow requested
		const auto& it = _flows.lower_bound(message.flowId);
		if (it == _flows.end() || it->first != message.flowId)
			_pFlow = &_flows.emplace_hint(it, SET, forward_as_tuple(message.flowId), forward_as_tuple(peer, _pSession->memberId))->second;
		else
			_pFlow = &it->second;

		BinaryReader reader(message.data(), message.size());
		UInt8 type = reader.read8();
		Packet signature;
		if (type & RTMFP::MESSAGE_OPTIONS) {
			signature.set(reader.current(), type & 0x7F);
			BinaryReader streamReader(signature.data(), signature.size());
			streamReader.next(4);
			_pFlow->streamId = range<UInt16>(streamReader.read7Bit<UInt64>(10));
			reader.next(signature.size());
			while (UInt8 length = reader.read8())
				reader.next(length);
			type = reader.read8();
		}
		if (!_pFlow->pWriter) // if not pWriter, it has just been created now
			_pFlow->pWriter = newWriter(_pFlow->streamId ? 2 : message.flowId, signature);

		// join group
		if (type == AMF::TYPE_CHUNKSIZE) { // trick to catch group in RTMFP (CHUNCKSIZE format doesn't exist in RTMFP)
			UInt32 size = range<UInt32>(reader.read7Bit<UInt64>(10));
			Entity::Map<RTMFP::Group>& groups(protocol<RTMFProtocol>().groups);
			Entity::Map<RTMFP::Group>::iterator it;
			const UInt8* groupId;
			if (reader.read8() == 0x10) {
				if (--size > reader.available()) {
					ERROR("Bad header size for RTMFP group id");
					size = reader.available();
				}
				// happens just on old flash version, otherwise is already a SHA256 (gain in peformance)
				thread_local UInt8 ID[Entity::SIZE];
				Crypto::Hash::SHA256(reader.current(), size, ID);
				groupId = ID;
			} else {
				if (--size != Entity::SIZE) {
					ERROR("Bad size for RTMFP group id");
					return;
				}
				groupId = reader.current();
			}
			it = groups.lower_bound(groupId);
			if (it == groups.end() || memcmp(it->first, groupId, Entity::SIZE) != 0)
				it = groups.emplace_hint(it, *new RTMFP::Group(groupId, groups));
			if (_pFlow->join(*it->second)) {
				// warn servers (scalability)
				shared<Buffer> pBuffer;
				BinaryWriter writer(protocol<RTMFProtocol>().initBuffer(pBuffer));
				writer.write(peer.id, Entity::SIZE).write(_pSession->memberId, Entity::SIZE).write(it->second->id, Entity::SIZE);
				RTMFP::WriteAddress(writer, peer.serverAddress, RTMFP::LOCATION_PUBLIC);
				std::set<SocketAddress> addresses = protocol<RTMFProtocol>().addresses;
				protocol<RTMFProtocol>().send(0x10, pBuffer, addresses);
			}
			return;
		}

		FlashStream* pStream = _mainStream.getStream(_pFlow->streamId);
		if (!pStream) {
			ERROR(name(), " indicates a non-existent ", _pFlow->streamId, " FlashStream");
			kill(ERROR_PROTOCOL);
			return;
		}

		UInt32 time = reader.read32();
		message += reader.position();
		// Capture setPeerInfo!
		switch (type) {
			case AMF::TYPE_INVOCATION_AMF3:
				reader.next();
			case AMF::TYPE_INVOCATION: {
				string buffer;
				AMFReader amf(Packet(message, reader.current(), reader.available()));
				if (amf.readString(buffer) && buffer == "setPeerInfo") {
					amf.read(DataReader::NUMBER, DataWriter::Null()); // callback number, useless here because no response
					amf.readNull();
					set<SocketAddress> addresses;
					while (amf.readString(buffer)) {
						if (buffer.empty())
							continue; // can happen, not display exception
						SocketAddress address;
						Exception ex;
						if (address.set(ex, buffer))
							addresses.emplace(address);
						if (ex)
							WARN("Invalid peer address info, ", ex);
					}
					if (!addresses.empty())
						_pSession->pRendezVous->set<RTMFP::Session>(peer.id, peer.address, peer.serverAddress, addresses, _pSession.get());
				}
			}
		}
		pStream->process((AMF::Type)type, time, message, *_pFlow->pWriter, *this);
	};
	_pSession->onFlush = [this](RTMFP::Flush& flush) {
		_recvTime.update();

		if (flush.died)
			return kill(flush.ping);

		// PING
		if (flush.ping >= 0)
			peer.setPing(flush.ping);

		// KEEPALIVE
		if (flush.keepalive)
			_timesKeepalive = 0;
		// Writer ACK + FAIL
		for (auto& it : flush.acks) {
			const auto& itWriter = _writers.find(it.first);
			if (itWriter == _writers.end()) {
				DEBUG("Writer ", it.first, " unfound on session ", name(), " or writer already closed");
				continue;
			}
			const Packet& ack(it.second);
			if (ack) {
				// ACK
				BinaryReader reader(ack.data(), ack.size());
				UInt64 stage(reader.read7Bit<UInt64>(10));
				UInt32 lostCount(0);
				if (reader.available())
					lostCount += UInt32(reader.read7Bit<UInt64>(10)) + 1;
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
	UInt64 queueing(_pSenderSession->queueing);
	UInt32 bufferSize(_pSenderSession->socket.sendBufferSize());
	// superior to buffer 0xFFFF to limit onFlush usage!
	return queueing > bufferSize ? queueing - bufferSize : 0;
}


void RTMFPSession::onParameters(const Parameters& parameters) {
	parameters.getNumber("keepalivePeer", _mainStream.keepalivePeer);
	parameters.getNumber("keepaliveServer", _mainStream.keepaliveServer);
	// set rendezvous address because now serverAddress is loaded!
	_pSession->pRendezVous->set<RTMFP::Session>(peer.id, peer.address, peer.serverAddress, _pSession.get());
}

void RTMFPSession::kill(Int32 error, const char* reason) {
	if (died)
		return;

	// if kill _flows is cleared so _pFlow becomes invalid!
	_pFlow = NULL;

	if (_mainStream.onStart) { // else already done
		
		if (_pSession) {
			// stop reception but not onFlush to get ACK and consume writers
			_pSession->onAddress = nullptr;
			_pSession->onMessage = nullptr;
			// no valid more for rendezvous
			_pSession->pRendezVous->erase(peer.id);
		}
		// unpublish and unsubscribe
		_mainStream.onStart = nullptr;
		_mainStream.onStop = nullptr;
		_mainStream.clearStreams();

		peer.onDisconnection(); // keep it before _writers deletion

		// close writers after disconnection but don't clear list to continue to try to relay last messages during killing steps!
		for (auto& it : _writers)
			it.second->close(error, reason);
		// delete flows after writer closing because flows close writers too, so can call peer:onClose
		_flows.clear();

		// start killing signal (after onDisconnection and writer close to allow to send last messages!)
		_killing = 10;
	}

	if ((error>0 || error==ERROR_CONGESTED) && _pSenderSession) {  // If error supporting a last message AND not come from client (0)
		// If CONGESTED in RTMFP => we have erase queue messages, signal the death anyway!
		if (!manage()) // to start immediatly killing signal
			return;
		flush();
		if (error != ERROR_SERVER)
			return; // Now can check that dies message is received excepting on server close
	}

	
	if(_pSession) {
		// stop completly the reception
		_pSession->onFlush = nullptr;
		_pSession.reset();
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
	send(shared<RTMFPCmdSender>(SET, 0x01));
	return true;
}

bool RTMFPSession::manage() {

	if (!Session::manage())
		return false;

	if (_killing) {
		// killing signal!
		// no other message, just fail message, so I erase all data in first
		send(shared<RTMFPCmdSender>(SET, 0x0C));
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
		if (!_pSenderSession) {
			kill(ERROR_CONGESTED); // timeout connection without response possible, client congested? client canceled?
			return false;
		}
		if (!peer) {
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
	auto it(_writers.begin());
	Exception ex;
	while (it != _writers.end()) {
		shared<RTMFPWriter>& pWriter(it->second);
		pWriter->flush(); // do flush even on closed to try to pass in a consumed state
		if (pWriter->consumed()) {
			// Keep consumption and deletion here and not in _onFlush on acquit to keep alive a little more the writer to avoid warning "writer unknown" a while
			DEBUG("Delete writer ", it->first, " on session ", name());
			it = _writers.erase(it);
		} else
			++it;
	}
}

void RTMFPSession::send(shared<RTMFPSender>&& pSender) {
	// continue even on _killing to repeat writers messages to flush it (reliable)
	pSender->address = peer.address;
	pSender->pSession = _pSenderSession;
	api.threadPool.queue(_senderTrack, move(pSender));
}

shared<RTMFPWriter> RTMFPSession::newWriter(UInt64 flowId, const Packet& signature) {
	// emplace with a new id
	map<UInt64, shared<RTMFPWriter>>::iterator it;
	do {
		while (++_nextWriterId == 0);
		it = _writers.lower_bound(_nextWriterId);
	} while (it != _writers.end() && it->first == _nextWriterId);
	DEBUG("New writer ", _nextWriterId, " on session ", name());
	return _writers.emplace_hint(it, SET, forward_as_tuple(_nextWriterId), forward_as_tuple(SET, _nextWriterId, flowId, signature, self))->second;
}

UInt64 RTMFPSession::resetWriter(UInt64 id) {
	// remove previous version
	const auto& it(_writers.find(id));
	while (++_nextWriterId == 0 || !_writers.emplace(_nextWriterId, it->second).second);
	_writers.erase(it);
	return _nextWriterId;
}


} // namespace Mona
