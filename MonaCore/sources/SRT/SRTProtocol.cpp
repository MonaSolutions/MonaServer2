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
#include "Mona/DataReader.h"
#include "Mona/MapWriter.h"
#include "Mona/MediaSocket.h"

using namespace std;

#if defined(SRT_API)

namespace Mona {

struct SRTIdReader : DataReader, virtual Object {
	SRTIdReader(const UInt8* data, UInt32 size) : DataReader(data, size), _header(true) {}

private:
	UInt8 followingType() { return reader.available() ? STRING : END; }

	bool readOne(UInt8 type, DataWriter& writer) {
		if (!reader.available())
			return false;

		if (_header) {
			string value;
			if (reader.available() < 4 || String::ICompare(reader.read(4, value), "#!::") != 0) {
				DEBUG("Unexpected header for the SRT streamid : ", value);
				return false;
			}
			_header = false;
		}

		// Read one pair key=value
		char* cur = STR reader.current();
		const char* end = cur + reader.available();

		const char* key = cur;
		while (*cur != '=') {
			if (cur++ == end)
				return false; // line without value
		}
		const char* separator = cur++;
		const char* value = cur;

		while (*cur != ',') {
			if (cur == end)
				break;
			++cur;
		}

		String::Scoped scoped(separator);
		writer.writeStringProperty(key, value, cur - value);

		// read/write all
		reader.next(cur - STR reader.current() + (*cur == ','));
		return true;
	}

	bool _header;
};

SRTProtocol::SRTProtocol(const char* name, ServerAPI& api, Sessions& sessions) : _server(api.ioSocket), Protocol(name, api, sessions) {
	setNumber("port", 9710);
	setNumber("timeout", 60); // 60 seconds

	_server.onConnection = [&](const shared<Socket>& pSocket) {

		// Try to read the parameter "streamid"
		string stream = ((SRT::Socket*)pSocket.get())->stream.c_str();
		if (stream.empty()) {
			WARN("SRT Connection without streamid, refused");
			return;
		}

		SRTIdReader reader(BIN stream.data(), stream.size());
		map<string, string> streamParameters;
		MapWriter<map<string, string>> mapWriter(streamParameters);
		reader.read(mapWriter);

		// Extract the application path and stream name
		auto it = streamParameters.find("r");
		Path path(it == streamParameters.end()? stream : it->second); // If no Resource Name we consider the whole SRT streamid as the stream name

		// Create the Session & the Peer
		SRTSession& session = this->sessions.create<SRTSession>(*this, pSocket);

		// Start Play / Publish
		Exception ex;
		Subscription* pSubscription(NULL);
		Publication* pPublication(NULL);
		shared<MediaSocket::Reader> pReader;
		shared<MediaSocket::Writer> pWriter;
		auto itMode = streamParameters.find("m");
		// Note: Bidirectional is not supported for now as a socket cannot be subcribed twice
		if (itMode == streamParameters.end() || String::ICompare(itMode->second, "request") == 0 /*|| String::ICompare(itMode->second, "bidirectional") == 0*/) {
			pWriter.set(MediaStream::TYPE_SRT, path, MediaWriter::New("ts"), pSocket, api.ioSocket);
			if (!api.subscribe(ex, session.peer, path.baseName(), *(pSubscription = new Subscription(*pWriter)))) {
				session.kill();
				return;
			}
		}
		if (itMode != streamParameters.end() && (String::ICompare(itMode->second, "publish") == 0 /*|| String::ICompare(itMode->second, "bidirectional") == 0*/)) {
			if ((pPublication = api.publish(ex, session.peer, path.baseName())) == NULL) {
				session.kill();
				return;
			}
			pReader.set(MediaStream::TYPE_SRT, path, *pPublication, MediaReader::New("ts"), pSocket, api.ioSocket);
		}
		
		if (!pSubscription && !pPublication) {
			WARN("Unhandled SRT mode : ", itMode->second);
			session.kill();
			return;
		}
		
		// Start the session
		session.init(pSubscription, pPublication, pReader, pWriter);
	};
	_server.onError = [this](const Exception& ex) {
		WARN("Protocol ", this->name, ", ", ex); // onError by default!
	};
}

bool SRTProtocol::load(Exception& ex) {
	if (!Protocol::load(ex))
		return false;
	return _server.start(ex, address);
}

} // namespace Mona

#endif
