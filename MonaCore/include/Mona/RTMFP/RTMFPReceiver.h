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

#pragma once

#include "Mona/Mona.h"
#include "Mona/Socket.h"
#include "Mona/ThreadPool.h"
#include "Mona/Handler.h"
#include "Mona/Crypto.h"
#include "Mona/Entity.h"
#include "Mona/RTMFP/RTMFP.h"

namespace Mona {

struct RTMFPReceiver : RTMFP::Session, virtual Object {
	RTMFPReceiver(const Handler& handler,
		UInt32 id, UInt32 farId,
		const UInt8* farPubKey, UInt8 farPubKeySize,
		const UInt8* decryptKey, const UInt8* encryptKey,
		const SocketAddress& address, const shared<RendezVous>& pRendezVous);

	UInt16					track;
	bool					obsolete(const Time& now = Time::Now());
	std::set<SocketAddress>	localAddresses;
	
	void	receive(Socket& socket, shared<Buffer>& pBuffer, const SocketAddress& address);
private:
	Buffer& write(Socket& socket, const SocketAddress& address, UInt8 type, UInt16 size);

	typedef std::function<void(UInt64 flowId, UInt32& lost, const Packet& packet)> Output;

	struct Flow {
		Flow(UInt64	id, const Output& output) : fragmentation(0), _stage(0), id(id), _lost(0), _stageEnd(0), _output(output) {}

		const UInt64	id;
		bool			consumed() const { return _stageEnd && _fragments.empty(); }
		UInt64			buildAck(std::vector<UInt64>& losts, UInt16& size);
		UInt32			fragmentation;

		void	input(UInt64 stage, UInt8 flags, const Packet& packet);
	private:
		void	onFragment(UInt64 stage, UInt8 flags, const Packet& packet);

		struct Fragment : Packet, virtual Object {
			Fragment(UInt8 flags, const Packet& packet) : flags(flags), Packet(std::move(packet)) {}
			const UInt8 flags;
		};

		const Output&				_output;
		UInt64						_stage;
		UInt64						_stageEnd;
		std::map<UInt64, Fragment>	_fragments;
		shared<Buffer>				_pBuffer;
		UInt32						_lost;
	};

	Output					_output;
	std::map<UInt64, Flow>	_flows;
	UInt32					_initiatorTime;
	UInt32					_echoTime;
	Time					_died;
	const Handler&			_handler;
	shared<Buffer>			_pBuffer;
	SocketAddress			_address;
	Time					_obsolete;
};



} // namespace Mona
