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
#include "Mona/Sessions.h"
#include "Mona/Publish.h"

namespace Mona {

struct Server : protected ServerAPI, private Thread {
	struct Action : Runner {
		Action(const char* name, ServerAPI& api) : Runner(name), _api(api) {}
	private:
		virtual void run(ServerAPI& api) = 0;
		bool run(Exception& ex) { run(_api); return true; }
		ServerAPI& _api;
	};

	Server(UInt16 cores=0);
	virtual ~Server();

	Parameters& start() { Parameters parameters;  return start(parameters); }// params by default
	Parameters& start(const Parameters& parameters);
	void stop() { Thread::stop(); }
	bool running() { return Thread::running(); }

	template<typename ActionType>
	bool queue(const shared<ActionType>& pAction) { return ServerAPI::queue(pAction); }

	/*!
	Publish a publication, stays valid until !*Publish */
	Publish* publish(const char* name);
	/*!
	Create a media stream target */
	unique<MediaStream> stream(const std::string& description) { return stream(Media::Source::Null(), description); }
	/*!
	Create a media stream source
	Trick: use '!' in description to create a "logs" publication */
	unique<MediaStream> stream(Media::Source& source, const std::string& description);
	

protected:
	/*!
	Create a media stream source or target automatically linked to a publication,
	Trick: use '!' in description to create a "logs" publication */
	shared<MediaStream> stream(const std::string& publication, const std::string& description, bool isSource = false);

	template<typename  ...Args>
	Publication* publish(Exception& ex, Args&&... args) { return ServerAPI::publish(ex, args ...); }

	ServerAPI& api() { return self; }	
private:
	virtual void onStart() {}
	virtual void onManage() {}
	virtual void onStop() {}

	void  loadIniStreams();

	bool			run(Exception& ex, const volatile bool& requestStop);

	Handler				_handler;
	Timer				_timer;
	Protocols			_protocols;
	std::string			_www;

	std::map<shared<MediaStream>, unique<Subscription>>				_iniStreams;
	std::multimap<const char*, Publication*, String::Comparator>	_streamPublications; // contains publications initiated by autoStreams
	std::map<shared<Media::Target>, unique<Subscription>>			_streamSubscriptions; // contains susbscriptions created by auto streams target
	std::map<std::string, Publication>								_publications;
};



} // namespace Mona
