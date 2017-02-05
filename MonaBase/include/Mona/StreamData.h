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
#include "Mona/Buffer.h"

namespace Mona {

template<typename ...Args>
struct StreamData {

	bool addStreamData(shared<Buffer>&& pBuffer, UInt32 limit, Args... args) {
		if (_pBuffer)
			_pBuffer->append(pBuffer->data(), pBuffer->size());
		else
			_pBuffer = pBuffer;
		pBuffer = _pBuffer;
		// Just one time to prefer recursivity rather "while repeat", and allow a "flush" info!
		UInt32 rest(onStreamData(Packet(_pBuffer), std::forward<Args>(args)...));
		if (!rest) {
			pBuffer.reset();
			return true; // no rest, can have deleted this, so return immediatly!
		}
		if (rest > pBuffer->size())
			rest = pBuffer->size();
		if (rest > limit) {
			// test limit on rest no before to allow a pBuffer in input of limit size + pBuffer stored = limit size too
			pBuffer.reset();
			return false;
		}
		if (pBuffer.unique()) {
			if (rest < pBuffer->size()) {
				memmove(pBuffer->data(), pBuffer->data() + pBuffer->size() - rest, rest);
				pBuffer->resize(rest, true);
			}
			_pBuffer = std::move(pBuffer);
		} else {
			// new buffer because has been captured in a packet by decoding code
			_pBuffer.reset(new Buffer(rest, pBuffer->data() + pBuffer->size() - rest));
			pBuffer.reset();
		}
		return true;
	}
	void clearStreamData() { _pBuffer.reset(); }

	virtual UInt32 onStreamData(Packet& buffer, Args... args) = 0;
private:
	shared<Buffer> _pBuffer;
};

} // namespace Mona
