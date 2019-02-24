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

#include "Mona/RTMFP/RTMFPSender.h"
#include "Mona/BinaryWriter.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

bool RTMFPSender::run(Exception&) {	
	run();
	if (!pQueue)
		return true;

	// Flush Queue!
	while (pSession->sendable && !pQueue->empty()) {
		TRACE("Stage ", pQueue->stageSending+1, " sent");
		shared<Packet>& pPacket(pQueue->front());
		if (!RTMFP::Send(pSession->socket, *pPacket, address)) {
			pSession->sendable = 0;
			break;
		}
		--pSession->sendable;
		pSession->sendTime = Time::Now();
		pSession->sendByteRate += pPacket->size();
		pSession->queueing -= pPacket->size();
		pPacket->setSent();
		pQueue->stageSending += pPacket->fragments;
		pQueue->sending.emplace_back(pPacket);
		pQueue->pop_front();
	}
	return true;
}

void RTMFPCmdSender::run() {
	// COMMAND
	shared<Buffer> pBuffer;
	BinaryWriter(RTMFP::InitBuffer(pBuffer, pSession->initiatorTime())).write24(UInt32(_cmd << 16));
	RTMFP::Send(pSession->socket, Mona::Packet(pSession->pEncoder->encode(pBuffer, pSession->farId(), address)), address);
}

void RTMFPAcquiter::run() {
	// ACK!
	if (_stageAck > pQueue->stageSending) {
		ERROR("stageAck ", _stageAck, " superior to sending stage ", pQueue->stageSending, " on writer ", pQueue->id);
		_stageAck = pQueue->stageSending;
	}
	while (!pQueue->sending.empty() && _stageAck > pQueue->stageAck) {
		pQueue->stageAck += pQueue->sending.front()->fragments;
		pQueue->sending.pop_front();
		pSession->sendable = RTMFP::SENDABLE_MAX; // has progressed, can send max!
	}
}

void RTMFPRepeater::run() {
	// REPEAT
	bool oneReliable = false;
	UInt64 abandonStage = 0;
	UInt64 stage = pQueue->stageAck;
	UInt8 sendable(RTMFP::SENDABLE_MAX);
	for (shared<Packet>& pPacket : pQueue->sending) {
		DEBUG("Stage ", stage + 1, " repeated");
		stage += pPacket->fragments;
		if (pPacket->reliable) {
			oneReliable = true;
			if (abandonStage) {
				sendAbandon(abandonStage);
				abandonStage = 0;
			}
			if (!RTMFP::Send(pSession->socket, *pPacket, address)) {
				pSession->sendable = 0; // pause sending!
				break;
			}
			if (!--sendable)
				break;
		} else if (!oneReliable) {
			abandonStage = stage;
			pSession->sendLostRate += pPacket->sizeSent();
		}
		if (_fragments) {
			if (pPacket->fragments >= _fragments)
				break;
			_fragments -= pPacket->fragments;
		}
	}
	if (abandonStage)
		sendAbandon(abandonStage);
}

void RTMFPRepeater::sendAbandon(UInt64 stage) {
	shared<Buffer> pBuffer;
	BinaryWriter writer(RTMFP::InitBuffer(pBuffer, pSession->initiatorTime()));
	writer.write8(0x10).write16(2 + Binary::Get7BitSize<UInt64>(pQueue->id, 10) + Binary::Get7BitSize<UInt64>(stage, 10));
	writer.write8(RTMFP::MESSAGE_ABANDON).write7Bit<UInt64>(pQueue->id, 10).write7Bit<UInt64>(stage, 10).write8(0);
	RTMFP::Send(pSession->socket, Mona::Packet(pSession->pEncoder->encode(pBuffer, pSession->farId(), address)), address);
}


UInt32 RTMFPMessenger::headerSize() { // max size header = 50
	UInt32 size = Binary::Get7BitSize<UInt64>(pQueue->id, 10);
	size += Binary::Get7BitSize<UInt64>(pQueue->stage, 10);
	size += Binary::Get7BitSize<UInt64>(pQueue->stage - pQueue->stageAck, 10);
	size += pQueue->stageAck ? 0 : (pQueue->signature.size() + (pQueue->flowId ? (4 + Binary::Get7BitSize<UInt64>(pQueue->flowId, 10)) : 2));
	return size;
}


void RTMFPMessenger::run() {
	for (Message& message : _messages) {
		message.convertToAMF();

		UInt32 size = message.packet.size();
		UInt32 available = size;
		const UInt8* current;
		if (message.writer) {
			current = message.writer->data();
			available = message.writer->size();
			size += available;
		} else
			current = message.packet.data();

		bool header(!_pBuffer);
		do {
			++pQueue->stage;

			UInt32 contentSize(size);

			UInt32 headerSize(RTMFP::SIZE_HEADER + 4); // 4 bytes = type + UInt16(size) + flag
			if (header)
				headerSize += this->headerSize();

			UInt32 availableToWrite(RTMFP::SIZE_PACKET);
			if (_pBuffer)
				availableToWrite -= _pBuffer->size();
			// headerSize+16 to avoid a useless fragment (without payload data)
			if (!_pBuffer || (headerSize + 16) > availableToWrite || message.reliable != (_flags&RTMFP::MESSAGE_RELIABLE ? true : false)) {
				flush();
				// New packet
				if (!header) {
					headerSize += this->headerSize();
					header = true;
				}
				RTMFP::InitBuffer(_pBuffer, pSession->initiatorTime());
				_fragments = 1;

				if ((headerSize + contentSize) > RTMFP::SIZE_PACKET)
					contentSize = RTMFP::SIZE_PACKET - headerSize;

			} else {
				if ((headerSize + contentSize)>availableToWrite)
					contentSize = availableToWrite - headerSize;
				++_fragments;
			}

			size -= contentSize;

			// Compute flags
			if (contentSize) {
				_flags = (_flags&RTMFP::MESSAGE_WITH_AFTERPART) ? RTMFP::MESSAGE_WITH_BEFOREPART : 0;
				if (!pQueue->stageAck && header)
					_flags |= RTMFP::MESSAGE_OPTIONS;
				if (size > 0)
					_flags |= RTMFP::MESSAGE_WITH_AFTERPART;
			} else // contentSize=0 => just possible for MESSAGE_END
				_flags = RTMFP::MESSAGE_ABANDON | RTMFP::MESSAGE_END; // MESSAGE_END is always with MESSAGE_ABANDON otherwise flash client can crash

			BinaryWriter writer(*_pBuffer);
			writer.write8(header ? 0x10 : 0x11);
			writer.write16(headerSize - RTMFP::SIZE_HEADER - 3 + contentSize);
			writer.write8(_flags);
			if (message.reliable)
				_flags |= RTMFP::MESSAGE_RELIABLE;

			if (header) {
				writer.write7Bit<UInt64>(pQueue->id, 10);
				writer.write7Bit<UInt64>(pQueue->stage, 10);
				writer.write7Bit<UInt64>(pQueue->stage - pQueue->stageAck, 10);
				header = false;
				// signature
				if (!pQueue->stageAck) {
					writer.write8(UInt8(pQueue->signature.size())).write(pQueue->signature);
					// No write this in the case where it's a new flow (create on server side)
					if (pQueue->flowId) {
						writer.write8(1 + Binary::Get7BitSize<UInt64>(pQueue->flowId, 10)); // following size
						writer.write8(0x0a); // Unknown!
						writer.write7Bit<UInt64>(pQueue->flowId, 10);
					}
					writer.write8(0); // marker of end for this part
				}
			}

			if (contentSize>available) {
				writer.write(current, available);
				available = contentSize - available;
				current = message.packet.data();
				writer.write(current, available);
				current += available;
				available = size;
			} else {
				writer.write(current, contentSize);
				current += contentSize;
				available -= contentSize;
			}

		} while (size);
	}
	flush();
}

void RTMFPMessenger::flush() {
	if (!_pBuffer)
		return;
	// encode and add to pQueue
	pQueue->emplace_back(SET, pSession->pEncoder->encode(_pBuffer, pSession->farId(), address), _fragments, _flags&RTMFP::MESSAGE_RELIABLE ? true : false);
	pSession->queueing += pQueue->back()->size();
}


} // namespace Mona
