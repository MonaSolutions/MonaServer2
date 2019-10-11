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

#pragma once

#include "Mona/SRT.h"
#include "Mona/Protocol.h"
#include "Mona/Peer.h"

#if defined(SRT_API)
namespace Mona {

struct SRTProtocol : Protocol, virtual Object {
	SRTProtocol(const char* name, ServerAPI& api, Sessions& sessions);
	virtual ~SRTProtocol() {}

	bool load(Exception& ex);

	struct Params : private DataReader, virtual Object {
		NULLABLE
		Params(const std::string& streamId, Peer& peer) : DataReader(BIN streamId.data(), streamId.size()), _peer(peer), _publish(false), _type(OTHER), _done(false), _subscribe(true) {
			read(DataWriter::Null()); // fill parameters!
		}
		operator bool() const { return _done; }

		const std::string&	stream() const { return _stream; }
		bool				publish() const { return _publish; }
		bool				subscribe() const { return _subscribe; }

		DataReader& operator()();

	private:
		UInt8 followingType() { return _type; }

		bool readOne(UInt8 type, DataWriter& writer);
		void write(DataWriter& writer, const char* key, const char* value, UInt32 size);

		bool setResource(const char* value, UInt32 size);

		std::string	_stream;
		Peer&		_peer;
		bool		_publish;
		bool		_subscribe;
		bool		_done;
		UInt8		_type;
	};

protected:
	SRT::Server	_server;
};

} // namespace Mona
#endif
