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

#include "Mona/Mona.h"
#include "Mona/ICE.h"
#include "Mona/ServerAPI.h"
#include "Mona/Group.h"
#include "Mona/Congestion.h"


namespace Mona {


struct Peer : Client, virtual Object {
	typedef Event<void(Parameters&)>					 ON(Parameters);
	typedef Event<void(const SocketAddress& oldAddress)> ON(AddressChanged);
	typedef Event<void(Int32 error, const char* reason)> ON(Close);

	Peer(ServerAPI& api);
	virtual ~Peer();

	Writer&						writer() { return _pWriter ? *_pWriter : Writer::Null(); }

	std::set<SocketAddress>		localAddresses;

	const Parameters&			properties() const { return _properties; }
	Parameters&					properties() { return _properties; }

	void						setAddress(const SocketAddress& address);
	/*!
	Try to assign/replace the serverAddress with string address gotten of protocol. Returns false if failed. */
	void						setServerAddress(const SocketAddress& address);
	void						setServerAddress(const std::string& address);
	void						setPath(const std::string& value);
	void						setQuery(const std::string& value) { ((std::string&)Client::query).assign(value); }
	
	UInt16						setPing(UInt64 value);
	UInt16						ping() const { return _ping; }
	Time						pingTime;
	UInt32						rto() const { return _rto; }

	Time						recvTime() const { return _pNetStats ? _pNetStats->recvTime() : 0; }
	UInt64						recvByteRate() const { return _pNetStats ? _pNetStats->recvByteRate() : 0; }
	double						recvLostRate() const { return _pNetStats ? _pNetStats->recvLostRate() : 0; }

	Time						sendTime() const { return _pNetStats ? _pNetStats->sendTime() : 0; }
	UInt64						sendByteRate() const { return _pNetStats ? _pNetStats->sendByteRate() : 0; }
	double						sendLostRate() const { return _pNetStats ? _pNetStats->sendLostRate() : 0; }

	bool						congested(UInt32 duration = Net::RTO_INIT) { return  _pNetStats ? _congestion(_pNetStats->queueing(), duration) : false;  }
	UInt64						queueing() const { return _pNetStats ? _pNetStats->queueing() : 0; }

	
	ICE&	ice(const Peer& peer);


	void	unsubscribeGroups(const std::function<void(const Group& group)>& forEach=nullptr);
	Group&  joinGroup(const UInt8* id, Writer* pWriter);
	void    unjoinGroup(Group& group);

	// events
	void onConnection(Exception& ex, Writer& writer, Net::Stats& netStats, DataReader& parameters) { onConnection(ex, writer, netStats, parameters, DataWriter::Null()); }
	void onConnection(Exception& ex, Writer& writer, Net::Stats& netStats, DataReader& parameters, DataWriter& response);
	void onDisconnection();

	void onRendezVousUnknown(const UInt8* peerId, std::set<SocketAddress>& addresses);
	void onHandshake(UInt8 attempts, std::set<SocketAddress>& addresses);

	bool onInvocation(Exception& ex, const std::string& name, DataReader& reader, UInt8 responseType = 0);
	/// \brief call the onRead lua function ang get result in properties
	/// \param filePath : relative path to the file (important : the directory will be erase)
	/// \param parameters : gives parameters to the function onRead()
	/// \param properties : recieve output parameters returned by onRead()
	bool onRead(Exception& ex, Path& file, DataReader& arguments, DataWriter& properties) { return onFileAccess(ex, File::MODE_READ, file, arguments, properties); }
	bool onWrite(Exception& ex, Path& file, DataReader& arguments, DataWriter& properties) { return onFileAccess(ex, File::MODE_WRITE, file, arguments, properties); }

private:
	bool onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties);

	void onJoinGroup(Group& group);
	void onUnjoinGroup(Group& group,bool dummy);
	bool exchangeMemberId(Group& group,Peer& peer,Writer* pWriter);

	ServerAPI&						_api;
	std::map<Group*,Writer*>		_groups;
	std::map<const Peer*,ICE*>		_ices;
	Parameters						_properties;

	Net::Stats*						_pNetStats;
	Congestion						_congestion;
	Writer*							_pWriter;

	UInt16							_ping;
	UInt32							_rto;
	double							_rttvar;
};

} // namespace Mona
