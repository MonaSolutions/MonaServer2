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
	enum : Int32 {
		// Int32 0 =>  Normal close (Client close properly)
		// Int32 Positive => Can send a last message (>=100 to avoid conflict with user code and stays compatible with a Int8 encoding)
		ERROR_UNEXPECTED = 100, // Unexpected CLIENT close, will display a error log
		ERROR_SERVER = 101, // server shutdown, always possible to send a message to the client!
		ERROR_IDLE = 102,
		ERROR_REJECTED = 103, // close requested by the client
		ERROR_PROTOCOL = 104,
		ERROR_APPLICATION = 105, // user-application error
		ERROR_UNSUPPORTED = 106,
		ERROR_UNAVAILABLE = 107,
		ERROR_UNFOUND = 108,
		ERROR_UPDATE = 109, // update of one application
		// Int32 Negative => no possible to distribute a message (<=100 to avoid conflict with user code and stays compatible with a Int8 encoding)
		ERROR_SOCKET = -100,
		ERROR_CONGESTED = -101,
		ERROR_ZOMBIE = -102 // is already died indeed, impossible to send a message!
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
