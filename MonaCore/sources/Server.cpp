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


Server::Server(UInt16 cores) : Thread("Server"), ServerAPI(_www, _handler, _protocols, _timer, cores), _protocols(*this), _handler(wakeUp) {
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
		if (!_www && String::ICompare(it.first, "www.dir") == 0)
			_www.set(it.second);
		setString(it.first, it.second);
	}

	if(!_www)
		_www.set("www/");

	Exception ex;
	AUTO_ERROR(FileSystem::CreateDirectory(ex, _www), "Application directory creation");

	Thread::start();
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
	set<shared<Media::Stream>>	streams;
	set<Publication*>			publications;
	set<shared<Subscription>>	subscriptions;

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
		_protocols.start(*this, sessions);
		// TODO? ((RelayServer&)relayer).start(exWarn)

		onStart();

		// Start streams after onStart to get onPublish/onSubscribe permissions!
		loadStreams(streams, publications, subscriptions);

		onManage = ([&](UInt32) {
			sessions.manage(); // in first to detect session useless died
			_protocols.manage(); // manage custom protocol manage (resource protocols)

			// reset subscription ejected
			for (const shared<Subscription>& pSubscription : subscriptions) {
				if (pSubscription->ejected())
					pSubscription->reset(); // reset ejected!
			}
			// Pulse streams!
			for (const shared<Media::Stream>& pStream : streams)
				pStream->start(self);
			manage(); // client manage (script, etc..)
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

	// delete streams after handler flush
	for (const shared<Subscription>& pSubscription : subscriptions)
		unsubscribe(*pSubscription);
	for (const shared<Media::Stream>& pStream : streams)
		pStream->stop();
	for (const shared<Media::Stream>& pStream : _streams)
		pStream->stop();
	_streams.clear();
	// delete intern publications after handler flush
	for (Publication* pPublication : publications)
		unpublish(*pPublication);

	// release memory
	INFO("Server memory release...");
	Buffer::Allocator::Set();

	NOTE("Server stopped");
	_www.reset();
	return true;
}

void Server::loadStreams(set<shared<Media::Stream>>& streams, set<Publication*>& publications, set<shared<Subscription>>& subscriptions) {
	// Load Streams configs
	multimap<string, string> lines;
	string temp;
	vector<const char*> keyToRemove;
	for (auto& it : self) {
		// [MyPublication=Publication]
		if (it.first.find('.') != string::npos || String::ICompare(it.second,"publication") != 0)
			continue;

		if (String::ICompare(it.first, "logs") == 0)
			lines.emplace(it.first, string());
	
		for (auto& it2 : range(temp.assign(it.first) += '.')) {
			const char* name(it2.first.c_str() + temp.size());
			if (!*name || !it2.second.empty())
				continue; // Stream line have no value!
			lines.emplace(it.first, name);
			keyToRemove.emplace_back(it2.first.c_str());
		}
	}
	// Erase key which was streams to remove it of metadata publication (could be a security issue with host/port etc..)
	for (const char* key : keyToRemove)
		erase(key);

	// start streams
	Exception ex;
	Publication* pLastPublication(NULL);
	for (auto& it : lines) {
		shared<Media::Stream> pStream;
		bool isTarget = it.second[0] == '@';
		if (isTarget) {
			AUTO_ERROR(pStream = Media::Stream::New(ex=nullptr, it.second.c_str()+1, timer, ioFile, ioSocket, pTLSClient), it.second);
			if (!pStream)
				continue;
			INFO(pStream->description(), " loaded on publication ", it.first);
			pStream->start(self); // Start stream before subscription to call Stream::start before Target::beginMedia
			if (Media::Target* pTarget = dynamic_cast<Media::Target*>(pStream.get())) {
				unique<Subscription> pSubscription(new Subscription(*pTarget));
				if (!subscribe(ex = nullptr, it.first, *pSubscription, pStream->query.c_str()))  // logs already displaid by subscribe
					continue;
				subscriptions.emplace(move(pSubscription));
			} else if (Media::Targets* pTargets = dynamic_cast<Media::Targets*>(pStream.get())) {
				pTargets->onTargetAdd = [this, &subscriptions, pStream, pTargets, publication = it.first](Media::Target& target) {
					shared<Subscription> pSubscription(SET, target);
					Exception ex;
					if (!subscribe(ex, publication, *pSubscription, pStream->query.c_str()))  // logs already displaid by subscribe
						return false;
					if (!pTargets->onTargetRemove)
						pTargets->onTargetRemove = [this, &subscriptions, pSubscription](Media::Target& target) {
							unsubscribe(*pSubscription);
							subscriptions.erase(pSubscription);
						};
					subscriptions.emplace(move(pSubscription));
					return true;
				};
			}
		} else {
			// publish
			Publication* pPublication = publish(ex = nullptr, it.first); // logs already displaid by publish
			if (!pPublication) {
				if (!pLastPublication || pLastPublication->name() != it.first)
					continue;
				pPublication = pLastPublication; // was same publication!
			} else
				pLastPublication = pPublication;
			if (it.second.empty()) { // LOGS PUBLICATION!
				pStream.set<MediaLogs>("logs",*pPublication, api()); // on error remove this stream, if no error server is shutdown!
				pStream->onStop = [&streams, pStream](const Exception& ex) { if (ex) streams.erase(pStream); };
			} else {
				AUTO_ERROR(pStream = Media::Stream::New(ex = nullptr, *pPublication, it.second, timer, ioFile, ioSocket, pTLSClient), it.second);
				if (!pStream)
					continue;
			}
			INFO(pStream->description(), " loaded on publication ", it.first);
			pStream->start(self);
			publications.emplace(pPublication);
		}
		streams.emplace(move(pStream));
	}
}

shared<Media::Stream> Server::stream(Media::Source& source, const string& description) {
	Exception ex;
	unique<Media::Stream> pStream;
	AUTO_ERROR(pStream = Media::Stream::New(ex, source, description, timer, ioFile, ioSocket, pTLSClient), description);
	return pStream ? *_streams.emplace(move(pStream)).first : nullptr;
}



} // namespace Mona
