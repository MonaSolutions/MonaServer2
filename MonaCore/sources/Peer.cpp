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

#include "Mona/Peer.h"
#include "Mona/SplitWriter.h"
#include "Mona/MapWriter.h"
#include "Mona/ServerAPI.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

Peer::Peer(ServerAPI& api, const char* protocol, const SocketAddress& address) : _api(api), pingTime(0), // pingTime to 0 in beginning to force ping on session beginning for protocols requiring a writePing to compute it
	Client(protocol, address) {
}

Peer::~Peer() {
	if (connection)
		CRITIC("Client ", String::Hex(id, Entity::SIZE), " deleting whereas connected, onDisconnection forgotten");
}

void Peer::setAddress(const SocketAddress& address) {
	if (this->address == address)
		return;
	// Try to determine serverAddress.host with address: if address.host is loopback => serverAddress.host certainly too
	if (!serverAddress.host() && address.host().isLoopback())
		((IPAddress&)serverAddress.host()).set(address.host());
	// Change peer address
	SocketAddress oldAddress(this->address);
	((SocketAddress&)this->address) = address;
	onAddressChanged(oldAddress);
	if (connection)
		_api.onAddressChanged(self, oldAddress);
}

bool Peer::setServerAddress(const string& address) {
	// If assignation fails, the previous value is preserved (allow to try different assignation and keep just what is working)
	Exception ex;
	SocketAddress serverAddress;
	if (serverAddress.set(ex, address))
		return setServerAddress(serverAddress);
	if (!ex.cast<Ex::Net::Address::Port>())
		return false;
	// address contains just host!
	if(serverAddress.set(ex, address, this->serverAddress.port()))
		return setServerAddress(serverAddress);
	return false;
}

bool Peer::setServerAddress(const SocketAddress& address) {
	// If assignation fails, the previous value is preserved (allow to try different assignation and keep just what is working)
	if (!address)
		return false;
	if (address.host()) {
		if (address.port())
			((SocketAddress&)serverAddress).set(address);
		else
			((IPAddress&)serverAddress.host()).set(address.host());
		return true;
	}
	if (!address.port())
		return false;
	((SocketAddress&)serverAddress).setPort(address.port());
	return true;
}


/// EVENTS ////////

SocketAddress& Peer::onHandshake(SocketAddress& redirection) {
	_api.onHandshake(path,  protocol, address, properties(), redirection);
	if (redirection)
		INFO(protocol, " redirection from ", serverAddress, " to ", redirection);
	return redirection;
}

void Peer::onConnection(Exception& ex, Writer& writer, Net::Stats& netStats, DataReader& parameters, DataWriter& response) {
	if(disconnection) {
		setWriter(writer, netStats);
		writer.flushable = false; // response parameter causes unflushable to avoid a data corruption
		writer.onClose = onClose;

		// reset default protocol parameters
		Parameters outParams;
		MapWriter<Parameters> parameterWriter(outParams);
		SplitWriter parameterAndResponse(parameterWriter,response);

		_api.onConnection(ex, self, parameters, parameterAndResponse);
		if (!ex && !((Entity::Map<Client>&)_api.clients).emplace(self).second) {
			CRITIC(ex.set<Ex::Protocol>("Client ", String::Hex(id, Entity::SIZE), " exists already"));
			_api.onDisconnection(self);
		}
		if (ex) {
			writer.clear();
			writer.onClose = nullptr;
			setWriter();
		} else {
			((Time&)connection).update();
			(Time&)disconnection = 0;
			onParameters(outParams);
			DEBUG("Client ",address," connection")
		}
		writer.flushable = true; // remove unflushable
	} else
		CRITIC("Client ", String::Hex(id, Entity::SIZE), " seems already connected!")
}

void Peer::onDisconnection() {
	if (disconnection)
		return;
	(Time&)connection = 0;
	((Time&)disconnection).update();
	if (!((Entity::Map<Client>&)_api.clients).erase(id))
		CRITIC("Client ", String::Hex(id, Entity::SIZE), " seems already disconnected!");
	writer().onClose = nullptr; // before close, no need to subscribe to event => already disconnecting!
	_api.onDisconnection(*this);
	setWriter();
}

bool Peer::onInvocation(Exception& ex, const string& name, DataReader& reader, UInt8 responseType) {
	if (connection)
		return _api.onInvocation(ex, *this, name, reader, responseType);
	CRITIC("RPC client before connection to ", path);
	return false;
}

bool Peer::onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties) {
	if (!connection) {
		CRITIC(ex.set<Ex::Intern>(path, '/', file.name(), " access by a not connected client"));
		return false;
	}
	if(_api.onFileAccess(ex,mode,file, arguments, properties, this))
		return true;
	if (!ex)
		ERROR(ex.set<Ex::Permission>(path, '/', file.name(), " access denied by application ", path));
	return false;
}



} // namespace Mona
