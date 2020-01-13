/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/
#include "Mona/SRT/SRTProtocol.h"
#include "Mona/SRT/SRTSession.h"
#include "Mona/URL.h"

using namespace std;

#if defined(SRT_API)

namespace Mona {

DataReader& SRTProtocol::Params::operator()() {
	_type = OTHER;
	DataReader::reset();
	return self;
}

bool SRTProtocol::Params::setResource(const char* value, size_t size) {
	Path path;
	const char* query = URL::ParseRequest(value, size, path, REQUEST_FORCE_RELATIVE);
	if (path.isFolder()) {
		_stream.clear();
		return false;
	}
	_stream = path.name();
	_peer.setPath(path.parent());
	_peer.setQuery(string(query, size));
	return true;
}

bool SRTProtocol::Params::readOne(UInt8 type, DataWriter& writer) {
	_type = END;
	// TODO: Handle nested keys? => #!:{...}
	if (reader.available() < 4 || String::ICompare(STR reader.current(), EXPAND("#!::")) != 0) {
		// direct resource!
		if (!(_ok=setResource(STR reader.current(), reader.available())))
			DEBUG("Invalid SRT streamid ", String::Data(reader.current(), 4));
		return _ok;
	}
	reader.next(4);

	writer.beginObject();
	// Read pair key=value
	const char* key = NULL;
	const char* value = NULL;
	do {
		const char* cur = STR reader.current();
		if (!reader.available() || *cur == ',') {
			if (!key)
				break; // nothing to read!
			if (!value)
				value = cur;
			const char* end = value;
			while (end > key && isblank(*(end - 1)))
				--end;
			String::Scoped scoped(end);
			// trim left the value
			while (isblank(*++value));
			// trim right the value
			end = cur;
			while (end > value && isblank(*(end - 1)))
				--end;
			write(writer, key, value, end - value);
			key = value = NULL;
		} else if (*cur == '=') {
			if (!key)
				key = ""; // empty key!
			if(!value)
				value = cur;
		} else if (!key && !isblank(*cur))
			key = cur;
	} while (reader.next());
	writer.endObject();
	return _ok = true;
}

void SRTProtocol::Params::write(DataWriter& writer, const char* key, const char* value, UInt32 size) {
	writer.writeProperty(key, value, size);
	if (_ok)
		return; // else already done!
	if (String::ICompare(key, "m") == 0) {
		if(String::ICompare(value, size, "request") == 0)
			_subscribe = true;
		else if (String::ICompare(value, size, "publish") == 0)
			_publish = true;
		else if (String::ICompare(value, size, "bidirectional") == 0)
			_publish = _subscribe = true;
	} else if (String::ICompare(key, "r") == 0)
		setResource(value, size);
}


SRTProtocol::SRTProtocol(const char* name, ServerAPI& api, Sessions& sessions) : _server(api.ioSocket), Protocol(name, api, sessions) {
	setNumber("port", 9710);
	// SRT has no need of timeout by default even if it can open multiple socket, 
	// indeed we can trust in SRT lib which checks already zombie socket:
	// there is an intern ping, we can see it with SRT::Socket::getStats recvTime/sendTime

	_server.onConnection = [this](const shared<Socket>& pSocket) {
		// Try to read the parameter "streamid"
		shared<Peer> pPeer(SET, this->api, this->name, pSocket->peerAddress());
		Params params(((SRT::Socket&)*pSocket).streamId(), *pPeer);
		if (params) {
			if(params.stream().empty()) // raise just in the case where streamid=#!:: (without any resource)
				ERROR("SRT connection with streamid has to specify a valid resource (r parameter)")
			else
				this->sessions.create<SRTSession>(self, pSocket, pPeer).init(params);
		} else
			ERROR("SRT connection without a valid streamid, use ini configuration rather to configure statically SRT input and output");

	};
	_server.onError = [this](const Exception& ex) {
		WARN("Protocol ", this->name, ", ", ex); // onError by default!
	};
}

SocketAddress SRTProtocol::load(Exception& ex) {
	initSocket(*_server);
	return _server.start(ex, address) ? _server->address() : SocketAddress::Wildcard();
}

} // namespace Mona

#endif
