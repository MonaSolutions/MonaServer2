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

#include "Mona/WS/WSClient.h"
#include "Mona/ByteReader.h"
#include "Mona/URL.h"

using namespace std;

namespace Mona {

WSClient::WSClient(IOSocket& io, const char* name) :TCPClient(io), binaryData(false),
	Client("WS", SocketAddress::Wildcard()), _writer(self, name ? name : "WSClient") {
}
WSClient::WSClient(IOSocket& io, const shared<TLS>& pTLS, const char* name) :TCPClient(io, pTLS), binaryData(false),
	Client(pTLS ? "WSS" : "WS", SocketAddress::Wildcard()), _writer(self, name ? name : (pTLS ? "WSSClient" : "WSClient")) {
}

UInt16 WSClient::ping() {
	if(connection) {
		_writer.writePing(connection);
		_writer.flush();
	}
	return Client::ping();
}

bool WSClient::send(Exception& ex, const Packet& packet, int flags) {
	if (!connection) {
		Socket::SetException(NET_ENOTCONN, ex);
		return false;
	}
	TCPClient::send<WSSender>(socket(), WS::Type(flags ? flags : (binaryData ? WS::TYPE_BINARY : WS::TYPE_TEXT)), packet, _writer.name());
	return true;
}

void WSClient::disconnect() {
	if (!connection)
		return;
	// stop reception
	setWriter();
	_onResponse = nullptr;
	_onMessage = nullptr;
	_pHTTPHeader = nullptr;
	_url.clear();
	(SocketAddress&)this->address = nullptr;
	(SocketAddress&)this->serverAddress = nullptr;
	disconnect(); // disconnect can delete this!
}

bool WSClient::connect(Exception& ex, const SocketAddress& addr, const string& request) {
	(string&)query = URL::ParseRequest(request, (Path&)path, REQUEST_MAKE_FOLDER | REQUEST_FORCE_RELATIVE);
	SocketAddress address(addr);
	if (!address.port())
		address.setPort(80);
	if (!TCPClient::connect(ex, address))
		return false;
	if (connection)
		return true; // already send!

	String::Append(_url, self->isSecure() ? "wss://" : "ws://", self->peerAddress(), request);
	(SocketAddress&)this->address = socket()->address();
	(SocketAddress&)this->serverAddress = address;
	setWriter(_writer, *socket());

	_onResponse = [this](HTTP::Response& response) {
		if (response.ex) {
			onError(response.ex);
			return disconnect();
		}
		if (!response || !response->pWSDecoder) {
			Exception ex;
			ex.set<Ex::Protocol>("Connection ", serverAddress, " failed");
			onError(ex);
			return disconnect();
		}
		_pHTTPHeader = response;
		_pHTTPHeader->pWSDecoder->onMessage = _onMessage;
		setPing(connection.elapsed()); // set the ping immediatly with the first response!
	};
	_onMessage = [this](WS::Message& message) {
		switch (message.type) {
			case WS::TYPE_BINARY:
				binaryData = true;
				break;
			case WS::TYPE_TEXT:
				binaryData = false;
				break;
			case WS::TYPE_CLOSE: {
				BinaryReader reader(message.data(), message.size());
				UInt16 code(reader.read16());
				string error(STR reader.current(), reader.available());
				Exception ex;
				switch (code) {
					case WS::CODE_ENDPOINT_GOING_AWAY: // client is dying
						ex.set<Ex::Net::Socket>(error);
						break;
					case WS::CODE_POLICY_VIOLATION: // no permission
						ex.set<Ex::Permission>(error);
						break;
					case WS::CODE_PROTOCOL_ERROR:
					case WS::CODE_PAYLOAD_NOT_ACCEPTABLE:
					case WS::CODE_MALFORMED_PAYLOAD:
					case WS::CODE_PAYLOAD_TOO_BIG:
						ex.set<Ex::Protocol>(error);
						break;
					case WS::CODE_EXTENSION_REQUIRED: // Unsupported
						ex.set<Ex::Unsupported>(error);
						break;
					case 0: // no error code
					case WS::CODE_NORMAL_CLOSE: // normal error
						if (error.empty())
							break;
					case WS::CODE_RESERVED_ABNORMAL_CLOSE:
					default:; // user code
						ex.set<Ex::Application>(error);
						break;
				}
				if (ex)
					onError(ex);
				disconnect();
				return;
			}
			case WS::TYPE_PING:
				_writer.writePong(message);
				_writer.flush();
				return;
			case WS::TYPE_PONG: {
				UInt32 elapsed0(BinaryReader(message.data(), message.size()).read32());
				UInt32 elapsed1 = UInt32(connection.elapsed());
				if (elapsed1>elapsed0)
					setPing(elapsed1 - elapsed0);
				return;
			}
			default: {
				Exception ex;
				ex.set<Ex::Protocol>(String::Format<UInt8>(" request type %#x unknown", message.type));
				onError(ex);
				return;
			}
		}
		// BINARY or TEXT
		Media::Data::Type type = binaryData ? Media::Data::TYPE_UNKNOWN : Media::Data::TYPE_JSON;
		unique<DataReader> pReader = Media::Data::NewReader<ByteReader>(type, message); // Use NewReader to check JSON validity
		onMessage(*pReader);
		if (message.flush)
			_writer.flush();
	};

	shared<Buffer> pHeader(SET);
	String::Assign(*pHeader, "GET ", _url, " HTTP/1.1\r\n");
	String::Append(*pHeader, "Host: ", address, "\r\n");
	String::Append(*pHeader, "Connection: Upgrade\r\n");
	String::Append(*pHeader, "Upgrade: websocket\r\n");
	//String::Append(*pHeader, "Origin: http://", address, "\r\n");
	String::Append(*pHeader, "Sec-WebSocket-Version: 13\r\n");
	String::Append(*pHeader, "Sec-WebSocket-Key: ");
	String::Append(Util::ToBase64(id, 16, *pHeader, true), "\r\n\r\n");
	DUMP_RESPONSE(_writer.name(), pHeader->data(), pHeader->size(), address);
	return socket()->write(ex, Packet(pHeader))>=0;
}

Socket::Decoder* WSClient::newDecoder() {
	HTTPDecoder* pDecoder = new HTTPDecoder(io.handler, String::Empty(), _writer.name());
	pDecoder->onResponse = _onResponse;
	return pDecoder;
}



}
