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
	if (!_pQueue) {
		// COMMAND
		shared<Buffer> pBuffer;
		BinaryWriter(RTMFP::InitBuffer(pBuffer)).write24(_sendable << 16);
		RTMFP::Send(pSession->socket(), Mona::Packet(pSession->encoder().encode(pBuffer, pSession->farId(), address)), address);
		return true;
	}
	if (stageAck > _pQueue->stage) {
		ERROR("stageAck ", stageAck, " superior to stage ", _pQueue->stage, " on writer ", _pQueue->id);
		stageAck = _pQueue->stage;
	}
	// ACK!
	while (!_pQueue->empty() && stageAck > _pQueue->stageQueue) {
		_pQueue->stageQueue += _pQueue->front().fragments;
		_pQueue->pop_front();
		_sendable = RTMFP::SENDABLE_MAX; // has progressed, can send max!
	}

	// Fill queue
	run(*_pQueue);

	// Flush Queue!
	if (!_sendable)
		return true;
	bool oneReliable = false;
	UInt64 abandonStage = 0;
	UInt64 stage = _pQueue->stageQueue;
	for (Packet& packet : *_pQueue) {
		TRACE("Stage ", stage + 1);
		stage += packet.fragments;
		if (packet.reliable) {
			oneReliable = true;
			if (abandonStage) {
				sendAbandon(abandonStage);
				abandonStage = 0;
			}
			if (!send(packet))
				break;
		} else if (packet) {
			// unreliable always not sent!
			if (!send(packet))
				break;
		} else if (!oneReliable) {
			abandonStage = stage;
			pSession->sendLostRate += packet.sizeSent();
		}
		if (_fragments) {
			if (packet.fragments >= _fragments)
				break;
			_fragments -= packet.fragments;
		}
	}
	if (abandonStage)
		sendAbandon(abandonStage);

	return true;
}

void RTMFPSender::sendAbandon(UInt64 stage) {
	shared<Buffer> pBuffer;
	BinaryWriter writer(RTMFP::InitBuffer(pBuffer));
	writer.write8(0x10).write16(2 + Binary::Get7BitValueSize(_pQueue->id) + Binary::Get7BitValueSize(stage));
	writer.write8(RTMFP::MESSAGE_ABANDON).write7BitLongValue(_pQueue->id).write7BitLongValue(stage).write8(0);
	RTMFP::Send(pSession->socket(), Mona::Packet(pSession->encoder().encode(pBuffer, pSession->farId(), address)), address);
}


bool RTMFPSender::send(Packet& packet) {
	if (!_sendable)
		return false;
	if (!RTMFP::Send(pSession->socket(), packet, address)) {
		_sendable = 0;
		return false;
	}
	if (packet.setSent()) {
		pSession->sendTime = Time::Now();
		pSession->sendByteRate += packet.sizeSent();
		_pQueue->congestion -= packet.size();
	}
	return --_sendable ? true : false;
}


UInt32 RTMFPMessenger::headerSize(Queue& queue) { // max size header = 50
	UInt32 size = Binary::Get7BitValueSize(queue.id);
	size += Binary::Get7BitValueSize(queue.stage);
	size += Binary::Get7BitValueSize(queue.stage - stageAck);
	size += stageAck ? 0 : (queue.signature.size() + (queue.flowId ? (4 + Binary::Get7BitValueSize(queue.flowId)) : 2));
	return size;
}


void RTMFPMessenger::run(Queue& queue) {
	for (Message& message : _messages)
		write(queue, message);
	flush(queue);
}

void RTMFPMessenger::flush(Queue& queue) {
	if (!_pBuffer)
		return;
	// encode and add to _pQueue
	queue.emplace_back(pSession->encoder().encode(_pBuffer, pSession->farId(), address), _fragments, _flags&RTMFP::MESSAGE_RELIABLE ? true : false);
	queue.congestion += queue.back().size();
}

void RTMFPMessenger::write(Queue& queue, const Message& message) {

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
		++queue.stage;

		UInt32 contentSize(size);

		UInt32 headerSize(13); // 13 bytes = 6 bytes of low header + marker byte + time 2 bytes + type byte + size 2 bytes + flags byte
		if (header)
			headerSize += this->headerSize(queue);

		UInt32 availableToWrite(RTMFP::SIZE_PACKET);
		if(_pBuffer)
			availableToWrite -= _pBuffer->size();
		// headerSize+16 to avoid a useless fragment (without payload data)
		if (!_pBuffer || (headerSize + 16) > availableToWrite || message.reliable != (_flags&RTMFP::MESSAGE_RELIABLE ? true : false)) {
			flush(queue);
			// New packet
			if (!header) {
				headerSize += this->headerSize(queue);
				header = true;
			}
			RTMFP::InitBuffer(_pBuffer);
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
		if (contentSize == 0) // just possible for MESSAGE_END
			_flags = RTMFP::MESSAGE_ABANDON | RTMFP::MESSAGE_END; // MESSAGE_END is always with MESSAGE_ABANDON otherwise flash client can crash
		else {
			_flags = (_flags&RTMFP::MESSAGE_WITH_AFTERPART) ? RTMFP::MESSAGE_WITH_BEFOREPART : 0;
			if (!stageAck && header)
				_flags |= RTMFP::MESSAGE_OPTIONS;
			if (size > 0)
				_flags |= RTMFP::MESSAGE_WITH_AFTERPART;
		}
		
		BinaryWriter writer(*_pBuffer);
		writer.write8(header ? 0x10 : 0x11);
		writer.write16(headerSize - 12 + contentSize);
		writer.write8(_flags);
		if (message.reliable)
			_flags |= RTMFP::MESSAGE_RELIABLE;

		if (header) {
			writer.write7BitLongValue(queue.id);
			writer.write7BitLongValue(queue.stage);
			writer.write7BitLongValue(queue.stage - stageAck);
			header = false;
			// signature
			if (!stageAck) {
				writer.write8(UInt8(queue.signature.size())).write(queue.signature);
				// No write this in the case where it's a new flow (create on server side)
				if (queue.flowId) {
					writer.write8(1 + Binary::Get7BitValueSize(queue.flowId)); // following size
					writer.write8(0x0a); // Unknown!
					writer.write7BitLongValue(queue.flowId);
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


} // namespace Mona
