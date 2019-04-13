/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Packet.h"

namespace Mona {

template<typename ...Args>
struct StreamData : virtual Object {

	bool addStreamData(const Packet& packet, UInt32 limit, Args... args) {
		// Call onStreamData just one time to prefer recursivity rather "while repeat", and allow a "flush" info!
		UInt32 rest;
		shared<Buffer> pBuffer(std::move(_pBuffer)); // because onStreamData returning 0 can delete this!
		if (pBuffer) {
			pBuffer->append(packet.data(), packet.size());
			Packet buffer(static_pointer_cast<const Binary>(pBuffer)); // trick to keep reference to _pBuffer!
			rest = min(onStreamData(buffer, std::forward<Args>(args)...), pBuffer->size());
		} else {
			Packet buffer(packet);
			rest = min(onStreamData(buffer, std::forward<Args>(args)...), packet.size());
		}
		if (!rest) // no rest, can have deleted this, so return immediatly!
			return true;
		if (rest > limit) // test limit on rest no before to allow a pBuffer in input of limit size + pBuffer stored = limit size too
			return false;
		_pBuffer = std::move(pBuffer);
		if (!_pBuffer) { // copy!
			_pBuffer.set(packet.data() + packet.size() - rest, rest);
			return true;
		}
		if (!_pBuffer.unique()) { // if not unique => packet hold by user in "parse", buffer immutable now
			_pBuffer.set(_pBuffer->data() + _pBuffer->size() - rest, rest);
			return true;
		}
		if (rest < _pBuffer->size()) {
			memmove(_pBuffer->data(), _pBuffer->data() + _pBuffer->size() - rest, rest);
			_pBuffer->resize(rest, true);
		}
		return true;
	}
	void clearStreamData() { _pBuffer.reset(); }
	shared<Buffer>& clearStreamData(shared<Buffer>& pBuffer) { return pBuffer = std::move(_pBuffer); }

private:
	virtual UInt32 onStreamData(Packet& buffer, Args... args) = 0;

	shared<Buffer> _pBuffer;
};

} // namespace Mona
