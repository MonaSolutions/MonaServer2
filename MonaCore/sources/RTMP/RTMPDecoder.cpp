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

#include "Mona/RTMP/RTMPDecoder.h"
#include "Mona/DiffieHellman.h"
#include "Mona/BinaryWriter.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

RTMPDecoder::RTMPDecoder(const Handler& handler) :
	_decoded(0), _handshake(HANDSHAKE_FIRST), _chunkSize(RTMP::DEFAULT_CHUNKSIZE), _handler(handler),
	_unackBytes(0), _winAckSize(RTMP::DEFAULT_WIN_ACKSIZE)
#if defined(_DEBUG)
	, _readBytes(0)
#endif
	{
}

void RTMPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (!addStreamData(Packet(pBuffer), pSocket->recvBufferSize(), *pSocket)) {
		ERROR("RTMP message exceeds buffer maximum ", pSocket->recvBufferSize(), " size");
		pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception (works for HTTP and Upgrade session as WebSocket)
	}
}

UInt32 RTMPDecoder::onStreamData(Packet& buffer, Socket& socket) {
	do {
		// HANDSHAKE 1
		if (_handshake == HANDSHAKE_FIRST) {
			// Validate handshake
			if (buffer.size() < 1537)
				return buffer.size();
			if (buffer.size() > 1537) {
				ERROR("RTMP Handshake unknown");
				socket.shutdown(Socket::SHUTDOWN_RECV);
				return 0;
			}

			DUMP_REQUEST_DEBUG("RTMP", buffer.data(), buffer.size(), socket.peerAddress())

			UInt8 type(*buffer.data());
			if (type != 3 && type != 6) {
				ERROR("RTMP Handshake type '", type, "' unknown");
				socket.shutdown(Socket::SHUTDOWN_RECV);
				return 0;
			}

			Exception ex;

			bool middle;
			UInt32 keySize;
			const UInt8* key = RTMP::ValidateClient(buffer.data(), buffer.size(), middle, keySize); // size = HMAC_KEY_SIZE

			bool encrypted(type==6); // RTMPE?

			shared<Buffer> pBuffer(SET);
			BinaryWriter writer(*pBuffer);

			if (!key) {
				if (encrypted) {
					ERROR("RTMP Handshake encrypted without key")
					socket.shutdown(Socket::SHUTDOWN_RECV);
					return 0;
				}
				/// Simple Handshake ///

				writer.write8(3);
				//generate random data
				writer.writeRandom(1536);
				// write data
				writer.write(buffer.data() + 1, buffer.size() - 1);

				DUMP_RESPONSE_DEBUG(socket.isSecure() ? "RTMPS" : "RTMP", writer.data(), writer.size(), socket.peerAddress())
			} else {
				/// Complexe Handshake ///

				// encrypted flag
				writer.write8(type);

				//timestamp
				writer.write32(UInt32(Time::Now()));

				//version
				writer.write32(0);

				//generate random data
				writer.writeRandom(3064);

				if (encrypted) {
					unique<RC4_KEY>	pEncryptKey(SET);
					_pDecryptKey.set();

					UInt32 farPubKeySize;
					const UInt8* farPubKey = buffer.data() + RTMP::GetDHPos(buffer.data(), buffer.size(), middle, farPubKeySize);

					//compute DH key position
					UInt32 serverDHSize;
					UInt32 serverDHPos = RTMP::GetDHPos(writer.data(), writer.size(), middle, serverDHSize);

					UInt8 secret[DiffieHellman::SIZE];
					UInt8 size(0);
					//generate DH key
					DiffieHellman dh;
					do {
						AUTO_ERROR(dh.computeKeys(ex),"RTMPE Handshake");
						if (!dh)
							break;
						AUTO_ERROR(size = dh.computeSecret(ex, farPubKey, farPubKeySize, secret), "RTMPE Handshake");
					} while (size && (size != DiffieHellman::SIZE || dh.privateKeySize() != DiffieHellman::SIZE || dh.publicKeySize() != DiffieHellman::SIZE));
					if (!dh || !size) {
						socket.shutdown(Socket::SHUTDOWN_RECV);
						return 0;
					}
					RTMP::ComputeRC4Keys(dh.readPublicKey(pBuffer->data() + serverDHPos), DiffieHellman::SIZE, farPubKey, farPubKeySize, secret, size, *_pDecryptKey, *pEncryptKey);
					_handler.queue(onEncryptKey, pEncryptKey.release());
				}

				//generate the digest
				if (!RTMP::WriteDigestAndKey(key, keySize, middle, pBuffer->data(), pBuffer->size())) {
					socket.shutdown(Socket::SHUTDOWN_RECV);
					return 0;
				}

				DUMP_RESPONSE_DEBUG(encrypted ? "RTMPE" : "RTMP", writer.data(), writer.size(), socket.peerAddress())
			}
		
			_unackBytes += 1537;
			_handshake = HANDSHAKE_SECOND;

			AUTO_ERROR(socket.write(ex, Packet(pBuffer)) != -1, "RTMP Handshake");
			return 0; // buffer.size() == 1537 here, no remaining data possible!
		}


		// Dump just one time + decryption if RTMPE
		if (buffer.size() > _decoded) {
			if(_pDecryptKey) {
				RC4(_pDecryptKey.get(), buffer.size() - _decoded, buffer.data() + _decoded, BIN buffer.data() + _decoded);
				DUMP_REQUEST("RTMPE", buffer.data() + _decoded, buffer.size() - _decoded, socket.peerAddress());
			} else
				DUMP_REQUEST(socket.isSecure() ? "RTMPS" : "RTMP", buffer.data() + _decoded, buffer.size() - _decoded, socket.peerAddress());
			_decoded = buffer.size();
		}

		// HANDSHAKE 2
		if (_handshake == HANDSHAKE_SECOND) {
			if (buffer.size() < 1536)
				return buffer.size();
			// nothing to do!
			_decoded -= 1536;
			_unackBytes += 1536;
			_handshake = HANDSHAKE_DONE;
			buffer += 1536;
			continue;
		}

	//// RECEPTION => here at less one byte available
		BinaryReader reader(buffer.data(), buffer.size());

		UInt8 headerSize = reader.read8();
		UInt32 channelId = headerSize & 0x3F;

		headerSize = 12 - (headerSize >> 6) * 4;
		if (!headerSize)
			headerSize = 1;

		// if idWriter==0 id is encoded on the following byte, if idWriter==1 id is encoded on the both following byte
		if (channelId < 2)
			headerSize += channelId + 1;

		if (reader.size() < headerSize) // want read in first the header!
			return buffer.size();

		if (channelId < 2)
			channelId = (channelId == 0 ? reader.read8() : reader.read16()) + 64;

		Channel& channel(_channels.emplace(SET, forward_as_tuple(channelId), forward_as_tuple(channelId)).first->second);

		bool isRelative(true);
		if (headerSize >= 4) {

			// TIME
			channel.time = reader.read24();

			if (headerSize >= 8) {
				// SIZE
				channel.bodySize = reader.read24();
				// TYPE
				channel.type = AMF::Type(reader.read8());
				if (headerSize >= 12) {
					isRelative = false;
					// STREAM
					channel.streamId = reader.read8();
					channel.streamId += reader.read8() << 8;
					channel.streamId += reader.read8() << 16;
					channel.streamId += reader.read8() << 24;
				}
			}
		}

		if (channel.time >= 0xFFFFFF) {
			headerSize += 4;
			if (reader.available() < 4)
				return buffer.size();
			channel.time = reader.read32();
			isRelative = true; // is absolute here
		}

		UInt32 chunkSize(channel.bodySize);
		if (!channel()) {
			// New packet
			channel.reset();
			if (isRelative)
				channel.absoluteTime += channel.time; // relative
			else
				channel.absoluteTime = channel.time; // absolute
		} else if (channel->size() > chunkSize) {
			ERROR("Invalid RTMP packet, chunked message doesn't match bodySize")
			socket.shutdown(Socket::SHUTDOWN_RECV);
			return 0;
		} else
			chunkSize -= channel->size();

		if (_chunkSize && chunkSize > _chunkSize) // if chunkSize==0, no chunkSize!
			chunkSize = _chunkSize;
		if (reader.available() < chunkSize)
			return buffer.size();

		// data consumed!
		channel->append(reader.current(), chunkSize);
		chunkSize += headerSize; // + headerSize!

		buffer += chunkSize;
		_decoded -= chunkSize;
		_unackBytes += chunkSize;

		// entiere packet gotten?
		if (channel.bodySize <= channel->size()) {
			// packet ready!
			Packet packet(channel());

			switch (channel.type) {
				case AMF::TYPE_ABORT: {
					const auto& it(_channels.find(BinaryReader(packet.data(), packet.size()).read32()));
					if (it != _channels.end())
						it->second.clear();
					break;
				}
				case AMF::TYPE_CHUNKSIZE:
					_chunkSize = BinaryReader(packet.data(), packet.size()).read32();
					break;
				case AMF::TYPE_WIN_ACKSIZE:
					_winAckSize = BinaryReader(packet.data(), packet.size()).read32();
					break;
				case AMF::TYPE_ACK:
					// nothing to do, a ack message says about how many bytes have been gotten by the peer
					// RTMFP has a complexe ack mechanism and RTMP is TCP based, ack mechanism is in system layer => so useless
					break;
				default: {
					// ack if required
					UInt32 ackBytes(0);
					if (_unackBytes >= (_winAckSize>>1)) { // _winAckSize divised by 2 to be sure to get a "receive" to ack!
	#if defined(_DEBUG)
						_readBytes += _unackBytes;
						TRACE("Sending ACK : ", _readBytes, " bytes (_unackBytes: ", _unackBytes, ")");
	#endif
						ackBytes = _unackBytes;
						_unackBytes = 0;
					}
					_handler.queue(onRequest, channel.type, channel.absoluteTime, channel.id, channel.streamId, ackBytes, packet, !buffer);
					break;
				}
			}
		}
		
	} while (buffer);

	return 0;
}


} // namespace Mona
