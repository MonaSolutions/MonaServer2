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

#define KILL(error) {_died.update(); echoTime=error; size = reader.available();}

RTMFPReceiver::RTMFPReceiver(const Handler& handler, UInt32 id, UInt32 farId, const Packet& farPubKey, const UInt8* decryptKey, const UInt8* encryptKey, const shared<Socket>& pSocket, const SocketAddress& address) :
	RTMFP::Session(id, farId, farPubKey, encryptKey), _expTime(0xFFFFFFFF), _echoTime(0xFFFFFFFF), _handler(handler), _pDecoder(new RTMFP::Engine(decryptKey)), track(0), _died(0), _obsolete(0), _weakSocket(pSocket), _address(address),
	_output([this](UInt64 flowId, UInt32& lost, const Packet& packet){
		_handler.queue(onMessage, flowId, lost, packet);
		lost = 0;
	}) {
}

bool RTMFPReceiver::obsolete() {
	// Just called by RTFMPDecoder when this is unique!
	if (!_obsolete) {
		if (!_died)
			_died.update(Time::Now()-2001); // to send 0x4C
		_obsolete.update();
		return false;
	}
	return _obsolete.isElapsed(19000); // 19 seconds!
}


void RTMFPReceiver::receive(shared<Buffer>& pBuffer, const SocketAddress& address) {
	shared<Socket> pSocket(_weakSocket.lock());
	if (!pSocket)
		return; // protocol died!
	if (_died) {
		if (_died.isElapsed(2000)) {
			_died.update();
			BinaryWriter(RTMFP::InitBuffer(_pBuffer)).write24(0x4c << 16);
			RTMFP::Send(*pSocket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
		}
		return;
	}
	Exception ex;
	bool success;
	AUTO_ERROR(success = _pDecoder->decode(ex, *pBuffer, address), "RTMFP session ", id);
	if (!success)
		return;

	if (_address != address) {
		// address has changed!
		_address = address;
		_handler.queue(onAddress, address);
	}

	Packet buffer(pBuffer);
	BinaryReader reader(buffer.data(), buffer.size());
	// Read packet
	UInt8 marker = reader.read8() | 0xF0;
	UInt16 time = reader.read16();

	Time expTime(time == _expTime ? 0 : RTMFP::Time(time)); // to avoid to use a repeating packet version
	_expTime = time;

	Int32 echoTime(0xFFFFFFFF);
	// with time echo
	if (marker == 0xFD) {
		time = reader.read16(); // ping
		if (time != _echoTime)  // to avoid to use a repeating packet version
			_echoTime = echoTime = time;
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
				BUFFER_RESET(_pBuffer, 6);  // overwrite!
				write(*pSocket, address, expTime, 0x4c, 0);
			case 0x4c:
				KILL(0);
				break;
			/// KeepAlive
			case 0x01:
				write(*pSocket, address, expTime, 0x41, 0);
				break;
			case 0x41:
				keepalive = true;
				break;
			case 0x5e: {
				// RTMFPWriter exception!
				UInt64 id(message.read7BitLongValue());
				acks[id].reset();
				// trick to avoid a repeating exception message from client
				BinaryWriter(write(*pSocket, address, expTime, 0x10, 3 + Binary::Get7BitValueSize(id)))
					.write8(RTMFP::MESSAGE_ABANDON | RTMFP::MESSAGE_END).write7BitLongValue(id).write16(0); // Stage=0 and DeltaAck = 0 to avoid to request a ack for that
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
				UInt64 id = message.read7BitLongValue();
				UInt64 bufferSize = message.read7BitLongValue();
				if (bufferSize) {
					auto& it = acks.emplace(id, Packet());
					if (it.second || it.first->second) { // to avoid to override a RTMFPWriter fails!
						if(type == 0x50) {
							// convert to 0x51!
							const UInt8* begin(message.current());
							UInt64 stage(message.read7BitLongValue());
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
							it.first->second.set(Packet(buffer, begin, (lostBuffer - begin) + BinaryWriter(lostBuffer, 4).write7BitValue(lostCount).size()));
						} else
							it.first->second.set(Packet(buffer, message.current(), message.available()));
					}
				} else {
					// no more place to write, reliability broken
					WARN("RTMFPWriter ", id, " can't deliver its data, client buffer full");
					KILL(Mona::Session::ERROR_PROTOCOL);
				}
				break;
			}
			/// Request
			// 0x10 normal request
			// 0x11 special request, in repeat case (following stage request)
			case 0x10: {
				flags = message.read8();
				flowId = message.read7BitLongValue();
				stage = message.read7BitLongValue() - 1;
				message.read7BitLongValue(); // deltaNAck

				if (flags & RTMFP::MESSAGE_OPTIONS) {
					// flow creation
					*(UInt8*)message.current() |= RTMFP::MESSAGE_OPTIONS;
					pFlow = &_flows.emplace(piecewise_construct, forward_as_tuple(flowId), forward_as_tuple(flowId, _output)).first->second;
				} else {
					const auto& it = _flows.find(flowId);
					if (it == _flows.end()) {
						DEBUG("Message for flow ", flowId, " unknown or flow already closed");
						BinaryWriter(write(*pSocket, address, expTime, 0x5e, 1 + Binary::Get7BitValueSize(flowId)))
							.write7BitLongValue(flowId).write8(0);
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
				if (pFlow)
					pFlow->input(stage, flags, Packet(buffer, message.current(), message.available()));
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
				vector<UInt64> losts;
				stage = pFlow->buildAck(losts, size = 0);
				size += Binary::Get7BitValueSize(pFlow->id) + Binary::Get7BitValueSize(0xFF7Fu) + Binary::Get7BitValueSize(stage);
				BinaryWriter writer(write(*pSocket, address, expTime, 0x51, size));
				writer.write7BitLongValue(pFlow->id).write7BitValue(0xFF7F).write7BitLongValue(stage);
				for (UInt64 lost : losts)
					writer.write7BitLongValue(lost);
				if (pFlow->consumed())
					_flows.erase(pFlow->id);
				pFlow = NULL;
			} else {
				// commit everything (flow unknown)
				BinaryWriter(write(*pSocket, address, expTime, 0x51, 1 + Binary::Get7BitValueSize(flowId) + Binary::Get7BitValueSize(stage)))
					.write7BitLongValue(flowId).write7BitValue(0).write7BitLongValue(stage);
					
			}
			stage = 0;
		}
	}

	if(_pBuffer)
		RTMFP::Send(*pSocket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
	_handler.queue(onFlush, echoTime, keepalive, _died ? true : false, acks);
}

Buffer& RTMFPReceiver::write(Socket& socket, const SocketAddress& address, Time& expTime, UInt8 type, UInt16 size) {
	if (_pBuffer && size > (RTMFP::SIZE_PACKET - _pBuffer->size()))
		RTMFP::Send(socket, Packet(pEncoder->encode(_pBuffer, farId, address)), address);
	BinaryWriter(_pBuffer ? *_pBuffer : RTMFP::InitBuffer(_pBuffer, expTime)).write8(type).write16(size);
	return *_pBuffer;
}
	
UInt64 RTMFPReceiver::Flow::buildAck(vector<UInt64>& losts, UInt16& size) {
	// Lost informations!
	UInt64 stage = _stage;
	auto it = _fragments.begin();
	while (it != _fragments.end()) {
		stage = it->first - stage - 2;
		size += Binary::Get7BitValueSize(stage);
		losts.emplace_back(stage); // lost count
		UInt32 buffered(0);
		stage = it->first;
		while (++it != _fragments.end() && it->first == (++stage))
			++buffered;
		size += Binary::Get7BitValueSize(buffered);
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
	if (stage>nextStage) {
		// not following stage, bufferizes the stage
		if(!_fragments.emplace(piecewise_construct, forward_as_tuple(stage), forward_as_tuple(flags, packet)).second)
			DEBUG("Stage ", stage, " on flow ", id, " has already been received")
		else if (_fragments.size()>100)
			DEBUG("_fragments.size()=", _fragments.size());
	} else {
		onFragment(nextStage++, flags, packet);
		auto& it = _fragments.begin();
		while (it != _fragments.end() && it->first <= nextStage) {
			onFragment(nextStage++, it->second.flags, it->second);
			it = _fragments.erase(it);
		}
		if (_fragments.empty() && _stageEnd)
			_output(id, _lost, Packet::Null()); // end flow!
	}
}
	
void RTMFPReceiver::Flow::onFragment(UInt64 stage, UInt8 flags, const Packet& packet) {
	_stage = stage;
	// If MESSAGE_ABANDON, abandon the current packet (happen on lost data)
	if (flags&RTMFP::MESSAGE_ABANDON) {
		if (_pBuffer) {
			_lost += packet.size(); // this fragment abandonned
			_lost += _pBuffer->size(); // the bufferized fragment abandonned
			DEBUG("Fragments lost on flow ", id);
			_pBuffer.reset();
		}
		return;
	}

	if (_pBuffer) {
		_pBuffer->append(packet.data(), packet.size());
		if (flags&RTMFP::MESSAGE_WITH_AFTERPART)
			return;
		Packet packet(_pBuffer);
		if (packet)
			_output(id, _lost, packet);
		return;
			
	}
	if (flags&RTMFP::MESSAGE_WITH_AFTERPART) {
		_pBuffer.reset(new Buffer(packet.size(), packet.data()));
		return;
	}
	if(packet)
		_output(id, _lost, packet);
}

} // namespace Mona
