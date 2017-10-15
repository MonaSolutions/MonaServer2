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
#include "Mona/SocketAddress.h"
#include "Mona/Entity.h"
#include "Mona/Writer.h"
#include "Mona/Parameters.h"
#include "Mona/DataReader.h"
#include <vector>

namespace Mona {

struct Client : Entity, virtual NullableObject, Net::Stats {
	typedef Event<bool(DataReader& reader, std::string& value)> ON(CallProperties);

	const SocketAddress			address;
	const SocketAddress			serverAddress;

	const std::string			protocol;

	virtual const Parameters&	properties() const = 0;

	operator bool() const		{ return connection ? true : false; }
	const Time					connection;
	const Time					disconnection;

	 // user data (custom data)
	template <typename DataType>
	DataType* setCustomData(DataType* pData) const { return (DataType*)(_pData = pData); }
	bool	  hasCustomData() const { return _pData != NULL; }
	template<typename DataType>
	DataType* getCustomData() const { return (DataType*)_pData; }

	// Alterable in class children Peer
	
	const std::string			path;
	const std::string			query;
	
	virtual UInt16				ping() const { return 0; } // 0 for protocol which doesn't support PING calculation
	virtual UInt32				rto() const { return Net::RTO_MAX; }
	virtual bool				congested(UInt32 duration = Net::RTO_INIT) = 0;

	virtual Writer&				writer() = 0;

protected:
	Client(const char* protocol) : protocol(protocol), _pData(NULL), connection(0), disconnection(Time::Now()) {}

private:
	mutable void*				_pData;
};


} // namespace Mona
