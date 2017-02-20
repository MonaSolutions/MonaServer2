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
#include "Mona/Sessions.h"

namespace Mona {

struct Server : protected ServerAPI, private Thread {
	Server(UInt16 cores=0);
	virtual ~Server();

	bool	start() { Parameters parameters;  return start(parameters); }// params by default
	bool	start(const Parameters& parameters);
	void	stop() { Thread::stop(); }
	bool	running() { return Thread::running(); }

private:
	virtual void	manage() {}

	void  loadStreams(std::multimap<std::string, Media::Stream*>& streams);
	void  startStreams(std::multimap<std::string, Media::Stream*>& streams, std::set<Publication*>& publications, std::set<Subscription*>& subscriptions);

	bool			run(Exception& ex, const volatile bool& stopping);

	Handler			_handler;
	Timer			_timer;
	Protocols		_protocols;
	Sessions		_sessions;
	Path			_application;
	Path			_www;
};



} // namespace Mona
