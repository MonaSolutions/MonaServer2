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
#include "Mona/QueryReader.h"
#include "Mona/HTTP/HTTPWriter.h"
#include "Mona/HTTP/HTTPDecoder.h"

namespace Mona {

struct HTTPSession : virtual Object, TCPSession {
	HTTPSession(Protocol& protocol);

private:
	shared<Socket::Decoder> newDecoder();
	HTTPDecoder::OnRequest	_onRequest;

	void			onParameters(const Parameters& parameters);
	bool			manage();
	void			flush();

	void			close(); // usefull for Protocol upgrade like WebSocket upgrade
	void			kill(Int32 error=0, const char* reason = NULL);

	void			openSubscribtion(Exception& ex, const std::string& stream, Writer& writer);
	void			closeSusbcription();

	void			openPublication(Exception& ex, const Path& stream);
	void			closePublication();

	/// \brief Send the Option response
	/// Note: It is called when processMove is used before a SOAP request
	void			processOptions(Exception& ex, const HTTP::Header& request);

	/// \brief Process GET & HEAD commands
	/// Search for a method or a file whitch correspond to the _filePath
	void			processGet(Exception& ex, HTTP::Request& request, QueryReader& parameters);
	void			processPost(Exception& ex, HTTP::Request& request);
	void			processPut(Exception& ex, HTTP::Request& request);

	HTTPWriter			_writer;
	Subscription*		_pSubscription;
	Publication*		_pPublication;

	Session*			_pUpgradeSession;

	// options
	std::string			_index;
	bool				_indexDirectory;
};


} // namespace Mona
