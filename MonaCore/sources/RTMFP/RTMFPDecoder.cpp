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
	OnHandshake	onHandshake;

	Handshake(const Handler& handler, const shared<RendezVous>& pRendezVous) : track(0), _pResponse(new Packet()), _pRendezVous(pRendezVous), _handler(handler) {}

	UInt16					track;
	shared<RTMFPReceiver>	pReceiver;

	bool obsolete() const { return _timeout.isElapsed(95000); } // 95 seconds, must be less that RTMFPSession timeout (120 seconds), 95 = RTMFP spec.

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
		reader.read16(); // time
		UInt8 type = reader.read8();
		reader.shrink(reader.read16());
		switch (type) {
			case 0x30: {
				reader.read7BitLongValue(); // useless
				reader.read7BitLongValue(); // size epd (useless)
				UInt8 type(reader.read8());
				if (type == 0x0f) {
					if (reader.available()<48) {
						ERROR("Handshake 0x30-0F rendezvous without peer id and 16 bytes tag field");
						return;
					}
					
					const UInt8* peerId(reader.current());
					reader.next(Entity::SIZE);
					Packet tag(reader.current(), 16);

					SocketAddress bAddress;
					map<SocketAddress, bool> aAddresses, bAddresses;
					RTMFP::Session* pSession = _pRendezVous->meet<RTMFP::Session>(address, peerId, aAddresses, bAddress, bAddresses);
					if (!pSession) {
						DEBUG("UDP Hole punching, session ", Util::FormatHex(peerId, Entity::SIZE, string()), " wanted not found")
						return; // peerId unknown! TODO?
					}

					shared<Buffer> pBuffer;
					{
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
					{
						RTMFP::InitBuffer(pBuffer);
						BinaryWriter writer(*pBuffer);
						for (auto& it :aAddresses) {
							if (!it.second)
								continue; // useless to considerate local address on this side (could exceeds packet size, and useless on the paper too, see UDP hole punching process)
							writer.write8(0x0F).write16(it.first.host().size()+Entity::SIZE+22).write24(0x22210F).write(peerId, Entity::SIZE);
							DEBUG(bAddress, it.second ? " get public " : " get local ", it.first);
							RTMFP::WriteAddress(writer, it.first, it.second ? RTMFP::LOCATION_PUBLIC : RTMFP::LOCATION_LOCAL);
							writer.write(tag); // tag in footer
						}
						// create a new encoder to be thread safe =>
						RTMFP::Send(socket, Packet(RTMFP::Engine(*pSession->pEncoder).encode(pBuffer, pSession->farId, bAddress)), bAddress);
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
					if (_tag) {
						if (pReceiver) {
							// from 0x38 to 0x30 again => erase pReceiver, and restart handshake again
							DEBUG("38 handshake back to 30 handshake again");
							pReceiver.reset();
							_pResponse.reset(new Packet());
						} else if (_tag==tag) {
							// repeat response
							RTMFP::Send(socket, *_pResponse, address); 
							return;
						}
						// new 0x30 request, maybe url/path have changed!
					}
					_tag = move(tag);
					_handler.queue(onHandshake, Packet(pBuffer, reader.current(), reader.available()), address, _pResponse);
				} else
					ERROR("Handshake 0x30 with unknown ", String::Format<UInt8>("%02x", type), " type");
				return;
			}
			case 0x38: {
				if (!_tag) {
					ERROR("38 hanshake without 30 before, request ignored");
					return;
				}
				if (!_pResponse.unique()) {
					WARN("38 handshake before end of 30 handshake")
					return;
				}
				if (pReceiver) {
					// repeat response!
					RTMFP::Send(socket, *_pResponse, address);
					return;
				}
				UInt32 farId = reader.read32();
				if (reader.read7BitLongValue() != RTMFP::SIZE_COOKIE) {
					ERROR("Bad handshake cookie ", Util::FormatHex(reader.current(), 16, string()), "..., its size should be 64 bytes");
					return;
				}

				UInt32 id = Byte::From32Network(*(UInt32*)reader.current());
				reader.next(RTMFP::SIZE_COOKIE);

				UInt8 farPubKeySize = UInt8(reader.read7BitLongValue());
				const UInt8* farPubKey = reader.current();
		
				UInt16 size = reader.read7BitValue() - 2;
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
				writer.write8(0x78).next(2).write32(id).write7BitLongValue(dh.publicKeySize() + 11);
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
				size = UInt16(reader.read7BitValue());
				RTMFP::ComputeAsymetricKeys(secret, secretSize, reader.current(), size, writer.data() + noncePos, dh.publicKeySize() + 11, decryptKey, encryptKey);
				pReceiver.reset(new RTMFPReceiver(_handler, id, farId, farPubKey, farPubKeySize, decryptKey, encryptKey, address, _pRendezVous));

				// write size!
				BinaryWriter(pOut->data() + 10, 2).write16(pOut->size() - 12);
				// encode and save response
				_pResponse->set(RTMFP::Engine::Encode(pOut, farId, address));
				// encode save and send
				RTMFP::Send(socket, *_pResponse, address);
				return;
			}
			default:
				ERROR("Unkown handshake ", String::Format<UInt8>("%02x", type), " type");
		}
	}

	const Handler&			_handler;
	Time					_timeout;
	shared<RendezVous>		_pRendezVous;
	shared<Packet>			_pResponse;
	Packet					_tag;
};

RTMFPDecoder::RTMFPDecoder(const Handler& handler, const ThreadPool& threadPool) : _handler(handler), _threadPool(threadPool), _pRendezVous(new RendezVous()), _pReceiving(new atomic<UInt32>(0)),
	_validateReceiver([this](UInt32 keySearched, map<UInt32, shared<RTMFPReceiver>>::iterator& it) {
		return keySearched != it->first && it->second.unique() && it->second->obsolete() ? false : true;
	}),
	_validateHandshake([this](const SocketAddress& keySearched, map<SocketAddress, shared<Handshake>>::iterator& it) {
		return it->second->obsolete() ? false : true;
	}) {
}

bool RTMFPDecoder::finalizeHandshake(UInt32 id, const SocketAddress& address, shared<RTMFPReceiver>& pReceiver) {
	auto itHand = lower_bound(_handshakes, address, _validateHandshake);
	if (itHand == _handshakes.end() || itHand->first != address || !itHand->second->pReceiver) {
		// no "receiver"("session") handshake, address has changed?
		itHand = _handshakes.begin();
		while (itHand != _handshakes.end()) {
			if (itHand->second->pReceiver) {
				if (itHand->second->pReceiver.unique()) {
					// this client (identified by its address) has started many hanshake without terminate it,
					// erase it because has already a pReceiver unique (is the reason of this call!
					itHand = _handshakes.erase(itHand);
					continue;
				}
				if (id == itHand->second->pReceiver->id)
					break;
			}
			++itHand;
		}
		if (itHand == _handshakes.end())
			return false;
	}
	// finalize!
	pReceiver = itHand->second->pReceiver;
	if (!pReceiver)
		return false; // can happen on UDP hole punching handshake!
	_handshakes.erase(itHand);
	_handler.queue(onSession, pReceiver);
	return true;
}

UInt32 RTMFPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (pBuffer->size() <= RTMFP::SIZE_HEADER || (pBuffer->size() > (RTMFP::SIZE_PACKET<<1))) {
		pBuffer.reset();
		ERROR("Invalid RTMFP packet");
		return 0;
	}
	if (*_pReceiving >= pSocket->recvBufferSize()) {
		DEBUG("Packet ignored because reception is receiving already socket recvBufferSize")
		return 0; // ignore packet! wait end of receiving process (saturation)
	}

	UInt32 id = RTMFP::ReadID(*pBuffer);
	//DEBUG("RTMFP Session ",id," size ",pBuffer->size());
	if (!id) {
		// HANDSHAKE
		Exception ex;
		auto it = lower_bound(_handshakes, address, _validateHandshake);
		if (it == _handshakes.end() || it->first != address) {
			// Create handshake
			it = _handshakes.emplace_hint(it, address, new Handshake(_handler, _pRendezVous));
			it->second->onHandshake = onHandshake;
		}
		receive(it->second, pBuffer, address, pSocket);
	} else {
		auto it = lower_bound(_receivers, id, _validateReceiver);
		if (it == _receivers.end() || it->first != id) {
			shared<RTMFPReceiver> pReceiver;
			if (!finalizeHandshake(id, address, pReceiver)) {
				WARN("Unknown RTMFP session ", id);
				pBuffer.reset();
				return 0;
			}
			it = _receivers.emplace_hint(it, id, pReceiver);
		} else if (it->second.unique()) {
			// RTMFPSession dead!
			// Search in handshake?
			shared<RTMFPReceiver> pReceiver;
			if (finalizeHandshake(id, address, pReceiver)) // else is an obsolete session! will resend 0x4C message
				it->second = pReceiver; // replace by new session!
		}
		receive(it->second, pBuffer, address, pSocket);
	}
	return 0;
}

} // namespace Mona
