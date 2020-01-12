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

#include "Mona/RTMFP/RTMFPWriter.h"


using namespace std;


namespace Mona {


RTMFPWriter::RTMFPWriter(UInt64 id, UInt64 flowId, const Binary& signature, RTMFP::Output& output) :
		_repeatDelay(0), _output(output), _stageAck(0), _lostCount(0), _pQueue(SET, id, flowId, signature) {
}

void RTMFPWriter::fail() {
	// Close the current writer id and start with a new id (same instance object, reset)
	// Is called just by remote peer, and no messages are recoverable, just send a close message

	// clear resources excepting QoS to detect this lost flow of data
	_stageAck = 0;
	_repeatDelay = _lostCount = 0;
	_pQueue.set(_output.resetWriter(_pQueue->id), _pQueue->flowId, _pQueue->signature);
}

void RTMFPWriter::closing(Int32 code, const char* reason) {
	FlashWriter::closing(code, reason);
	if (code >= 0 && (_stageAck || _repeatDelay))
		newMessage(true); // Send a MESSAGE_END just in the case where the receiver has been created
}

void RTMFPWriter::acquit(UInt64 stageAck, UInt32 lostCount) {
	TRACE("Ack ", stageAck, " on writer ", _pQueue->id," (lostCount=",lostCount,")");
	// have to continue to become consumed even if writer closed!
	if (stageAck > _stageAck) {
		// progress!
		_stageAck = stageAck;
		_lostCount = 0;
		// reset repeat time on progression!
		_repeatDelay = _output.rto();
		_repeatTime.update();
		// continue sending
		_output.send(shared<RTMFPAcquiter>(SET, _pQueue, _stageAck));
		return;
	}
	if (!lostCount) {
		DEBUG("Ack ", stageAck, " obsolete on writer ", _pQueue->id);
		return;
	}
	if (lostCount > _lostCount) {
		/// emulate ERTO-timeout=ping if lost infos =>
		// repeating is caused by a gap in ack-range, it can be a packet lost or an non-ordering transfer
		// to avoid a self-sustaining congestion repeated just the first missing packet if lost infos are present
		// and just once time (raising=true, to emulate first RTMFP ERTO=ping), let do the trigger after
		repeatMessages(_lostCount = lostCount);
	}
}

void RTMFPWriter::repeatMessages(UInt32 lostCount) {
	if (lostCount) {
		// means that there is something lost! We can start a repeation without wait end of current sending
		_output.send(shared<RTMFPRepeater>(SET, _pQueue, lostCount>0xFF ? 0xFF : lostCount));
		return;
	}
	if (!_pQueue.unique())
		return; // wait next! is sending, wait before to repeat packets
	// REPEAT!
	if (_pQueue->empty()) {
		// nothing to repeat, stop repeat
		_repeatDelay = 0;
		return;
	}
	if (!_repeatTime.isElapsed(_repeatDelay))
		return;
	_repeatTime.update();
	if (_repeatDelay<7072)
		_repeatDelay = (UInt32)(_repeatDelay*1.4142);
	else
		_repeatDelay = 10000;
	_output.send(shared<RTMFPRepeater>(SET, _pQueue));
}

void RTMFPWriter::flushing() {
	// manage sub writers, erase them closed
	auto it = _writers.begin();
	while (it != _writers.end()) {
		if ((*it)->closed())
			it = _writers.erase(it);
		else
			++it;
	}

	repeatMessages();
	if (!_pSender)
		return;
	if (!_repeatDelay) {
		// start repeat messages
		_repeatDelay = _output.rto();
		_repeatTime.update();
	}
	_output.send(move(_pSender));
}

AMFWriter& RTMFPWriter::newMessage(bool reliable, Media::Data::Type type, const Packet& packet) {
	if (closed())
		return AMFWriter::Null();
	if (!_pSender)
		_pSender.set<RTMFPMessenger>(_pQueue);
	return ((RTMFPMessenger&)*_pSender).newMessage(reliable, type, packet);
}

AMFWriter& RTMFPWriter::write(AMF::Type type, UInt32 time, Media::Data::Type packetType, const Packet& packet, bool reliable) {
	if (type < AMF::TYPE_AUDIO || type > AMF::TYPE_VIDEO)
		time = 0; // Because it can "dropped" the packet otherwise (like if the Writer was not reliable!)

	AMFWriter& writer = newMessage(reliable, packetType, packet);
	writer->write8(type).write32(time);
	if (type == AMF::TYPE_DATA_AMF3)
		writer->write8(0);
	return writer;
}

void RTMFPWriter::sendMember(const UInt8* id) {
	DEBUG("Group member exchanged ", String::Hex(id, Entity::SIZE))
	newMessage(reliable)->write8(0x0b).write(id, Entity::SIZE);
	flush();
}



} // namespace Mona
