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
#include "Mona/FlashStream.h"

namespace Mona {

struct FlashMainStream : FlashStream, virtual Object {
	FlashMainStream(ServerAPI& api, Peer& peer);
	virtual ~FlashMainStream();

	UInt16 keepaliveServer;
	UInt16 keepalivePeer;

	FlashStream* getStream(UInt16 id);
	void		 clearStreams();

	void flush() { for(auto& it : _streams) it.second.flush(); }
	
private:
	void	messageHandler(const std::string& name, AMFReader& message, FlashWriter& writer, Net::Stats& netStats);
	void	rawHandler(UInt16 type, const Packet& packet, FlashWriter& writer);

	std::map<UInt16,FlashStream>	_streams;
};


} // namespace Mona
