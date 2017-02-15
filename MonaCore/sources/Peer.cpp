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

Peer::Peer(ServerAPI& api) : _rttvar(0), _rto(Net::RTO_INIT), _pWriter(&Writer::Null()), _pNetStats(NULL), _ping(0), pingTime(0), _api(api) {
}

Peer::~Peer() {
	if (connected)
		ERROR("Client ", Util::FormatHex(id, Entity::SIZE, string()), " deleting whereas connected, onDisconnection forgotten");
	// Ices subscription can happen on peer not connected (virtual member for group for example)
	for(auto& it: _ices) {
		((Peer*)it.first)->_ices.erase(this);
		delete it.second;
	}
}

void Peer::setAddress(const SocketAddress& address) {
	if (this->address == address)
		return;
	SocketAddress oldAddress(this->address);
	((SocketAddress&)this->address) = address;
	onAddressChanged(oldAddress);
	if (connected)
		_api.onAddressChanged(*this, oldAddress);
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

UInt16 Peer::setPing(UInt64 value) {
	
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

	pingTime.update();
	return _ping;
}

void Peer::setPath(const string& value) {
	if (String::ICompare(path, value) == 0)
		return;
	((string&)Client::path).assign(value);
	onDisconnection(); // disconnected if path change!
}

ICE& Peer::ice(const Peer& peer) {
	map<const Peer*,ICE*>::iterator it = _ices.begin();
	while(it!=_ices.end()) {
		if(it->first == &peer) {
			it->second->setCurrent(*this);
			return *it->second;
		}
		if(it->second->obsolete()) {
			delete it->second;
			_ices.erase(it++);
			continue;
		}
		if(it->first>&peer)
			break;
		++it;
	}
	if(it!=_ices.begin())
		--it;
	ICE& ice = *_ices.emplace_hint(it,&peer,new ICE(*this,peer))->second; // is offer
	((Peer&)peer)._ices[this] = &ice;
	return ice;
}

/// EVENTS ////////

void Peer::onHandshake(set<SocketAddress>& addresses) {
	_api.onHandshake(path,  protocol, address, properties(), addresses);
}

void Peer::onConnection(Exception& ex, Writer& writer, Net::Stats& netStats, DataReader& arguments,DataWriter& response) {
	if(!connected) {
		(bool&)connected = true;
		((Time&)connectionTime).update();
		_pWriter = &writer;
		_pNetStats = &netStats;
		writer.flushable = false; // response parameter causes unflushable to avoid a data corruption
		(bool&)writer.isMain = true;
		writer.onClose = onClose;
		

		string buffer;

		// reset default protocol parameters
		Parameters parameters;
		MapWriter<Parameters> parameterWriter(parameters);
		SplitWriter parameterAndResponse(parameterWriter,response);

		_api.onConnection(ex, *this, arguments, parameterAndResponse);
		if (!ex && !((Entity::Map<Client>&)_api.clients).emplace(*this).second) {
			ERROR(ex.set<Ex::Protocol>("Client ", Util::FormatHex(id, Entity::SIZE, buffer), " exists already"));
			_api.onDisconnection(*this);
		}
		if (ex) {
			(Time&)connectionTime = 0;
			(bool&)connected = false;
			writer.clear();
			(bool&)_pWriter->isMain = false;
			_pWriter = NULL;
			_pNetStats = NULL;
		} else {
			onParameters(parameters);
			DEBUG("Client ",address," connection")
		}
		writer.flushable = true; // remove unflushable
	} else
		ERROR("Client ", Util::FormatHex(id, Entity::SIZE, string()), " seems already connected!")
}

void Peer::onDisconnection() {
	if (!connected)
		return;
	(bool&)connected = false;
	(Time&)connectionTime = 0;
	if (!((Entity::Map<Client>&)_api.clients).erase(id))
		ERROR("Client ", Util::FormatHex(id, Entity::SIZE, string()), " seems already disconnected!");
	_pWriter->onClose = nullptr; // before close, no need to subscribe to event => already disconnecting!
	_api.onDisconnection(*this);
	(bool&)_pWriter->isMain = false;
	_pWriter = NULL;
	_pNetStats = NULL;
}

bool Peer::onInvocation(Exception& ex, const string& name, DataReader& reader, UInt8 responseType) {
	if (connected)
		return _api.onInvocation(ex, *this, name, reader, responseType);
	ERROR("RPC client before connection to ", path);
	return false;
}

bool Peer::onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties) {
	if(connected)
		return _api.onFileAccess(ex,mode,file, arguments, properties, this);
	ERROR(ex.set<Ex::Intern>("File '", file, "' access by a not connected client"));
	return false;
}



} // namespace Mona
