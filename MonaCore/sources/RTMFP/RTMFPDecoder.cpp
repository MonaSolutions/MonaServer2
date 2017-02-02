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

	Handshake(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket, const Handler& handler) : _attempts(0), Runner("RTMFPHandshake"), _handler(handler), _weakSocket(pSocket), pBuffer(move(pBuffer)), address(address) {}

	shared<Buffer>			pBuffer;
	SocketAddress			address;
	shared<Packet>			pResponse;
	shared<RTMFPReceiver>	pReceiver;

	bool obsolete() const { return _timeout.isElapsed(95000); } // 95 seconds, must be less that RTMFPSession timeout (120 seconds), 95 = RTMFP spec.

private:
	bool run(Exception&) {
		shared<Socket> pSocket(_weakSocket.lock());
		if (!pSocket) {
			pBuffer.reset();
			return true; // RTMFP stopped
		}

		Exception ex;
		bool success;
		AUTO_ERROR(success = RTMFP::Engine::Decode(ex, *pBuffer, address), "RTMFP handshake decoding");
		if (!success)
			return true;
		BinaryReader reader(pBuffer->data(), pBuffer->size());
		UInt8 marker = reader.read8();
		if (marker != 0x0b) {
			ERROR("Marker handshake ", String::Format<UInt8>("%.2x", marker), " wrong, should be 0b and not ");
			return true;
		}
		reader.read16(); // time
		UInt8 type = reader.read8();
		reader.shrink(reader.read16());
		switch (type) {
			case 0x30: {
				if (pResponse) {
					// repeat response!
					RTMFP::Send(*pSocket, *pResponse, address);
					break;
				}
				reader.read7BitLongValue(); // useless
				reader.read7BitLongValue(); // size epd (useless)
				UInt8 type(reader.read8());
				if (reader.available()<16) {
					ERROR("Handshake 0x30 without 16 bytes tag field")
						break;
				}
				if (type == 0x0f) {
					if (reader.available()<48) {
						ERROR("Handshake 0x30 rendezvous without peer id");
						break;
					}
					/*	
					const UInt8* peerId(reader.current());

					RTMFPSession* pSessionWanted = _sessions.findByPeer<RTMFPSession>(peerId);
	
					if(pSessionWanted) {
						if(pSessionWanted->killing())
							return 0x00; // TODO no way in RTMFP to tell "died!"
						/// Udp hole punching
						UInt32 times = attempt(tag);
		
						RTMFPSession* pSession(NULL);
						if(times > 0 || address.host() == pSessionWanted->peer.address.host()) // try in first just with public address (excepting if the both peer are on the same machine)
							pSession = _sessions.findByAddress<RTMFPSession>(address,Socket::DATAGRAM);
					
						// send hanshaker addresses to session wanted
						pSessionWanted->p2pHandshake(tag,address,times,pSession);
					
						// public address
						RTMFP::WriteAddress(response,pSessionWanted->peer.address, RTMFP::ADDRESS_PUBLIC);
						DEBUG("P2P address initiator exchange, ",pSessionWanted->peer.address.toString());

						if (pSession && pSession->peer.serverAddress.host() != pSessionWanted->peer.serverAddress.host() && pSession->peer.serverAddress.host()!=pSessionWanted->peer.address.host()) {
							// the both peer see the server in a different way (and serverAddress.host()!= public address host written above),
							// Means an exterior peer, but we can't know which one is the exterior peer
							// so add an interiorAddress build with how see eachone the server on the both side
						
							// Usefull in the case where Caller is an exterior peer, and SessionWanted an interior peer,
							// we give as host to Caller how it sees the server (= SessionWanted host), and port of SessionWanted
							SocketAddress interiorAddress(pSession->peer.serverAddress.host(), pSessionWanted->peer.address.port());
							RTMFP::WriteAddress(response,interiorAddress, RTMFP::ADDRESS_PUBLIC);
							DEBUG("P2P address initiator exchange, ",interiorAddress.toString());
						}

						// local address
						for(const SocketAddress& address : pSessionWanted->peer.localAddresses) {
							RTMFP::WriteAddress(response,address, RTMFP::ADDRESS_LOCAL);
							DEBUG("P2P address initiator exchange, ",address.toString());
						}

						// add the turn address (RelayServer) if possible and required
						if (pSession && times>0) {
							UInt8 timesBeforeTurn(0);
							if(pSession->peer.parameters().getNumber("timesBeforeTurn",timesBeforeTurn) && timesBeforeTurn>=times) {
								UInt16 port = invoker.relayer.relay(pSession->peer.address,pSessionWanted->peer.address,20); // 20 sec de timeout is enough for RTMFP!
								if (port > 0) {
									SocketAddress address(pSession->peer.serverAddress.host(), port);
									RTMFP::WriteAddress(response, address, RTMFP::ADDRESS_REDIRECTION);
								} // else ERROR already display by RelayServer class
							}
						}
						return 0x71;
					}


					DEBUG("UDP Hole punching, session ", Util::FormatHex(peerId, ID_SIZE, LOG_BUFFER), " wanted not found")
					set<SocketAddress> addresses;
					peer.onRendezVousUnknown(peerId,addresses);
					set<SocketAddress>::const_iterator it;
					for(it=addresses.begin();it!=addresses.end();++it) {
						if(it->host().isWildcard())
							continue;
						if(address == *it)
							WARN("A client tries to connect to himself (same ", address.toString()," address)");
						RTMFP::WriteAddress(response,*it,RTMFP::ADDRESS_REDIRECTION);
						DEBUG("P2P address initiator exchange, ",it->toString());
					}
					return addresses.empty() ? 0 : 0x71;
					
					*/
				} else if (type == 0x0a) {
					shared<RTMFP::Handshake> pHandshake(new RTMFP::Handshake(_attempts++, Packet(pBuffer, reader.current(), reader.available()), address));
					pResponse = pHandshake;
					_handler.queue(onHandshake, pHandshake);
				} else
					ERROR("Handshake 0x30 with unknown ", String::Format<UInt8>("%02x", type), " type");
				break;
			}
			case 0x38: {
				if (pReceiver && pResponse) {
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

				Packet farPubKey(pBuffer, reader.current(), UInt32(reader.read7BitLongValue()));
		
				UInt32 sizeKey = reader.read7BitValue() - 2;
				reader.next(2); // unknown
				const UInt8* key = reader.current(); reader.next(sizeKey);

				DiffieHellman dh;
				Buffer	secret(DiffieHellman::SIZE);
				AUTO_ERROR(success = dh.computeSecret(ex = nullptr, key, sizeKey, secret.data()) ? true : false, "DiffieHellman");
				if (!success)
					break;

				RTMFP::InitBuffer(pBuffer, 0x0B);
				BinaryWriter writer(*pBuffer);
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
				BinaryWriter(pBuffer->data() + 10, 2).write16(pBuffer->size() - 12);

				// Compute Keys
				UInt8 encryptKey[Crypto::SHA256_SIZE];
				UInt8 decryptKey[Crypto::SHA256_SIZE];
				sizeKey = reader.read7BitValue();
				RTMFP::ComputeAsymetricKeys(secret, reader.current(), sizeKey, writer.data() + noncePos, dh.publicKeySize() + 11, decryptKey, encryptKey);
		
				pReceiver.reset(new RTMFPReceiver(_handler, id, farId, farPubKey, decryptKey, encryptKey, pSocket, address));

				pResponse.reset(new Packet(RTMFP::Engine::Encode(pBuffer, farId, address)));
				RTMFP::Send(*pSocket,*pResponse, address);
				break;
			}
			default:
				ERROR("Unkown handshake ", String::Format<UInt8>("%02x", type), " type");
		}
		return true;
	}

	weak<Socket>	_weakSocket;
	const Handler&	_handler;
	UInt8			_attempts;
	Time			_timeout;
};

RTMFPDecoder::RTMFPDecoder(const Handler& handler, const ThreadPool& threadPool) : _handler(handler), _threadPool(threadPool),
	_validateReceiver([this](map<UInt32, shared<RTMFPReceiver>>::iterator& it) {
		return it->second.unique() ? false : true;
	}),
	_validateHandshake([this](map<SocketAddress, shared<Handshake>>::iterator& it) {
		return it->second->obsolete() ? false : true;
	}) {
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
			it = _handshakes.emplace_hint(it, address, new Handshake(pBuffer, address, pSocket, _handler));
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
		// first packet after handshake?
		auto itHand = lower_bound(_handshakes, address, _validateHandshake);
		if (itHand == _handshakes.end() || itHand->first != address) {
			// has changed of address?
			for (itHand = _handshakes.begin(); itHand != _handshakes.end(); ++itHand) {
				if (!itHand->second->pReceiver)
					continue;
				if (id == itHand->second->pReceiver->id)
					break;
			}
			if (itHand == _handshakes.end()) {
				pBuffer.reset();
				WARN("Unknown RTMFP session ", id);
				return 0;
			}
		}
		if (!itHand->second->pReceiver) {
			ERROR("Handshake invalid");
			pBuffer.reset();
			return 0;
		}
		it = _receivers.emplace_hint(it, id, itHand->second->pReceiver);
		_handshakes.erase(itHand);
		_handler.queue(onSession, it->second);
	}
	/* TODO? on kill session keep it reseted a time to catch obsolete session message
	if (!it->second) {
		pBuffer.reset();
		return 0;
	}*/


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
