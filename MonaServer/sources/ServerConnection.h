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

#include "Mona/TCPClient.h"
#include "Mona/AMFWriter.h"
#include "Mona/MapParameters.h"
#include "Mona/Event.h"
#include "Mona/BinaryReader.h"

class ServerMessage : public Mona::AMFWriter {
public:
	ServerMessage(const char* name) : name(name), AMFWriter(*new Mona::Buffer()) {
		_pBuffer.reset(&(*this)->buffer());
		(*this)->next(260);
	}

	const char* name;

	const shared<const Mona::Binary>& buffer() { return _pBuffer; }

private:
	shared<const Mona::Binary> _pBuffer;
};


class ServerConnection : public Mona::MapParameters {
public:
	typedef Mona::Event<void()>													   ON(Connection);
	typedef Mona::Event<void(const std::string& name, Mona::BinaryReader& reader)> ON(Message);
	typedef Mona::Event<void(const Mona::Exception& ex)>						   ON(Disconnection);

	// Target version
	ServerConnection(const Mona::SocketAddress& address, Mona::SocketEngine& engine, const char* query);
	// Initiator version
	ServerConnection(const shared<Mona::Socket>& pSocket, Mona::SocketEngine& engine);

	~ServerConnection();

	const Mona::SocketAddress	address;
	const bool					isTarget;

	void			connect(const Mona::Parameters& parameters);
	bool			connected() { return _pClient->connected(); }
	void			disconnect(const char* error=NULL);

	void			send(ServerMessage& message);
	
	bool			protocolAddress(Mona::Exception& ex, const std::string& protocol, Mona::SocketAddress& socketAddress);

private:
	void			init();

	Mona::MapParameters					_properties;

	std::map<std::string,Mona::UInt32>	_sendRefs;
	std::map<Mona::UInt32,std::string>	_recvRefs;

	bool								_connected;
	Mona::Exception						_ex;
	unique<Mona::TCPClient>	_pClient;

};

