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
#include "Mona/DataReader.h"

#if defined(SRT_API)
namespace Mona {

struct SRTProtocol : Protocol, virtual Object {
	SRTProtocol(const char* name, ServerAPI& api, Sessions& sessions);
	virtual ~SRTProtocol() {}

	bool load(Exception& ex);

	struct Params : private DataReader, virtual Object {
		NULLABLE
		Params(const SRT::Socket& socket) : DataReader(BIN socket.stream().data(), socket.stream().size()), _publish(false), _done(false), _subscribe(false) {
			read(DataWriter::Null()); // fill parameters!
		}
		operator bool() const { return _publish || _subscribe; }

		const std::string&	stream() const { return _path.name(); }
		const std::string&	path() const { return _path.parent(); }
		bool				publish() const { return _publish; }
		bool				subscribe() const { return _subscribe; }

		DataReader& operator()();

	private:
		UInt8 followingType() { return _done ? END : OTHER; }

		bool readOne(UInt8 type, DataWriter& writer);
		void write(DataWriter& writer, const char* key, const char* value, UInt32 size);

		Path	_path;
		bool	_publish;
		bool	_subscribe;
		bool	_done;
	};

protected:
	SRT::Server	_server;
};

} // namespace Mona
#endif
