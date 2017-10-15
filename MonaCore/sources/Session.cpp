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
	_protocol(protocol), _name(name ? name : ""), api(protocol.api), died(false), _id(0), timeout(protocol.getNumber<UInt32>("timeout") * 1000) {
	init(*this);
}
	
Session::Session(Protocol& protocol, const SocketAddress& address, const char* name) : peer(*new Peer(protocol.api, protocol.name)),
	_protocol(protocol),_name(name ? name : ""), api(protocol.api), died(false), _id(0), timeout(protocol.getNumber<UInt32>("timeout") * 1000) {
	_pPeer.reset(&peer);
	peer.setAddress(address);
	init(*this);
}

Session::Session(Protocol& protocol, Session& session) : _pPeer(session._pPeer), peer(*session._pPeer),
	_sessionsOptions(session._sessionsOptions), _protocol(protocol), api(protocol.api), died(false), _id(session._id), timeout(protocol.getNumber<UInt32>("timeout") * 1000) {
	// Morphing
	session._name.clear(); // to fix name!
	((string&)peer.protocol) = protocol.name;
	peer.onParameters = nullptr;
	DEBUG("Upgrading ", session.name(), " to ", protocol.name, " session");
	init(session);
}

void Session::init(Session& session) {
	peer.setServerAddress(_protocol.address);
	peer.onParameters = [this, &session](Parameters& parameters) {
		struct Params : Parameters {
			Params(Protocol& protocol, Parameters& parameters) : protocol(protocol), Parameters(move(parameters)) {}
			const char* onParamUnfound(const std::string& key) const { return protocol.getString(key); }
			Protocol& protocol;
		};
		session.onParameters(Params(_protocol, parameters));
	};
	if(_pPeer.unique()) // in morphing case, use the kill "wrapper session" rather (already subscribed to kill of "wrapper session")
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
		return String::Assign(_name, peer.protocol," session ", _id); // use peer.protocol rather _protocol.name to expect session morphing
	return String::Assign(_name, peer.protocol, " session");
}

void Session::onParameters(const Parameters& parameters) {
	if (parameters.getNumber("timeout", _timeout))
		_timeout *= 1000;
}

void Session::kill(Int32 error, const char* reason) {
	if (died)
		return;
	if (error == ERROR_UNEXPECTED)
		ERROR("Unexpected close from ", name());
	// Unsubscribe onClose before onDisconnection which could close the main writer (and so recall kill)
	peer.onParameters = nullptr;
	peer.onClose = nullptr;
	peer.onDisconnection();
	(bool&)died = true; // keep in last to allow TCPSession::send in peer.onDisconnection!
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
	// Connection timeout to liberate useless socket ressource (usually used just for TCP session)
	// Control sending and receiving for protocol like HTTP which can streaming always in the same way (sending), without never more request (receiving)
	if (!timeout || (peer && (!peer.recvTime().isElapsed(timeout) || !peer.sendTime().isElapsed(timeout))) || !peer.disconnection.isElapsed(timeout))
		return true;
	INFO(name(), " timeout connection");
	kill(ERROR_IDLE);
	return false;
}


} // namespace Mona
