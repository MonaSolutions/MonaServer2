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
#include "Mona/ServerAPI.h"


namespace Mona {


struct Peer : Client, virtual Object {
	typedef Event<void(Parameters&)>					 ON(Parameters);
	typedef Event<void(const SocketAddress& oldAddress)> ON(AddressChanged);
	typedef Event<void(Int32 error, const char* reason)> ON(Close);

	Peer(ServerAPI& api, const char* protocol, const SocketAddress& address);
	~Peer();

	Parameters&					properties() { return (Parameters&)Client::properties(); }
	const char*					authentification() const override;

	void						setAddress(const SocketAddress& address);
	/*!
	Try to assign/replace the serverAddress with string address gotten of protocol. Returns false if failed. */
	bool						setServerAddress(const SocketAddress& address);
	bool						setServerAddress(const std::string& address);
	void						setPath(const Path& path) { (Path&)Client::path = path; }
	void						setQuery(std::string&& value) { (std::string&)Client::query = std::move(value); }
	
	UInt16						setPing(UInt64 value) { pingTime.update(); return Client::setPing(value); }
	Time						pingTime;

	// events
	/*!
	Peer app connection, parameters will contains peer.query (useless to pass it if just it) */
	void onConnection(Exception& ex, Writer& writer, Net::Stats& netStats, DataReader& parameters = DataReader::Null(), DataWriter& response = DataWriter::Null());
	void onDisconnection();

	SocketAddress& onHandshake(SocketAddress& redirection);

	bool onInvocation(Exception& ex, const std::string& name, DataReader& reader, UInt8 responseType = 0);
	/*!
	Read access, arguments are the request arguments, and properties are the "<%%> pattern" to replace in read file */
	bool onRead(Exception& ex, Path& file, DataReader& arguments, DataWriter& properties) { return onFileAccess(ex, File::MODE_READ, file, arguments, properties); }
	/*!
	Write access, arguments are the request arguments, and properties are the "<%%> pattern" to replace in write file */
	bool onWrite(Exception& ex, Path& file, DataReader& arguments, DataWriter& properties) { return onFileAccess(ex, File::MODE_WRITE, file, arguments, properties); }
	bool onDelete(Exception& ex, Path& file, DataReader& arguments) { return onFileAccess(ex, File::MODE_DELETE, file, arguments, DataWriter::Null()); }

private:
	bool onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties);

	ServerAPI&			_api;
	mutable std::string	_auth;
	mutable UInt32		_authVersion;
};

} // namespace Mona
