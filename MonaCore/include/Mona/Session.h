/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or 
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).
*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Peer.h"
#include "Mona/Protocol.h"
#include "Mona/Logs.h"

namespace Mona {

#define TO_ERROR(EX) Session::ToError(EX), EX.c_str()

class Session : public virtual Object {
	shared<Peer> _pPeer; // build it in first to initialize peer&!
public:
	enum {
		// Int32 0 =>  Normal close (Client close properly)
		// Int32 Positive => Can send a last message
		ERROR_UNEXPECTED = 1, // Unexpected CLIENT close, will display a error log
		ERROR_SERVER = 2, 
		ERROR_IDLE = 3,
		ERROR_REJECTED = 4, // close requested by the client
		ERROR_PROTOCOL = 5,
		ERROR_UNSUPPORTED = 6,
		ERROR_UNAVAILABLE = 7,
		ERROR_UNFOUND = 8,
		ERROR_UPDATE = 9, // update of one application
		// Int32 Negative => no possible to distribute a message
		ERROR_SOCKET = -1,
		ERROR_SHUTDOWN = -1, // no more message exchange acceptation!
		ERROR_CONGESTED = -2,
		ERROR_ZOMBIE = -3 // is already died indeed, impossible to send a message!
	};

	static Int32 ToError(const Exception& ex);

	~Session();

	/*!
	If id>0 => managed by _sessions! */
	UInt32				id() const   { return _id; }
	const std::string&	name()	const;
	const UInt32		timeout;

	/*!
	Always implements kill and release resource here rather destructor */
	virtual void		kill(Int32 error = 0, const char* reason = NULL);

	template<typename ProtocolType = Protocol>
	ProtocolType& protocol() { return (ProtocolType&)_protocol; }

	Peer&				peer;
	ServerAPI&			api;

	operator const UInt8*() const { return peer.id; }
	
	virtual void onParameters(const Parameters& parameters);
	/*!
	implement it to flush writers to avoid message queue exceed! */
	virtual void flush() = 0;
	/*!
	Manage every 2 seconds */
	virtual bool manage();

protected:
	const bool			died; // keep it protected because just Sessions can check if session is died to delete it!

	Session(Protocol& protocol, const SocketAddress& address, const char* name=NULL);
	Session(Protocol& protocol, shared<Peer>& pPeer, const char* name = NULL);
	/*!
	Session morphing */
	Session(Protocol& protocol, Session& session);
	
	// to fix morphing issue with kill intern call!
	void close(Int32 error = 0, const char* reason = NULL) { peer.onClose(error, reason); } 
private:
	void init(Session& session);

	UInt32						_id;
	mutable std::string			_name;
	SESSION_OPTIONS				_sessionsOptions;
	Protocol&					_protocol;
	UInt32						_timeout;
	Congestion					_congestion;


	friend struct Sessions;
};


} // namespace Mona
