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

#include "Mona/Server.h"
#include "Mona/BufferPool.h"
#include "Mona/MediaLogs.h"

using namespace std;


namespace Mona {


Server::Server(UInt16 cores) : Thread("Server"), ServerAPI(_www, _publications, _handler, _protocols, _timer, cores), _protocols(*this), _handler(wakeUp) {
	DEBUG(threadPool.threads(), " threads in server threadPool");
}
 
Server::~Server() {
	stop();
}

void Server::start(const Parameters& parameters) {
	if(running())
		stop();

	// copy and load parametes
	for (auto& it : parameters) {
		if (_www.empty() && String::ICompare(it.first, "www.dir") == 0)
			_www = it.second;
		setParameter(it.first, it.second);
	}
	if (_www.empty())
		_www.assign(EXPAND("www/"));
	else
		FileSystem::MakeFolder(_www);
	setString("www.dir", _www);
	Exception ex;
	AUTO_ERROR(FileSystem::CreateDirectory(ex, _www), "Application directory creation");

	Thread::start(); // start
}

Publish* Server::publish(const char* name) {
	if (running())
		return new Publish(self, name);
	ERROR("Start ", typeof(self), " before to publish ", name);
	return NULL;
}

bool Server::run(Exception&, const volatile bool& requestStop) {
	if (getBoolean<true>("poolBuffers"))
		Buffer::Allocator::Set<BufferPool>();

	Timer::OnTimer onManage;
	
#if !defined(_DEBUG)
	try
#endif
	{ // Encapsulate sessions!

		Exception ex;
		// SSL client
		AUTO_ERROR(TLS::Create(ex, pTLSClient), "SSL Client");
		// SSL server
		Path cert(getString("TLS.certificat", "cert.pem"));
		Path key(getString("TLS.key", "key.pem"));
		if (!cert.exists())
			WARN("No TLS/SSL server protocols, no ", cert.name()," file")
		else if(!key.exists())
			WARN("No TLS/SSL server protocols, no ", key.name(), " file")
		else
			AUTO_ERROR(TLS::Create(ex=nullptr, cert, key, pTLSServer), "SSL Server");
	
		UInt32 countClient(0);
		Sessions sessions;
		_protocols.start(self, sessions);
	
		onStart();

		// Start streams after onStart to get onPublish/onSubscribe permissions!
		loadIniStreams();

		onManage = ([&](UInt32) {
			sessions.manage(); // in first to detect session useless died
			_protocols.manage(); // manage custom protocol manage (resource protocols)

			// Reset subscriptions of streams target
			auto it = _streamSubscriptions.begin();
			while (it != _streamSubscriptions.end()) {
				if (!it->first.unique()) {
					if (it->second->ejected()) // pulse ejection timeout! and reset to allow a new start if need!
						it->second->reset();
					++it;
				} else { // remove subscriptions, no more usefull!
					unsubscribe(*it->second);
					it = _streamSubscriptions.erase(it);
				}
			}
			// Pulse streams!
			for (const auto& it : _iniStreams) {
				if (it.second && it.second->ejected())
					it.second->reset(); // wait next onManage to restart!
				else
					it.first->start(self);
			}
		
			this->onManage(); // client manage (script, etc..)
			if (clients.size() != countClient)
				INFO((countClient = clients.size()), " clients");
			// TODO? relayer.manage();
			return 2000;
		}); // manage every 2 seconds!
		_timer.set(onManage, 2000);

		while (!requestStop) {
			if (wakeUp.wait(_timer.raise()))
				_handler.flush();
		}

		// Sessions deletion!
	}
#if !defined(_DEBUG)
	catch (exception& ex) {
		FATAL("Server, ",ex.what());
	} catch (...) {
		FATAL("Server, unknown error");
	}
#endif
	Thread::stop(); // to set running() to false (and not more allows to handler to queue Runner)
	// Stop onManage (useless now)
	_timer.set(onManage, 0);

	// Close server sockets to stop reception
	_protocols.stop();

	// TODO? terminate relay server ((RelayServer&)relayer).stop();

	// stop event to unload children resource (before to release sockets, threads, and buffers)
	onStop();

	// stop socket sending (it waits the end of sending last session messages)
	threadPool.join();

	// finish writing file before to detach buffer allocator
	ioFile.join();

	// empty handler!
	_handler.flush();

	// clean init streams and unsubscribe subscriptions
	for (const auto& it : _iniStreams) {
		if(it.second)
			unsubscribe(*it.second);
	}
	_iniStreams.clear();
	// clean and unsubscribe streamSubscriptions
	UInt32 alives = 0;
	for (const auto& it : _streamSubscriptions) {
		if (!it.first.unique())
			++alives;
		unsubscribe(*it.second);
	}
	if (alives)
		ERROR(alives, " intern media stream target not released on stop");
	_streamSubscriptions.clear();
	// clean and unpublish streamSubscriptions
	if (!_streamPublications.empty()) {
		ERROR(_streamPublications.size(), " intern media stream source not released on stop");
		_streamPublications.clear();
	}
	// release publications remaining (can happen when using Publish extern way)
	_publications.clear();

	// release memory
	INFO("Server memory release...");
	Buffer::Allocator::Set();

	_www.clear();
	NOTE("Server stopped");
	return true;
}

void Server::loadIniStreams() {
	// Load Streams configs
	struct Description : String {
		Description(const string& publication, string&& description, bool isSource) : isSource(isSource), publication(publication), String(move(description)) {}
		const string& publication;
		const bool    isSource;
	};
	deque<Description> descriptions;
	string temp;
	vector<const char*> keyToRemove;
	for (auto& it : self) {
		// [MyPublication=Publication]
		if (it.first.find('.') != string::npos || String::ICompare(it.second,"publication") != 0)
			continue;

		if (String::ICompare(it.first, "logs") == 0)
			descriptions.emplace_back(it.first, "!logs", true);
	
		for (auto& it2 : range(temp.assign(it.first) += '.')) {
			const char* name(it2.first.c_str() + temp.size());
			bool isSource = String::ICompare(name, EXPAND("in ")) == 0;
			if (!isSource && String::ICompare(name, EXPAND("out ")) != 0)
				continue; // no stream line!
			name += isSource ? 3 : 4;
			if(!it2.second.empty())
				descriptions.emplace_back(it.first, String(name,'=',it2.second), isSource); // recompose the line!
			else
				descriptions.emplace_back(it.first, name, isSource);
			keyToRemove.emplace_back(it2.first.c_str());
		}
	}
	// Erase key which was streams to remove it of metadata publication (could be a security issue with host/port etc..)
	for (const char* key : keyToRemove)
		erase(key);

	// Create streams
	for (const Description& description : descriptions) {
		shared<MediaStream> pStream = stream(description.publication, description, description.isSource);
		if (!pStream)
			continue;
		pStream->start(self);
		if (pStream->type == MediaStream::TYPE_LOGS && !pStream->state())
			continue; // useless, can't work!
		// move stream target from _streamSubscriptions to _iniStreams to avoid double iteration on onManage!
		const auto& itTarget = _streamSubscriptions.find(dynamic_pointer_cast<Media::Target>(pStream));
		if (itTarget != _streamSubscriptions.end()) {
			_iniStreams.emplace(move(pStream), move(itTarget->second));
			_streamSubscriptions.erase(itTarget);
		} else
			_iniStreams.emplace(move(pStream), nullptr);
	}
}

shared<MediaStream> Server::stream(const string& publication, const string& description, bool isSource) {
	shared<MediaStream> pStream;
	if(isSource) { // is source
		// PUBLISH, keep publication opened!
		Exception ex;
		const auto& it = _streamPublications.lower_bound(publication.c_str());
		Publication* pPublication = it == _streamPublications.end() || publication.compare(it->first) != 0 ? publish(ex, publication) : it->second;
		if (!pPublication)
			return nullptr; // logs already displaid by publish call
		pStream = stream(*pPublication, description);
		if (!pStream) {
			if (it == _streamPublications.end())
				unpublish(*pPublication);
			return nullptr;
		}
		pStream->onDelete = [this, pPublication]() {
			auto it = _streamPublications.lower_bound(pPublication->name().c_str());
			if (it != _streamPublications.end() && pPublication->name().compare(it->first) == 0) {
				it = _streamPublications.erase(it);
				if (it == _streamPublications.end() || pPublication->name().compare(it->first) != 0)
					unpublish(*pPublication);
			}
		};
		_streamPublications.emplace_hint(it, pPublication->name().c_str(), pPublication);
	} else if (!(pStream = stream(description))) // is Target
		return nullptr;
	pStream->onNewTarget = [this, publication, &stream = *pStream](const shared<Media::Target>& pTarget) {
		const auto& it = _streamSubscriptions.emplace(pTarget, new Subscription(*pTarget)).first;
		for (const auto& itParam : stream.params())
			it->second->setParameter(itParam.first, itParam.second);
		Exception ex; // logs already displaid by subscribe
		if (!subscribe(ex, publication, *it->second))
			_streamSubscriptions.erase(it);
	};
	INFO(pStream->description(), " loaded on publication ", publication);
	return pStream;
}

unique<MediaStream> Server::stream(Media::Source& source, const string& description) {
	Exception ex;
	unique<MediaStream> pStream;
	if (&source == &Media::Source::Null() || description[0] != '!') {
		AUTO_ERROR(pStream = MediaStream::New(ex, source, description, timer, ioFile, ioSocket, pTLSClient), description);
		if (!pStream)
			return nullptr;
	} else
		pStream.set<MediaLogs>(description.c_str()+1, source, api());
	if (running())
		return pStream;
	ERROR(ex.set<Ex::Intern>(pStream->description(), ", start server ", name(), " before"));
	return nullptr;
}



} // namespace Mona
