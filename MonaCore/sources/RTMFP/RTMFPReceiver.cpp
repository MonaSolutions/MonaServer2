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

#include "Mona/RTMFP/RTMFPReceiver.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {

#define KILL(error) { _pBuffer.reset(); _died.update(); ping=error; size = reader.available();}

RTMFPReceiver::RTMFPReceiver(const Handler& handler,
		UInt32 id, UInt32 farId,
		const UInt8* farPubKey, UInt8 farPubKeySize,
		const UInt8* decryptKey, const UInt8* encryptKey,
		const SocketAddress& address, const shared<RendezVous>& pRendezVous) : RTMFP::Session(id, farId, farPubKey, farPubKeySize, decryptKey, encryptKey, pRendezVous),
			_initiatorTime(-1), _echoTime(-1), _handler(handler), track(0), _died(0), _obsolete(0), _address(address),
			_output([this](UInt64 flowId, UInt32& lost, const Packet& packet) {
				_handler.queue(onMessage, flowId, lost, packet);
				lost = 0;
			}) {
}

bool RTMFPReceiver::obsolete(const Time& now) {
	// Just called by RTFMPDecoder when this is unique!
	if (!_obsolete) {
		if (!_died)
			_died.update(now -2001); // to send 0x4C
		_obsolete.update();
		return false;
	}
	return (now - _obsolete) > 19000; // 19 seconds!
}


void RTMFPReceiver::receive(Socket& socket, shared<Buffer>& pBuffer, const SocketAddress& address) {
	if (_died) {
		if (_died.isElapsed(2000)) {
			_died.update();
			BinaryWriter(RTMFP::InitBuffer(_pBuffer)).write24(0x4c << 16);
			RTMFP::Send(socket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
		}
		return;
	}
	Exception ex;
	bool success;
	AUTO_ERROR(success = pDecoder->decode(ex, *pBuffer, address), "RTMFP session ", id);
	if (!success)
		return;

	if (_address != address) {
		// address has changed!
		_address = address;
		_handler.queue(onAddress, address);
	}

	Packet packet(pBuffer);
	BinaryReader reader(packet.data(), packet.size());
	// Read packet
	UInt8 marker = reader.read8() | 0xF0;
	UInt16 time = reader.read16();

	if (time != _initiatorTime) { // to avoid to use a repeating packet version
		_initiatorTime = time;
		initiatorTime = Time::Now() - (time*RTMFP::TIMESTAMP_SCALE);
	}

	Int32 ping(-1);
	if (marker == 0xFD) {
		// with time echo!
		time = reader.read16(); // echo time
		if (time != _echoTime) {  // to avoid to use a repeating packet version
			_echoTime = time;
			time = RTMFP::TimeNow() - time; // in first to stay in UInt16
			ping = time*RTMFP::TIMESTAMP_SCALE;
		}
	} else if (marker != 0xF9)
		WARN("Packet marker ", String::Format<UInt8>("%02x", marker)," unknown");

	// Variables for request (0x10 and 0x11)
	UInt8 flags;
	Flow* pFlow = NULL;
	UInt64 flowId;
	UInt64 stage = 0;
	UInt8 type = reader.read8();
	map<UInt64, Packet> acks;
	bool keepalive(false);
		
	// Can have nested queries
	while (type && type!=0xFF) { // 00 or FF = RTMFP padding!

		UInt16 size = reader.read16();

		BinaryReader message(reader.current(), size);

		switch (type) {
			case 0x0c:
				// just 4C message!
				BinaryWriter(RTMFP::InitBuffer(_pBuffer)).write24(0x4c << 16);
				RTMFP::Send(socket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
			case 0x4c:
				KILL(0);
				break;
			/// KeepAlive
			case 0x01:
				write(socket, address, 0x41, 0);
				break;
			case 0x41:
				keepalive = true;
				break;
			case 0x5e: {
				// RTMFPWriter exception!
				UInt64 id = message.read7Bit<UInt64>(10);
				acks[id].reset();
				// trick to avoid a repeating exception message from client
				BinaryWriter(write(socket, address, 0x10, 3 + Binary::Get7BitSize<UInt64>(id, 10)))
					.write8(RTMFP::MESSAGE_ABANDON | RTMFP::MESSAGE_END).write7Bit<UInt64>(id, 10).write16(0); // Stage=0 and DeltaAck = 0 to avoid to request a ack for that
				break;
			}
			case 0x18:
				/// This response is sent when we answer with a Acknowledgment negative
				// It contains the id flow (message.read8())
				// I don't unsertand the usefulness...
				// For the moment, we considerate it like a exception, and should never happened (server sends never a negative ack)
				WARN("Ack negative from server"); // send fail message immediatly
				KILL(Mona::Session::ERROR_PROTOCOL);
				break;

			case 0x50:
			case 0x51: {
				// Ack RTMFPWriter
				UInt64 id = message.read7Bit<UInt64>(10);
				UInt64 bufferSize = message.read7Bit<UInt64>(10);
				if (bufferSize) {
					const auto& it = acks.emplace(id, Packet());
					if (it.second || it.first->second) { // to avoid to override a RTMFPWriter fails!
						if(type == 0x50) {
							// convert to 0x51!
							const UInt8* begin(message.current());
							message.read7Bit<UInt64>(10); // stage!
							UInt8* lostBuffer(BIN message.current());
							UInt32 lostCount(0);
							while (message.available()) {
								UInt8 bits(message.read8());
								for (UInt8 i = 0; i < 8; ++i) {
									if (bits & 1) {
										message.next(message.available());
										break;
									}
									++lostCount;
									bits >>= 1;
								}
							}
							it.first->second.set(Packet(packet, begin, (lostBuffer - begin) + BinaryWriter(lostBuffer, 4).write7Bit<UInt64>(lostCount).size()));
						} else
							it.first->second.set(Packet(packet, message.current(), message.available()));
					}
				} else {
					const auto& it = acks.find(id);
					if (it == acks.end() || it->second) {
						// no more place to write, reliability broken
						WARN("RTMFPWriter ", id, " can't deliver its data, client buffer full");
						KILL(Mona::Session::ERROR_PROTOCOL);
					} // ack on a failed writer, ignore!
				}
				break;
			}
			/// Request
			// 0x10 normal request
			// 0x11 special request, in repeat case (following stage request)
			case 0x10: {
				flags = message.read8();
				flowId = message.read7Bit<UInt64>(10);
				stage = message.read7Bit<UInt64>(10) - 1;
				message.read7Bit<UInt64>(10); // deltaNAck

				if (flags & RTMFP::MESSAGE_OPTIONS) {
					// flow creation
					pFlow = &_flows.emplace(SET, forward_as_tuple(flowId), forward_as_tuple(flowId, _output)).first->second;
				} else {
					const auto& it = _flows.find(flowId);
					if (it == _flows.end()) {
						DEBUG("Message for flow ", flowId, " unknown or flow already closed");
						BinaryWriter(write(socket, address, 0x5e, 1 + Binary::Get7BitSize<UInt64>(flowId, 10)))
							.write7Bit<UInt64>(flowId, 10).write8(0);
					} else
						pFlow = &it->second;
				}

			}
			case 0x11: {
				if (type == 0x11) {
					flags = message.read8();
					if (!stage) {
						ERROR("0x11 following message without 0x10 beginning");
						break;
					}
				}
				++stage;	
				if (!pFlow)
					break;
				pFlow->input(stage, flags, Packet(packet, message.current(), message.available()));
				if (pFlow->fragmentation > socket.recvBufferSize()) {
					ERROR("Peer continue to send packets until exceeds buffer capacity whereas lost data have been requested");
					KILL(Mona::Session::ERROR_CONGESTED);
				}
				break;
			}
			default:
				ERROR("Message type '", String::Format<UInt8>("%02x", type), "' unknown");
		}

		// Next
		reader.next(size);
		type = reader.read8();

		// Commit RTMFPFlow (pFlow means 0x11 or 0x10 message)
		if (stage && type != 0x11) {
			if (pFlow) {
				// Write ack with a buffer information of 0xFF7Fu (max possible) => to simplify protocol and guarantee max exchange (max buffer always available)
				vector<UInt64> losts;
				stage = pFlow->buildAck(losts, size = 0);
				size += Binary::Get7BitSize<UInt64>(pFlow->id, 10) + Binary::Get7BitSize<UInt64>(0xFF7F) + Binary::Get7BitSize<UInt64>(stage, 10);
				BinaryWriter writer(write(socket, address, 0x51, size));
				writer.write7Bit<UInt64>(pFlow->id, 10).write7Bit<UInt64>(0xFF7F).write7Bit<UInt64>(stage, 10);
				for (UInt64 lost : losts)
					writer.write7Bit<UInt64>(lost, 10);
				if (pFlow->consumed())
					_flows.erase(pFlow->id);
				pFlow = NULL;
			} else {
				// commit everything (flow unknown, can be a flow which has failed)
				BinaryWriter(write(socket, address, 0x51, 1 + Binary::Get7BitSize<UInt64>(flowId, 10) + Binary::Get7BitSize<UInt64>(stage, 10)))
					.write7Bit<UInt64>(flowId, 10).write7Bit<UInt64>(0).write7Bit<UInt64>(stage, 10);
					
			}
			stage = 0;
		}
	}

	if(_pBuffer)
		RTMFP::Send(socket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
	_handler.queue(onFlush, ping, keepalive, _died ? true : false, acks);
}

Buffer& RTMFPReceiver::write(Socket& socket, const SocketAddress& address, UInt8 type, UInt16 size) {
	if (_pBuffer && size > (RTMFP::SIZE_PACKET - _pBuffer->size()))
		RTMFP::Send(socket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
	BinaryWriter(_pBuffer ? *_pBuffer : RTMFP::InitBuffer(_pBuffer, initiatorTime)).write8(type).write16(size);
	return *_pBuffer;
}
	
UInt64 RTMFPReceiver::Flow::buildAck(vector<UInt64>& losts, UInt16& size) {
	// Lost informations!
	UInt64 stage = _stage;
	auto it = _fragments.begin();
	while (it != _fragments.end()) {
		stage = it->first - stage - 2;
		size += Binary::Get7BitSize<UInt64>(stage, 10);
		losts.emplace_back(stage); // lost count
		UInt32 buffered(0);
		stage = it->first;
		while (++it != _fragments.end() && it->first == (++stage))
			++buffered;
		size += Binary::Get7BitSize<UInt64>(buffered, 10);
		losts.emplace_back(buffered);
		--stage;
	}
	return _stage;
}

void RTMFPReceiver::Flow::input(UInt64 stage, UInt8 flags, const Packet& packet) {
	if (_stageEnd) {
		if (_fragments.empty()) {
			// if completed accept anyway to allow ack and avoid repetition
			_stage = stage; 
			return; // completed!
		}
		if (stage > _stageEnd) {
			DEBUG("Stage ", stage, " superior to stage end ", _stageEnd, " on flow ", id);
			return;
		}
	} else if (flags&RTMFP::MESSAGE_END)
		_stageEnd = stage;

	UInt64 nextStage = _stage + 1;
	if (stage < nextStage) {
		DEBUG("Stage ", stage, " on flow ", id, " has already been received");
		return;
	}

	// If MESSAGE_ABANDON, abandon all stage inferior to abandon stage + clear current packet bufferized
	if (flags&RTMFP::MESSAGE_ABANDON) {
		// Compute a lost estimation
		UInt32 lost = UInt32((stage - nextStage)*RTMFP::SIZE_PACKET);
		if (!(flags&RTMFP::MESSAGE_END)) // END has always ABANDON too => no data lost!
			lost += RTMFP::SIZE_PACKET/2; // estimation...
		// Remove obsolete fragments
		nextStage = stage + 1;
		auto it = _fragments.begin();
		while (it != _fragments.end() && it->first < nextStage) {
			lost += it->second.size();
			fragmentation -= it->second.size();
			++it;
		}
		_fragments.erase(_fragments.begin(), it);
		// Abandon buffer
		if (_pBuffer) {	
			lost += _pBuffer->size();
			_pBuffer.reset();
		}
		if (lost) {
			DEBUG("Fragments ", _stage + 1, " to ", stage, " lost on flow ", id);
			_lost += lost;
		}
		_stage = stage; // assign new stage
	} else if (stage>nextStage) {
		// not following stage, bufferizes the stage
		if(_fragments.empty())
			DEBUG("Wait stage ", nextStage, " lost on flow ", id);
		if (_fragments.emplace(SET, forward_as_tuple(stage), forward_as_tuple(flags, packet)).second) {
			fragmentation += packet.size();
			if (_fragments.size()>100)
				DEBUG("_fragments.size()=", _fragments.size());
		} else
			DEBUG("Stage ", stage, " on flow ", id, " has already been received")
		return;
	} else
		onFragment(nextStage++, flags, packet);

	auto it = _fragments.begin();
	while (it != _fragments.end() && it->first <= nextStage) {
		onFragment(nextStage++, it->second.flags, it->second);
		fragmentation -= it->second.size();
		it = _fragments.erase(it);
	}
	if (_fragments.empty() && _stageEnd)
		_output(id, _lost, Packet::Null()); // end flow!
}
	
void RTMFPReceiver::Flow::onFragment(UInt64 stage, UInt8 flags, const Packet& packet) {
	_stage = stage;
	if (_pBuffer) {
		BinaryReader reader(packet.data(), packet.size());
		if (flags & RTMFP::MESSAGE_OPTIONS) {
			// remove options on concatenation!
			while (UInt8 length = reader.read8())
				reader.next(length);
		}
		_pBuffer->append(reader.current(), reader.available());
		if (flags&RTMFP::MESSAGE_WITH_AFTERPART)
			return;
		Packet packet(_pBuffer);
		if (packet)
			_output(id, _lost, packet);
		return;	
	}
	if (flags&RTMFP::MESSAGE_WITH_BEFOREPART) {
		DEBUG("Fragment  ", stage," lost on flow ", id);
		_lost += packet.size();
		return; // the beginning of this message is lost, ignore it!
	}

	// add options flag on AMF type on message beginning (compatible with AMF type value!)
	if (flags & RTMFP::MESSAGE_OPTIONS)
		*(UInt8*)packet.data() |= RTMFP::MESSAGE_OPTIONS;
	if (flags&RTMFP::MESSAGE_WITH_AFTERPART) {
		_pBuffer.set(packet.data(), packet.size());
		return;
	}
	if(packet)
		_output(id, _lost, packet);
}

} // namespace Mona
