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
		if (_pBuffer) {
			_pBuffer->append(packet.data(), packet.size());
			shared<Buffer> pBuffer(_pBuffer); // trick to keep reference to _pBuffer!
			Packet buffer(pBuffer);
			rest = min(onStreamData(buffer, std::forward<Args>(args)...), _pBuffer->size());
		} else {
			Packet buffer(packet);
			rest = min(onStreamData(buffer, std::forward<Args>(args)...), packet.size());
		}
		if (!rest) {
			// no rest, can have deleted this, so return immediatly!
			_pBuffer.reset();
			return true;
		}
		if (rest > limit) {
			// test limit on rest no before to allow a pBuffer in input of limit size + pBuffer stored = limit size too
			_pBuffer.reset();
			return false;
		}
		if (!_pBuffer) {
			_pBuffer.reset(new Buffer(rest, packet.data() + packet.size() - rest));
			return true;
		}
		if (!_pBuffer.unique()) { // if not unique => packet hold by user in "parse", buffer immutable now
			_pBuffer.reset(new Buffer(rest, _pBuffer->data() + _pBuffer->size() - rest));
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
