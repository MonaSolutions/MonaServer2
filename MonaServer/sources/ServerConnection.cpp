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

#include "ServerConnection.h"
#include "Mona/Util.h"
#include "Mona/Logs.h"


using namespace std;
using namespace Mona;


ServerConnection::ServerConnection(const SocketAddress& address, SocketEngine& engine,const char* query) : address(address), _pClient(new TCPClient(engine)), _connected(false), isTarget(true) {
	// Target version
	if (query)
		Util::UnpackQuery(query,_properties);
	init();
}

ServerConnection::ServerConnection(const shared<Socket>& pSocket, SocketEngine& engine) : address(pSocket->address()), _pClient(new TCPClient(engine)), _connected(false), isTarget(false) {
	// Initiator version
	init();
	_pClient->connect(_ex, pSocket); // if ex, the connect "hello" will fail, and report the error!
}

void ServerConnection::init() {
	_pClient->onError = [this](const Exception& ex) {
		if (_ex) // display previous error!
			WARN(this->address, " server, ", (_ex = ex));
		_ex = ex;
	};
	_pClient->onDisconnection = [this](const SocketAddress&) {
		_sendRefs.clear();
		_recvRefs.clear();
		if (_connected)
			_connected = false;
		MapParameters::clear();
		onDisconnection(_ex); // in last because can delete this
	};
	_pClient->onData = [this](const Time& time, Packet& buffer)->UInt32 {

		UInt32 size(buffer.size());
		if (size < 4)
			return size;

		BinaryReader reader(buffer.data(), buffer.size());
		size = reader.read32();
		if (reader.available() < size)
			return reader.position();
		reader.shrink(size);

		DUMP("SERVERS", reader.data(), reader.size(), "From ", address, " server");

		UInt8 nameSize = reader.read8();
		if (!nameSize) {
			UInt32 ref = reader.read7BitEncoded();
			if (!ref) {
				// CONNECTION/DISCONNECTION REQUEST
				bool connection(reader.readBool());
				if (!connection) {
					/// disconnection with error
					reader.readString(_ex.set<Ex::Protocol>());
					_pClient->disconnect();
					return 0; // this deleted, return immediatly!
				}
				/// properties itself
				MapParameters::clear();
				for (auto& it : _properties)
					setString(it.first, it.second);

				/// configs
				while (reader.available()) {
					string key, value;
					reader.readString(key);
					setString(key, reader.readString(value));
				}
				_connected = true;
				onConnection();
			} else {
				// Ref
				const auto& it(_recvRefs.find(ref));
				if (it == _recvRefs.end())
					ERROR("Impossible to find the ", ref, " message reference for the server ", address)
				else
					onMessage(it->second, reader);
			}
		} else {
			reader.read(nameSize, _recvRefs[_recvRefs.size() + 1]);
			onMessage(_recvRefs.rbegin()->second, reader);
		}

		return _pClient->onData(time, buffer += reader.size());
	};
}

ServerConnection::~ServerConnection() {
	_pClient->onData = nullptr;
	_pClient->onDisconnection = nullptr;
	_pClient->onError = nullptr;
}


void ServerConnection::connect(const Parameters& parameters) {
	if (isTarget) {
		if (_pClient->connected())
			return;
		INFO("Attempt to join ", address, " server");
		_ex = NULL;
		Exception ex;
		bool success;
		AUTO_ERROR(success = _pClient->connect(ex, address), "ServerConnection to ", address, ", ");
		if (!_pClient->connected())
			return;
	}
	// Send connection message
	ServerMessage message("");
	message->writeBool(true);
	Parameters::ForEach forEach([&message](const string& key, const string& value) {
		message->writeString(key); /// config name
		message->writeString(value); /// config value
	});
	parameters.iterate(forEach); /// configs
	_properties.iterate(forEach); /// properties
	send(message);
}

void ServerConnection::disconnect(const char* error) {
	if (!_pClient->connected())
		return;
	if (error) {
		ServerMessage message("");
		message->writeBool(false);
		message->writeString(error);
		send(message);
	}
	_pClient->disconnect();
}


void ServerConnection::send(ServerMessage& message) {
	// HEADER FORMAT
	// 4 size bytes, 1 byte for the handler size
	// handler>0 => handler bytes, the rest is the playload
	// handler==0 => Index handler 7BitEncoded bytes
	// Index handler>0 => the rest is the playload
	// Index handler==0 => HELLO MESSAGE => String host, 1 byte indication on ports count, String protocol name, UInt16 port, parameters (String key, String value)


	size_t sizeName(strlen(message.name));
	if (sizeName>255) {
		// display an error with the 32 first bytes of handler, and no send the message
		ERROR("ServerMessage name ", string(message.name,32), " must be inferior to 255 char (maximum acceptable size)")
		return;
	}

	// Search handler!
	UInt32 handlerRef(0);
	bool   writeRef(true);

	if(strlen(message.name)) {
		auto& it = _sendRefs.emplace(message.name, _sendRefs.size() + 1);
		if(it.second)
			writeRef = false;
		handlerRef = it.second;
	}

	sizeName = 1 + (writeRef ? Binary::Get7BitValueSize(handlerRef) : UInt8(sizeName));
	message->clip(256 - sizeName);

	BinaryWriter writer(BIN message->data(), sizeName);
	writer.write32(sizeName + message->size()).write8(UInt8(sizeName));
	if (writeRef)
		writer.write7BitEncoded(handlerRef);
	else
		writer.write(message.name);
	DUMP("SERVERS ", message->data(), message->size(), "To ", address," server");
	_pClient->send(message.buffer());
}

bool ServerConnection::protocolAddress(Exception& ex, const string& protocol,SocketAddress& socketAddress) {
	if (!getBoolean<false>(protocol)) {
		ex.set<Ex::Application::Config>("Server ", address, " has ", protocol, " disabled");
		return false;
	}
	string buffer;
	if (getString(String::Assign(buffer, protocol, ".publicHost"), buffer)
		socketAddress.host().set(buffer);
	else
		socketAddress.host().set(address.host());
	UInt16 port = getNumber<UInt16>(String::Assign(buffer, protocol, ".publicPort"));
	if(!port)
		port = getNumber<UInt16>(String::Assign(buffer, protocol, ".port"));
	if(!port) {
		ex.set<Ex::?>("Server ", address, " has ", protocol, " port undefined");
		return false;
	}
	socketAddress.setPort(port);
	return true;
}
