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
	// Try to determine serverAddress.host with address
	if (!serverAddress.host() && address.host())
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
	if(!connection) {
		if (!writer) {
			ERROR(ex.set<Ex::Intern>("Client writer given for connection is closed"));
			return;
		}
		setWriter(writer, netStats);
		writer.onClose = [&ex](Int32 error, const char* reason) {
			// anticipate a writer.close() inside _api.onConnection (no log, it's the application user which choose to display it or not)
			ex.set<Ex::Application>(reason ? reason : ""); // no need to have the exact Ex::Type, writer.close have been called manually with the type!
		};
		Util::Scoped<bool> noFlushable(writer.flushable, false); // response parameter causes unflushable to avoid a data corruption
		// reset default protocol parameters
		Parameters outParams;
		MapWriter<Parameters> parameterWriter(outParams);
		SplitWriter parameterAndResponse(parameterWriter,response);
		_api.onConnection(ex, self, parameters, parameterAndResponse);
		if (!ex && !((Entity::Map<Client>&)_api.clients).emplace(self).second) {
			CRITIC(ex.set<Ex::Protocol>("Client ", String::Hex(id, Entity::SIZE), " exists already"));
			_api.onDisconnection(self);
		}
		writer.onClose = nullptr;
		if (ex) { // writer can have been closed manually in onConnection!
			writer.clear();
			setWriter();
		} else {
			writer.onClose = onClose;
			DEBUG("Client ", address, " connection");
			onParameters(outParams);
		}
	} else
		CRITIC("Client ", String::Hex(id, Entity::SIZE), " seems already connected!")
}

void Peer::onDisconnection() {
	if (!connection)
		return;
	writer().onClose = nullptr; // before close, no need to subscribe to event => already disconnecting!
	_api.onDisconnection(self);
	if (!((Entity::Map<Client>&)_api.clients).erase(id))
		CRITIC("Client ", String::Hex(id, Entity::SIZE), " seems already disconnected!");
	setWriter(); // in last to allow a last message!
}

bool Peer::onInvocation(Exception& ex, const string& name, DataReader& reader, UInt8 responseType) {
	if (!connection) {
		CRITIC("RPC client before connection to ", path.length() ? path : "/");
		return false;
	}
	if (_api.onInvocation(ex, *this, name, reader, responseType))
		return true;
	// method not found!
	if(!ex)
		ex.set<Ex::Application>("Method client ", name, " not found in application ", path.length() ? path : "/"); // no log just in this case!
	return false; // method not found!
}

bool Peer::onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties) {
	if (!connection) {
		CRITIC(ex.set<Ex::Intern>(path, file.name(), " access by a not connected client"));
		return false;
	}
	if(_api.onFileAccess(ex,mode,file, arguments, properties, this))
		return true;
	if (!ex)
		ERROR(ex.set<Ex::Permission>(path, file.name(), " access denied by application ", path.length() ? path : "/"));
	return false;
}



} // namespace Mona
