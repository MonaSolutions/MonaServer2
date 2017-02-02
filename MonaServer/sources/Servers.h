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


#include "Broadcaster.h"
#include "Mona/TCPServer.h"


class Servers : public Broadcaster {
public:
	typedef Mona::Event<void(ServerConnection& server)>														  ON(Connection);
	typedef Mona::Event<void(ServerConnection& server, const std::string& name, Mona::BinaryReader& reader)>  ON(Message);
	typedef Mona::Event<void(ServerConnection& server, const Mona::Exception& ex)>							  ON(Disconnection);

	Servers(const Mona::Parameters& parameters, Mona::SocketEngine& engine);
	virtual ~Servers();
	
	void start();
	void manage();
	void stop();

	Broadcaster	initiators;
	Broadcaster	targets;


private:
	ServerConnection& initConnection(ServerConnection& server);

	const Mona::Parameters&					_parameters;

	Mona::UInt8								_manageTimes;
	bool									_running;

	std::set<ServerConnection*>				_targets;
	std::set<ServerConnection*>				_clients;

	Mona::TCPServer							_server;
};

