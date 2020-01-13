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
#include "App.h"


namespace Mona {


struct MonaTiny : Server {
	MonaTiny(TerminateSignal& terminateSignal, UInt16 cores = 0) :
		Server(cores), _terminateSignal(terminateSignal) {}

	virtual ~MonaTiny() { stop(); }


protected:

	//// Server Events /////

	void onStart();
	void onManage();
	void onStop();
	

	//// Client Events /////
	SocketAddress& onHandshake(const Path& path, const std::string& protocol, const SocketAddress& address, const Parameters& properties, SocketAddress& redirection);
	void onConnection(Exception& ex, Client& client, DataReader& inParams, DataWriter& outParams);
	void onDisconnection(Client& client);

	void onAddressChanged(Client& client, const SocketAddress& oldAddress);
	bool onInvocation(Exception& ex, Client& client, const std::string& name, DataReader& arguments, UInt8 responseType);
	bool onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties, Client* pClient);

	//// Publication Events /////

	bool onPublish(Exception& ex, Publication& publication, Client* pClient);
	void onUnpublish(Publication& publication, Client* pClient);

	bool onSubscribe(Exception& ex, Subscription& subscription, Publication& publication, Client* pClient);
	void onUnsubscribe(Subscription& subscription, Publication& publication, Client* pClient);

	TerminateSignal&			_terminateSignal;
	std::map<std::string,App*>	_applications;
};

} // namespace Mona
