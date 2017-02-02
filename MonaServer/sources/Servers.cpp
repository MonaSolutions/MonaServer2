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

#include "Servers.h"
#include "Mona/Logs.h"


using namespace std;
using namespace Mona;



Servers::Servers(const Parameters& parameters, SocketEngine& engine) : _parameters(parameters), _running(false), _server(engine), _manageTimes(1) {
}

Servers::~Servers() {
	stop();
}

void Servers::manage() {
	if (!_running)
		return;
	if(_targets.empty() || (--_manageTimes)!=0)
		return;
	_manageTimes = 8; // every 16 sec
	for (ServerConnection* pTarget : _targets)
		pTarget->connect(_parameters);
}

void Servers::start() {
	if (_running)
		stop();
	_running = true;

	_server.onError = [this](const Mona::Exception& ex) { WARN("Servers, ", ex); };

	_server.onConnection = [this](const shared<Socket>& pSocket) {
		initConnection(**_clients.emplace(new ServerConnection(pSocket, _server.engine)).first).connect(_parameters);
	};

	SocketAddress address(IPAddress::Wildcard(), _parameters.getNumber<UInt16>("servers.port"));
	if (!address)
		return;
	Exception ex;
	bool success(false);
	AUTO_ERROR(success = _server.start(ex, address), "Servers");
	if (success)
		NOTE("Servers incoming connection on ", address, " started");


	string targets;
	if (!_parameters.getString("servers.targets", targets))
		return;

	String::ForEach forEach( [this](UInt32 index, const char* target) {
		const char* query = strchr(target,'?');
		if (query) {
			*(char*)query = '\0';
			++query;
		}
		SocketAddress address;
		Exception ex;
		bool success;
		AUTO_ERROR(success=address.set(ex, target), "Servers ", target, " target");
		if (success)
			initConnection(**_targets.emplace(new ServerConnection(address, _server.engine, query)).first).connect(_parameters);
		return true;
	});
	String::Split(targets, ";", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);
}

ServerConnection& Servers::initConnection(ServerConnection& server) {
	server.onDisconnection = [this, &server](const Exception& ex) {
		remove(&server);
		if (server.isTarget)
			targets.remove(&server);
		else
			initiators.remove(&server);

		if (ex)
			ERROR("Disconnection from ", server.address, " server, ", ex)
		else
			NOTE("Disconnection from ", server.address, " server ")

		onDisconnection(server, ex);

		if (_clients.erase(&server))
			delete &server;
	};

	server.onMessage = [this, &server](const string& name, BinaryReader& reader) {
		onMessage(server, name, reader);
	};

	server.onConnection = [this, &server]() {
		if (!add(&server)) {
			ERROR("Server ", server.address, " already connected");
			return;
		}
		if (server.isTarget)
			targets.add(&server);
		else
			initiators.add(&server);
		NOTE("Connection established with ", server.address, " server");
		onConnection(server);
	};
	return server;
}

void Servers::stop() {
	if (!_running)
		return;
	_running = false;
	if (_server.running()) {
		NOTE("Servers incoming connection on ",_server->address(), " stopped");
		_server.stop();
	}

	_server.onConnection = nullptr;
	_server.onError = nullptr;

	clear();
	targets.clear();
	initiators.clear();
	for (ServerConnection* pClient : _clients)
		delete pClient;
	_clients.clear();
	for (ServerConnection* pTarget : _targets)
		delete pTarget;
	_targets.clear();
}
