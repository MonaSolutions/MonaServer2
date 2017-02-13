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

#include "Mona/Session.h"

using namespace std;

namespace Mona {

Int32 Session::ToError(const Exception& ex) {
	if (ex.cast<Ex::Application::Error>() || ex.cast<Ex::Permission>())
		return ERROR_REJECTED;
	if (ex.cast<Ex::Application>() || ex.cast<Ex::Protocol>())
		return ERROR_PROTOCOL;
	if (ex.cast<Ex::Unsupported>())
		return ERROR_UNSUPPORTED;
	if (ex.cast<Ex::Unavailable>())
		return ERROR_UNAVAILABLE;
	if (ex.cast<Ex::Unfound>())
		return ERROR_UNFOUND;
	if (ex.cast<Ex::Net>())
		return ERROR_SOCKET;
	return ERROR_UNEXPECTED;
}

Session::Session(Protocol& protocol, const shared<Peer>& pPeer, const char* name) : _pPeer(pPeer), peer(*pPeer),
	_protocol(protocol), _name(name ? name : ""), api(protocol.api), died(false), _id(0) {
	init();
}
	
Session::Session(Protocol& protocol, const SocketAddress& address, const char* name) : peer(*new Peer(protocol.api)),
	_protocol(protocol),_name(name ? name : ""), api(protocol.api), died(false), _id(0) {
	_pPeer.reset(&peer);
	peer.setAddress(address);
	init();
}

Session::Session(Protocol& protocol, Session& session) : _pPeer(session._pPeer), peer(*session._pPeer),
	_sessionsOptions(session._sessionsOptions), _protocol(protocol), api(protocol.api), died(false), _id(session._id) {
	// Morphing
	// Same id but no same name! Useless to subscribe to kill here, "session" wrapper have already it, and they share the same peer!
	((string&)peer.protocol) = protocol.name;
	DEBUG("Upgrading ", session.name(), " to ", protocol.name, " session");
}

void Session::init() {
	((string&)peer.protocol) = _protocol.name;

	if (!peer.serverAddress)
		((SocketAddress&)peer.serverAddress).set(_protocol.address);
	else if (!peer.serverAddress.port())
		((SocketAddress&)peer.serverAddress).setPort(_protocol.address.port());

	DEBUG("peer.id ", Util::FormatHex(peer.id, Entity::SIZE, string()));

	peer.onParameters = [this](Parameters& parameters) {
		struct Params : Parameters {
			Params(Protocol& protocol, Parameters& parameters) : protocol(protocol), Parameters(move(parameters)) {}
			const char* onParamUnfound(const std::string& key) const { return protocol.getString(key); }
			Protocol& protocol;
		};
		onParameters(Params(_protocol, parameters));
	};
	peer.onClose = [this](Int32 error, const char* reason) { kill(error, reason); };
}

Session::~Session() {
	if (!died)
		CRITIC(name(), " deleted without being closed");
}

const string& Session::name() const {
	if (!_name.empty())
		return _name;
	if(_id)
		return String::Assign(_name, _protocol.name," session ", _id);
	return String::Assign(_name, _protocol.name, " session");;
}

void Session::kill(Int32 error, const char* reason) {
	if (died)
		return;
	(bool&)died = true;
	peer.onParameters = nullptr;
	if (error == ERROR_UNEXPECTED)
		ERROR("Unexpected close from ", name());
	// If Peer is shared, maybe it's also in an other session (morphing for example), so call kill of other possible session by this trick way
	if (!_pPeer.unique()) {
		_pPeer.reset(); // to avoid to call again kill morphing session from master session
		peer.onClose(error, reason);
	}
	// Unsubscribe onClose before onDisconnection which could close the main writer (and so recall kill)
	peer.onClose = nullptr;
	peer.onDisconnection();
}

bool Session::manage() {
	if (died)
		return false;
	// Congestion timeout to avoid to saturate a client saturating ressource + PULSE congestion variable of peer!
	if (peer.congested(Net::RTO_MAX)) {
		// Control sending and receiving for protocol like HTTP which can streaming always in the same way (sending), without never more request (receiving)
		WARN(name(), " congested");
		kill(ERROR_CONGESTED);
		return false;
	}
	return true;
}


} // namespace Mona
