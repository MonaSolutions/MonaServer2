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

#include "Mona/RTMFP/RTMFPDecoder.h"
#include "Mona/DiffieHellman.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {


struct RTMFPDecoder::Handshake : virtual Object {
	OnHandshake		onHandshake;
	OnEdgeMember	onEdgeMember;

	Handshake(const Handler& handler, const shared<RendezVous>& pRendezVous) : _recvTime(Time::Now()), track(0), _pResponse(SET), _pRendezVous(pRendezVous), _handler(handler) {}

	Packet					tag;
	UInt16					track;
	shared<RTMFPReceiver>	pReceiver;

	bool obsolete(const Time& now = Time::Now()) const { return (now - _recvTime) > 95000; } // 95 seconds, must be less that RTMFPSession timeout (120 seconds), 95 = RTMFP spec.

	void receive(Socket& socket, shared<Buffer>& pBuffer, const SocketAddress& address) {
		Exception ex;
		bool success;
		AUTO_ERROR(success = RTMFP::Engine::Decode(ex, *pBuffer, address), "RTMFP handshake decoding");
		if (!success)
			return;
		BinaryReader reader(pBuffer->data(), pBuffer->size());
		UInt8 marker = reader.read8();
		if (marker != 0x0b) {
			ERROR("Marker handshake ", String::Format<UInt8>("%.2x", marker), " wrong, should be 0b");
			return;
		}
		_recvTime.update();
		reader.read16(); // time
		UInt8 type = reader.read8();
		reader.shrink(reader.read16());
		switch (type) {
			case 0x10: {
				// New member group, normally used between RTMFP servers
				// format => peerId + memberId + groupId + redirection addresses
				if (reader.available()<(Entity::SIZE*3)) {
					ERROR("New member group message without valid peer/member/group id");
					return;
				}
				Packet member(pBuffer, reader.current(), reader.available());
				reader.next(Entity::SIZE*3); // peerId + groupId
				map<SocketAddress, bool> redirections({ {address , false} });
				while (reader.available()) {
					SocketAddress address;
					RTMFP::Location location = RTMFP::ReadAddress(reader, address);
					if (!location) {
						WARN("New member group message with invalid redirection address");
						break;
					}
					redirections[address] = location != RTMFP::LOCATION_LOCAL;
				}
				_handler.queue(onEdgeMember, member, redirections);
				break;
			}
			case 0x30: {
				reader.read7Bit<UInt64>(10); // useless
				reader.read7Bit<UInt64>(10); // size epd (useless)
				UInt8 type(reader.read8());
				if (type == 0x0f) {
					if (reader.available()<48) {
						ERROR("Handshake 0x30-0F rendezvous without peer id and 16 bytes tag field");
						return;
					}

					DEBUG("Rendezvous request from ", address);

					shared<Buffer> pBuffer;
					
					const UInt8* peerId(reader.current());
					reader.next(Entity::SIZE);
					Packet tag(reader.current(), 16); // not save this tag! (rendezvous service, not session handshaking!)

					SocketAddress bAddress;
					map<SocketAddress, bool> aAddresses, bAddresses;
					RTMFP::Session* pSession = _pRendezVous->meet<RTMFP::Session>(address, peerId, aAddresses, bAddress, bAddresses);
					if (!pSession) {
						if (!aAddresses.empty() || this->tag == tag) {
							// Redirection!
							RTMFP::InitBuffer(pBuffer, 0x0B);
							BinaryWriter writer(*pBuffer);
							writer.write8(0x71).next(2).write8(16).write(tag); // tag in header
							for (auto& it : aAddresses) {
								DEBUG(address, it.second ? " redirected to public " : " redirected to local ", it.first);
								RTMFP::WriteAddress(writer, it.first, RTMFP::LOCATION_REDIRECTION);
							}
							if (aAddresses.empty()) {
								RTMFP::WriteAddress(writer, SocketAddress::Wildcard(), RTMFP::LOCATION_UNSPECIFIED);
								DEBUG(address, " get the wildcard address to inform peer death");
							}
							BinaryWriter(pBuffer->data() + 10, 2).write16(pBuffer->size() - 12);  // write size header
							RTMFP::Send(socket, Packet(RTMFP::Engine::Encode(pBuffer, 0, address)), address);
							return;
						}
						this->tag = move(tag); // save tag to log "unfound" repeating request and send Wildcard() if repeating (libRTMFP interprets it as: peer dies)
						DEBUG("UDP Hole punching, session ", String::Hex(peerId, Entity::SIZE), " wanted not found")
						return; // peerId unknown!
					}

					{ // B get A in first (A is waiting B)
						RTMFP::InitBuffer(pBuffer);
						BinaryWriter writer(*pBuffer);
						for (auto& it : aAddresses) {
							if (writer.size() < 1100 || it.second) { // could exceeds RTMFP::SIZE_PACKET, so write just public address if UDP packet is exceeding 1100 bytes (never more than 2 publics addresses in aAddresses)
								writer.write8(0x0F).write16(it.first.host().size() + Entity::SIZE + 22).write24(0x22210F).write(peerId, Entity::SIZE);
								DEBUG(bAddress, it.second ? " get public " : " get local ", it.first);
								RTMFP::WriteAddress(writer, it.first, it.second ? RTMFP::LOCATION_PUBLIC : RTMFP::LOCATION_LOCAL);
								writer.write(tag); // tag in footer
							}
						}
						// create a new encoder to be thread safe =>
						RTMFP::Send(socket, Packet(RTMFP::Engine(*pSession->pEncoder).encode(pBuffer, pSession->farId, bAddress)), bAddress);
					}
					{  // A get B
						RTMFP::InitBuffer(pBuffer, 0x0B);
						BinaryWriter writer(*pBuffer);
						writer.write8(0x71).next(2).write8(16).write(tag); // tag in header
						for (auto& it : bAddresses) {
							DEBUG(address, it.second ? " get public " : " get local ", it.first);
							RTMFP::WriteAddress(writer, it.first, it.second ? RTMFP::LOCATION_PUBLIC : RTMFP::LOCATION_LOCAL);
						}
						BinaryWriter(pBuffer->data() + 10, 2).write16(pBuffer->size() - 12);  // write size header
						RTMFP::Send(socket, Packet(RTMFP::Engine::Encode(pBuffer, 0, address)), address);
					}
					return;
				}
				if (type == 0x0a) {
					if (reader.available()<16) {
						ERROR("Handshake 0x30 without 16 bytes tag field");
						return;
					}
					if (!_pResponse.unique()) { 
						DEBUG("Handshake ignored because is already answering");
						return; // is already answering!
					}
					Packet tag(reader.current(), 16);
					if (this->tag) {
						if (pReceiver) {
							// from 0x38 to 0x30 again, address changed? => erase pReceiver, and restart handshake again
							WARN("38 handshake back to 30 handshake again, obsolete session handshake abandonned by client");
							pReceiver.reset();
							_pResponse.set();
						} else if (this->tag ==tag) {
							// repeat response
							DEBUG("Repeat 30 handshake response");
							RTMFP::Send(socket, *_pResponse, address); 
							return;
						}
						// new 0x30 request, maybe url/path have changed!
					}
					this->tag = move(tag);
					_handler.queue(onHandshake, Packet(pBuffer, reader.current(), reader.available()), address, _pResponse);
				} else
					ERROR("Handshake 0x30 with unknown ", String::Format<UInt8>("%02x", type), " type");
				return;
			}
			case 0x38: {
				if (!_pResponse.unique()) {
					ERROR("38 handshake before end of 30 handshake, request ignored")
					return;
				}
				if (pReceiver) {
					// repeat response!
					DEBUG("Repeat 38 handshake response");
					RTMFP::Send(socket, *_pResponse, address);
					return;
				}
				if (!tag)
					WARN("38 hanshake without 30 before or client has changed address between the both");
				UInt32 farId = reader.read32();
				if (reader.read7Bit<UInt64>(10) != RTMFP::SIZE_COOKIE) {
					ERROR("Bad handshake cookie ", String::Hex(reader.current(), 16), "..., its size should be 64 bytes");
					return;
				}

				UInt32 id = Byte::From32Network(*(UInt32*)reader.current());
				reader.next(RTMFP::SIZE_COOKIE);

				UInt8 farPubKeySize = range<UInt8>(reader.read7Bit<UInt64>(10));
				const UInt8* farPubKey = reader.current();
		
				UInt16 size = range<UInt16>(reader.read7Bit<UInt64>(10)) - 2;
				reader.next(2); // unknown
				const UInt8* key = reader.current(); reader.next(size);

				DiffieHellman dh;
				UInt8 secret[DiffieHellman::SIZE];
				UInt8 secretSize;
				AUTO_ERROR(secretSize = dh.computeSecret(ex = nullptr, key, size, secret), "DiffieHellman");
				if (!secretSize)
					return;
			
				shared<Buffer> pOut;
				RTMFP::InitBuffer(pOut, 0x0B);
				BinaryWriter writer(*pOut);
				writer.write8(0x78).next(2).write32(id).write7Bit<UInt64>(dh.publicKeySize() + 11);
				UInt32 noncePos = writer.size();
				writer.write(EXPAND("\x03\x1A\x00\x00\x02\x1E\x00"));
				UInt8 byte2 = DiffieHellman::SIZE - dh.publicKeySize();
				if (byte2>2) {
					CRITIC("Generation DH key with less of 126 bytes!");
					byte2 = 2;
				}
				writer.write8(0x81).write8(2 - byte2).write16(0x0D02);
				dh.readPublicKey(writer.buffer(dh.publicKeySize()));
				writer.write8(0x58);

				// Compute Keys
				UInt8 encryptKey[Crypto::SHA256_SIZE];
				UInt8 decryptKey[Crypto::SHA256_SIZE];
				size = range<UInt16>(reader.read7Bit<UInt64>(10));
				RTMFP::ComputeAsymetricKeys(secret, secretSize, reader.current(), size, writer.data() + noncePos, dh.publicKeySize() + 11, decryptKey, encryptKey);
				//TRACE(String::Hex(secret, DiffieHellman::SIZE));

				// write size!
				BinaryWriter(pOut->data() + 10, 2).write16(pOut->size() - 12);
				// encode and save response
				_pResponse->set(RTMFP::Engine::Encode(pOut, farId, address));
				// Create receiver just before send
				pReceiver.set(_handler, id, farId, farPubKey, farPubKeySize, decryptKey, encryptKey, address, _pRendezVous);
				// send
				RTMFP::Send(socket, *_pResponse, address);
				return;
			}
			default:
				ERROR("Unkown handshake ", String::Format<UInt8>("%02x", type), " type");
		}
	}

	const Handler&			_handler;
	Time					_recvTime;
	shared<RendezVous>		_pRendezVous;
	shared<Packet>			_pResponse;
};

RTMFPDecoder::RTMFPDecoder(const shared<RendezVous>& pRendezVous, const Handler& handler, const ThreadPool& threadPool) :
	_pRendezVous(pRendezVous), _handler(handler), _threadPool(threadPool), _pReceiving(SET) {
}

bool RTMFPDecoder::finalizeHandshake(UInt32 id, const SocketAddress& address, shared<RTMFPReceiver>& pReceiver) {
	auto itHand = _handshakes.lower_bound(address);
	if (itHand == _handshakes.end() || itHand->first != address || !itHand->second->pReceiver) {
		// no "receiver"("session") handshake, address has changed?
		DEBUG("Handshake iteration");
		itHand = _handshakes.begin();
		while (itHand != _handshakes.end()) {
			// test unicity of pReceiver to test that EXISTS + NOT HANDSHAKING!
			if (itHand->second->pReceiver.unique() && id == itHand->second->pReceiver->id)
				break;
			++itHand;
		}
		if (itHand == _handshakes.end())
			return false;
	}
	// finalize!
	pReceiver = itHand->second->pReceiver;
	if (!pReceiver)
		return false; // can happen on UDP hole punching handshake!
	_handler.queue(onSession, pReceiver);
	if(!itHand->second->tag) // address has changed between 30 and 38, raise the event because will not be detected by pReceiver (built on 38)
		_handler.queue(pReceiver->onAddress, address);
	_handshakes.erase(itHand);
	return true;
}

void RTMFPDecoder::decode(shared<Buffer>& pData, const SocketAddress& address, const shared<Socket>& pSocket) {
	shared<Buffer> pBuffer(move(pData)); // capture!

	if (pBuffer->size() <= RTMFP::SIZE_HEADER || (pBuffer->size() > (RTMFP::SIZE_PACKET<<1))) {
		if (address.host().isLoopback() && address.port() == pSocket->address().port()) {
			// itself messages! events mechanism!
			if(!pBuffer->size()) {
				// clean obsolete handshakes and receivers, sends by RTMFProtocol::manage!
				Time now; // get time just once for performance
				/// remove obsolete handshakes
				auto it = _handshakes.begin();
				while (it != _handshakes.end()) {
					if (it->second.unique() && it->second->obsolete(now))
						it = _handshakes.erase(it);
					else
						++it;
				}
				/// remove obsolete receivers
				auto itRecv = _receivers.begin();
				while (itRecv != _receivers.end()) {
					if (itRecv->second.unique() && itRecv->second->obsolete(now))
						itRecv = _receivers.erase(itRecv);
					else
						++itRecv;
				}
				return;
			}
		}
		ERROR("Invalid ", pBuffer->size(), " packet size");
		return;
	}
	if (*_pReceiving >= pSocket->recvBufferSize()) {
		DEBUG("Packet ignored because reception is receiving already socket recvBufferSize")
		return; // ignore packet! wait end of receiving process (saturation)
	}

	UInt32 id = RTMFP::ReadID(*pBuffer);
	//DEBUG("RTMFP Session ",id," size ",pBuffer->size());
	if (!id) {
		// HANDSHAKE
		Exception ex;
		auto it = _handshakes.lower_bound(address);
		if (it != _handshakes.end() && it->second.unique() && it->second->obsolete())
			it = _handshakes.erase(it);
		if (it == _handshakes.end() || it->first != address) {
			// Create handshake
			it = _handshakes.emplace_hint(it, SET, forward_as_tuple(address), forward_as_tuple(SET, _handler, _pRendezVous));
			it->second->onHandshake = onHandshake;
			it->second->onEdgeMember = onEdgeMember;
		}
		receive(it->second, pBuffer, address, pSocket);
	} else {
		auto it = _receivers.lower_bound(id);
		if (it == _receivers.end() || it->first != id) {
			shared<RTMFPReceiver> pReceiver;
			if (!finalizeHandshake(id, address, pReceiver)) {
				WARN("Unknown RTMFP session ", id);
				return;
			}
			it = _receivers.emplace_hint(it, id, pReceiver);
		} else if (it->second.unique()) {
			// RTMFPSession dead!
			// Search in handshake (address has changed?)
			shared<RTMFPReceiver> pReceiver;
			if (finalizeHandshake(id, address, pReceiver)) // else is an obsolete session! will resend 0x4C message
				it->second = pReceiver; // replace by new session!
			else if (it->second->obsolete()) {
				_receivers.erase(it);
				return;
			}
		}
		receive(it->second, pBuffer, address, pSocket);
	}
}

} // namespace Mona
