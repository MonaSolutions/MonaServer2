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
	if (!ex)
		return 0; // normal!
	if (ex.cast<Ex::Application>())
		return ERROR_APPLICATION;
	if (ex.cast<Ex::Permission>())
		return ERROR_REJECTED;
	if (ex.cast<Ex::Protocol>() || ex.cast<Ex::Format>())
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

Session::Session(Protocol& protocol, shared<Peer>& pPeer, const char* name) : _pPeer(move(pPeer)), peer(*_pPeer),
	_protocol(protocol), _name(name ? name : ""), api(protocol.api), died(false), _id(0), timeout(protocol.getNumber<UInt32>("timeout") * 1000) {
	init(self);
}
	
Session::Session(Protocol& protocol, const SocketAddress& address, const char* name) : peer(_pPeer.set(protocol.api, protocol.name, address)),
	_protocol(protocol),_name(name ? name : ""), api(protocol.api), died(false), _id(0), timeout(protocol.getNumber<UInt32>("timeout") * 1000) {
	init(self);
}

Session::Session(Protocol& protocol, Session& session) : _pPeer(session._pPeer), peer(*session._pPeer),
	_sessionsOptions(session._sessionsOptions), _protocol(protocol), api(protocol.api), died(false), _id(session._id), timeout(protocol.getNumber<UInt32>("timeout") * 1000) {
	// Morphing
	session._name.clear(); // to fix name!
	peer.protocol = protocol.name;
	peer.onParameters = nullptr;
	DEBUG("Upgrading ", session.name(), " to ", protocol.name, " session");
	init(session);
}

void Session::init(Session& session) {
	if(!peer.serverAddress) // set serverAddress if not already set
		peer.setServerAddress(_protocol.publicAddress);
	peer.onParameters = [this, &session](Parameters& parameters) {
		struct Params : Parameters {
			Params(Protocol& protocol, Parameters& parameters) : protocol(protocol), Parameters(move(parameters)) {}
			const string* onParamUnfound(const string& key) const { return protocol.getParameter(key); }
			Protocol& protocol;
		};
		session.onParameters(Params(_protocol, parameters));
	};
	if (!peer.onClose) // in morphing case it's already subscribed
		peer.onClose = [this](Int32 error, const char* reason) { kill(error ? error : ERROR_REJECTED, reason); };
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
	_congestion = peer.queueing();
	if (_congestion(Net::RTO_MAX + Net::RTO_INIT)) {
		// Control sending and receiving for protocol like HTTP which can streaming always in the same way (sending), without never more request (receiving)
		WARN(name(), " congested");
		close(ERROR_CONGESTED);
		return false;
	}
	// Connection timeout to liberate useless socket ressource (usually used just for TCP session)
	// Control sending and receiving for protocol like HTTP which can streaming always in the same way (sending), without never more request (receiving)
	if (!timeout || (peer && (!peer.recvTime().isElapsed(timeout) || !peer.sendTime().isElapsed(timeout))) || !peer.disconnection.isElapsed(timeout))
		return true;
	LOG(String::ICompare(_protocol.name, EXPAND("HTTP"))==0 ? LOG_DEBUG : LOG_INFO, name(), " timeout connection");
	close(ERROR_IDLE);
	return false;
}


} // namespace Mona
