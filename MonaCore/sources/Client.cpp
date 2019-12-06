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

#include "Mona/Client.h"

using namespace std;


namespace Mona {

Client::Client(const char* protocol, const SocketAddress& address) : // if broadcast address we are on a dummy client, so emulate connection (see Script::Client())!
	protocol(protocol), _pData(NULL), connection(!address.port() && address.host().isBroadcast() ? Time::Now() : 0),
	_pNetStats(&Net::Stats::Null()), _pWriter(&Writer::Null()), disconnection(Time::Now()), // disconnection has to be on Now value to be used as a timeout reference without onConnection call
	_rttvar(0), _rto(Net::RTO_INIT), _ping(0), address(address) {
	((IPAddress&)serverAddress.host()).set(address.host()); // Try to determine serverAddress.host with address
}

const string* Client::setProperty(const string& key, DataReader& reader) {
	string value;
	StringWriter<string> writer(value);
	reader.read(writer, 1);
	reader.reset();
	return onSetProperty(key, &reader) ? &_properties.emplace(key, move(value)).first->second : NULL;
}
Int8 Client::eraseProperty(const std::string& key) {
	auto it = _properties.find(key);
	if (it != _properties.end())
		return 0;
	if (!onSetProperty(key, NULL))
		return -1;
	_properties.erase(it);
	return 1;
}

UInt16 Client::setPing(UInt64 value) {
	if (value == 0)
		value = 1;
	else if (value > 0xFFFF)
		value = 0xFFFF;

	// Smoothed Round Trip time https://tools.ietf.org/html/rfc2988

	if (!_rttvar)
		_rttvar = value / 2.0;
	else
		_rttvar = ((3*_rttvar) + abs(_ping - UInt16(value)))/4.0;

	if (_ping == 0)
		_ping = UInt16(value);
	else if (value != _ping)
		_ping = UInt16((7*_ping + value) / 8.0);

	_rto = (UInt32)(_ping + (4*_rttvar) + 200);
	if (_rto < Net::RTO_MIN)
		_rto = Net::RTO_MIN;
	else if (_rto > Net::RTO_MAX)
		_rto = Net::RTO_MAX;

	return _ping;
}

void Client::setWriter(Writer& writer, Net::Stats& netStats) {
	_pWriter = &writer;
	_pNetStats = &netStats;
	if (writer) { // connection
		((Time&)connection).update();
		(Time&)disconnection = 0;
	} else { // disconnection
		((Time&)disconnection).update();
		(Time&)connection = 0;
	}
}


} // namespace Mona
