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

#define KILL(error) {_died=true; echoTime=error; size = reader.available();}

struct RTMFPDecoder::Handshake : Runner, virtual Object {
	OnHandshake	onHandshake;

	Handshake(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket, const Handler& handler, const shared<RendezVous>& pRendezVous) : _pRendezVous(pRendezVous), pResponse(new Packet()), _attempts(0), Runner("RTMFPHandshake"), _handler(handler), _weakSocket(pSocket), pBuffer(move(pBuffer)), address(address) {}

	shared<Buffer>			pBuffer;
	SocketAddress			address;
	shared<Packet>			pResponse;
	shared<RTMFPReceiver>	pReceiver;

	bool obsolete() const { return _timeout.isElapsed(95000); } // 95 seconds, must be less that RTMFPSession timeout (120 seconds), 95 = RTMFP spec.

private:
	bool run(Exception&) {
		shared<Buffer> pBuffer(move(this->pBuffer));
		shared<Socket> pSocket(_weakSocket.lock());
		if (!pSocket)
			return true; // RTMFP stopped

		Exception ex;
		bool success;
		AUTO_ERROR(success = RTMFP::Engine::Decode(ex, *pBuffer, address), "RTMFP handshake decoding");
		if (!success)
			return true;
		BinaryReader reader(pBuffer->data(), pBuffer->size());
		UInt8 marker = reader.read8();
		if (marker != 0x0b) {
			ERROR("Marker handshake ", String::Format<UInt8>("%.2x", marker), " wrong, should be 0b");
			return true;
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
						break;
					}
					
					const UInt8* peerId(reader.current());
					reader.next(Entity::SIZE);

					shared<Buffer> pBufferA, pBufferB;
					RTMFP::InitBuffer(pBufferA, 0x0B);
					RTMFP::InitBuffer(pBufferB);
					BinaryWriter writerA(*pBufferA);
					BinaryWriter writerB(*pBufferB);
					writerA.write8(0x71).next(2).write8(16).write(reader.current(), 16); // tag in header
					writerB.write8(0x0F).next(2).write24(0x22210F).write(peerId,Entity::SIZE);
					SocketAddress addressB;
					RTMFP::Session* pSession = _pRendezVous->meet<RTMFP::Session>(address, peerId, writerA, writerB, addressB, _attempts++);
					if (!pSession) {
						DEBUG("UDP Hole punching, session ", Util::FormatHex(peerId, Entity::SIZE, string()), " wanted not found")
						break; // peerId unknown! TODO?
					}
					writerB.write(reader.current(), 16); // tag in footer
					
					// Write header size
					BinaryWriter(pBufferA->data() + 10, 2).write16(pBufferA->size() - 12);
					BinaryWriter(pBufferB->data() + 10, 2).write16(pBufferB->size() - 12);
					// encode response
					RTMFP::Engine::Encode(pBufferA, 0, address);
					RTMFP::Engine(*pSession->pEncoder).encode(pBufferB, pSession->farId, addressB); // create a new encoder to be thread safe
					// send
					RTMFP::Send(*pSocket, Packet(pBufferA), address);
					RTMFP::Send(*pSocket, Packet(pBufferB), addressB);
					break;
				}
				if (type == 0x0a) {
					if (*pResponse) {
						// repeat response!
						RTMFP::Send(*pSocket, *pResponse, address);
						break;
					}
					if (reader.available()<16) {
						ERROR("Handshake 0x30 without 16 bytes tag field");
						break;
					}
					_handler.queue(onHandshake, _attempts++, Packet(pBuffer, reader.current(), reader.available()), address, pResponse);
				} else
					ERROR("Handshake 0x30 with unknown ", String::Format<UInt8>("%02x", type), " type");
				break;
			}
			case 0x38: {
				if (!*pResponse) {
					ERROR("0B-38 hanshake without 0B-30 before, request ignored");
					break;
				}
				if (pReceiver) {
					// repeat response!
					RTMFP::Send(*pSocket, *pResponse, address);
					break;
				}
				UInt32 farId = reader.read32();

				if (reader.read7BitLongValue() != RTMFP::SIZE_COOKIE) {
					ERROR("Bad handshake cookie ", Util::FormatHex(reader.current(), 16, string()), "..., its size should be 64 bytes");
					break;
				}

				UInt32 id = Byte::From32Network(*(UInt32*)reader.current());
				reader.next(RTMFP::SIZE_COOKIE);

				const UInt8* farPubKey = reader.current();
				UInt8 farPubKeySize = UInt8(reader.read7BitLongValue());
		
				UInt16 size = reader.read7BitValue() - 2;
				reader.next(2); // unknown
				const UInt8* key = reader.current(); reader.next(size);

				DiffieHellman dh;
				UInt8 secret[DiffieHellman::SIZE];
				UInt8 secretSize;
				AUTO_ERROR(secretSize = dh.computeSecret(ex = nullptr, key, size, secret), "DiffieHellman");
				if (!secretSize)
					break;
			
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
				pReceiver.reset(new RTMFPReceiver(_handler, id, farId, farPubKey, farPubKeySize, decryptKey, encryptKey, pSocket, address, _pRendezVous));

				// write size!
				BinaryWriter(pOut->data() + 10, 2).write16(pOut->size() - 12);
				// encode and save response
				pResponse->set(RTMFP::Engine::Encode(pOut, farId, address));
				// send
				RTMFP::Send(*pSocket,*pResponse, address);
				break;
			}
			default:
				ERROR("Unkown handshake ", String::Format<UInt8>("%02x", type), " type");
		}
		return true;
	}

	weak<Socket>		_weakSocket;
	const Handler&		_handler;
	UInt8				_attempts;
	Time				_timeout;
	shared<RendezVous>  _pRendezVous;
};

RTMFPDecoder::RTMFPDecoder(const Handler& handler, const ThreadPool& threadPool) : _handler(handler), _threadPool(threadPool), _pRendezVous(new RendezVous()),
	_validateReceiver([this](UInt32 keySearched, map<UInt32, shared<RTMFPReceiver>>::iterator& it) {
		return keySearched != it->first && it->second.unique() && it->second->obsolete() ? false : true;
	}),
	_validateHandshake([this](const SocketAddress& keySearched, map<SocketAddress, shared<Handshake>>::iterator& it) {
		return it->second->obsolete() ? false : true;
	}) {
}

bool RTMFPDecoder::finalizeHandshake(UInt32 id, const SocketAddress& address, shared<RTMFPReceiver>& pReceiver) {
	auto itHand = lower_bound(_handshakes, address, _validateHandshake);
	if (itHand == _handshakes.end() || itHand->first != address) {
		// has changed of address?
		for (itHand = _handshakes.begin(); itHand != _handshakes.end(); ++itHand) {
			if (!itHand->second->pReceiver)
				continue;
			if (id == itHand->second->pReceiver->id)
				break;
		}
		if (itHand == _handshakes.end())
			return false;
	}
	// finalize!
	pReceiver = itHand->second->pReceiver;
	_handshakes.erase(itHand);
	if (!pReceiver) {
		ERROR("Handshake invalid");
		return false;
	}
	_handler.queue(onSession, pReceiver);
	return true;
}

UInt32 RTMFPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (pBuffer->size() <= RTMFP::SIZE_HEADER || (pBuffer->size() > (RTMFP::SIZE_PACKET<<1))) {
		pBuffer.reset();
		ERROR("Invalid RTMFP packet");
		return 0;
	}
	UInt32 id = RTMFP::ReadID(*pBuffer);
	//DEBUG("RTMFP Session ",id," size ",pBuffer->size());

	if (!id) {
		// HANDSHAKE
		Exception ex;
		auto it = lower_bound(_handshakes, address, _validateHandshake);
		if (it == _handshakes.end() || it->first != address) {
			// First handshake
			it = _handshakes.emplace_hint(it, address, new Handshake(pBuffer, address, pSocket, _handler, _pRendezVous));
			it->second->onHandshake = onHandshake;
			AUTO_ERROR(_threadPool.queue(ex, it->second), "RTMFP handshake");
		} else if(it->second.unique() && it->second->pResponse.unique()) {
			// Second handshake or resend first
			it->second->pBuffer = move(pBuffer);
			it->second->address = address;
			AUTO_ERROR(_threadPool.queue(ex, it->second), "RTMFP handshake");
		}  // else is already answering, wait!
		return 0;
	}

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

	struct Receive : Runner, virtual Object {
		Receive(shared<RTMFPReceiver>& pReceiver, shared<Buffer>& pBuffer, const SocketAddress& address) : Runner("RTMFPReceive"), _weakReceiver(pReceiver), _pBuffer(move(pBuffer)), _address(address) {}
	private:
		bool run(Exception&) {
			shared<RTMFPReceiver> pReceiver(_weakReceiver.lock());
			if (pReceiver)
				pReceiver->receive(_pBuffer, _address);
			return true;
		}

		shared<Buffer>		_pBuffer;
		weak<RTMFPReceiver> _weakReceiver;
		SocketAddress		_address;
	};

	Exception ex;
	AUTO_ERROR(_threadPool.queue(ex, make_shared<Receive>(it->second, pBuffer, address), it->second->track), "RTMFP session ",it->first);
	return 0;
}

} // namespace Mona
