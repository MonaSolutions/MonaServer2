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
#include "Mona/StringReader.h"
#include <vector>

namespace Mona {

struct Client : Entity, virtual Object, Net::Stats {
	typedef Event<bool(const std::string& key, DataReader* pReader)>	ON(SetProperty);
	NULLABLE(!connection || !_pWriter->operator bool())

	Client(const char* protocol, const SocketAddress& address);

	const SocketAddress			address;
	const SocketAddress			serverAddress;

	const char*					protocol;

	/*!
	Can be usefull in some protocol implementation to allow to change client property (like HTTP and Cookie),
	reader has in first argument the value to assign and the rest is assignation parameters */
	const std::string*			setProperty(const std::string& key, DataReader& reader);
	Int8						eraseProperty(const std::string& key);
	const Parameters&			properties() const { return _properties; }

	const Time					connection;
	const Time					disconnection;

	 // user data (custom data)
	template <typename DataType>
	DataType*			setCustomData(DataType* pData) { _pData = pData; return pData; }
	bool				hasCustomData() const { return _pData != NULL; }
	template<typename DataType>
	DataType*			getCustomData() const { return (DataType*)_pData; }

	// Alterable in class children Peer
	/*!
	Path folder, "" or "live/" for example */
	const Path					path;
	const std::string			query;
	
	UInt16						ping() const { return _ping; } // 0 for protocol which doesn't support PING calculation
	UInt32						rto() const { return _rto; }

	UInt64						queueing() const { return _pNetStats->queueing(); }

	Time						recvTime() const { return _pNetStats->recvTime(); }
	UInt64						recvByteRate() const { return _pNetStats->recvByteRate(); }
	double						recvLostRate() const { return _pNetStats->recvLostRate(); }

	Time						sendTime() const { return _pNetStats->sendTime(); }
	UInt64						sendByteRate() const { return _pNetStats->sendByteRate(); }
	double						sendLostRate() const { return _pNetStats->sendLostRate(); }
	
	Writer&						writer() { return *_pWriter; }

	Writer&						newWriter() { return writer().newWriter(); }
	DataWriter&					writeInvocation(const char* name) { return writer().writeInvocation(name); }
	DataWriter&					writeMessage() { return writer().writeMessage(); }
	DataWriter&					writeResponse(UInt8 type = 0) { return writer().writeResponse(type); }
	void						writeRaw(DataReader& arguments, const Packet& packet = Packet::Null()) { writer().writeRaw(arguments, packet); }
	void						writeRaw(const Packet& packet) { writer().writeRaw(packet); }
	bool						flush() { return writer().flush(); }
	void						close(Int32 error = 0, const char* reason = NULL) { writer().close(error, reason); }

protected:
	
	void setWriter(Writer& writer = Writer::Null(), Net::Stats& netStats = Net::Stats::Null());

	UInt16		setPing(UInt64 value);
private:
	mutable void*				_pData;
	Net::Stats*					_pNetStats;
	Writer*						_pWriter;
	Parameters					_properties;
	UInt16						_ping;
	UInt32						_rto;
	double						_rttvar;
	
};



} // namespace Mona
