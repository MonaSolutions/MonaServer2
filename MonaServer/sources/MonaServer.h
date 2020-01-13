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

#include "Mona/Server.h"
#include "Mona/TerminateSignal.h"
#include "Mona/PersistentData.h"
#include "Service.h"


namespace Mona {

struct MonaServer : Server, private Service::Handler {

	MonaServer(const Parameters& configs, TerminateSignal& terminateSignal);
	~MonaServer();

private:
	//events
	void					onStart();
	void					onManage();
	void					onStop();

	SocketAddress& 			onHandshake(const Path& path, const std::string& protocol, const SocketAddress& address, const Parameters& properties, SocketAddress& redirection);
	void					onConnection(Exception& ex, Client& client, DataReader& inParams, DataWriter& outParams);
	void					onDisconnection(Client& client);
	void					onAddressChanged(Client& client, const SocketAddress& oldAddress);
	bool					onInvocation(Exception& ex, Client& client, const std::string& name, DataReader& reader, UInt8 responseType);
	bool					onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties, Client* pClient);

	bool					onPublish(Exception& ex, Publication& publication, Client* pClient);
	void					onUnpublish(Publication& publication, Client* pClient);

	bool					onSubscribe(Exception& ex, Subscription& subscription, Publication& publication, Client* pClient);
	void					onUnsubscribe(Subscription& subscription, Publication& publication, Client* pClient);

	// Application handler
	void					onStop(Service& service);

	lua_State*				_pState;
	TerminateSignal&		_terminateSignal;
	unique<Service>			_pService;
	std::set<Subscription*> _luaSubscriptions;

	std::set<Service*>		_servicesRunning;
	PersistentData			_persistentData;
	Parameters				_data;
	Path					_dataPath;
	bool					_starting;
};

} // namespace Mona

