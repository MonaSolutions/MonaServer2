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
#include "Mona/TSReader.h"
#include "Mona/MediaSocket.h"


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
		if (!_www && String::ICompare(it.first, EXPAND("www.dir")) == 0)
			_www.set(it.second);
		setString(it.first, it.second);
	}

	if(!_www)
		_www.set("www/");

	Exception ex;
	AUTO_ERROR(FileSystem::CreateDirectory(ex, _www), "Application directory creation");

	Thread::start();
}

bool Server::publish(const char* name, shared<Publish>& pPublish) {
	if (!running()) {
		ERROR("Start ", typeof(*this), " before to publish ", name);
		return false;
	}
	pPublish.reset(new Publish(*this, name));
	return true;
}

bool Server::run(Exception&, const volatile bool& requestStop) {
	BufferPool bufferPool(timer);
	if(getBoolean<true>("poolBuffers"))
		Buffer::SetAllocator(bufferPool);

	Timer::OnTimer onManage;
	multimap<string, Media::Stream*>	streams;
	set<Publication*>					publications;
	set<Subscription*>					subscriptions;

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

		// Load streams before onStart because can change API properties!
		loadStreams(streams);

		onStart();

		// Start streams
		startStreams(streams, publications, subscriptions);

		onManage = ([&](UInt32) {
			sessions.manage(); // in first to detect session useless died
			_protocols.manage(); // manage custom protocol manage (resource protocols)

			// reset subscription ejected
			for (Subscription* pSubscription : subscriptions) {
				if (pSubscription->ejected())
					pSubscription->reset(); // reset ejected!
			}
			// Pulse streams!
			for (auto& it : streams)
				it.second->start(self);
			
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

	// delete streams
	for (Subscription* pSubscription : subscriptions) {
		unsubscribe(*pSubscription);
		delete pSubscription;
	}
	for (auto& it : streams)
		delete it.second;
	for (Publication* pPublication : publications)
		unpublish(*pPublication);

	// TODO? terminate relay server ((RelayServer&)relayer).stop();

	// stop event to unload children resource (before to release sockets, threads, and buffers)
	onStop();

	// stop socket sending (it waits the end of sending last session messages)
	threadPool.join();

	// finish writing file before to detach buffer allocator
	ioFile.join();

	// empty handler!
	_handler.flush();

	// release memory
	INFO("Server memory release");
	Buffer::SetAllocator();
	bufferPool.clear();

	NOTE("Server stopped");
	_www.reset();
	return true;
}

void Server::loadStreams(multimap<string, Media::Stream*>& streams) {
	// Load Streams configs
	string temp;
	vector<const char*> keyToRemove;
	for (auto& it : *this) {
		// [MyPublication=Publication...] => Start by "publication"
		if (it.first.find('.') != string::npos || String::ICompare(it.second, EXPAND("publication")) != 0)
			continue;

		for (auto& it2 : range(temp.assign(it.first) += '.')) {
			const char* name(it2.first.c_str() + temp.size());
			if (!it2.second.empty())
				continue;
			Media::Stream* pStream;
			Exception ex;
			AUTO_ERROR(pStream = Media::Stream::New(ex, name, timer, ioFile, ioSocket, pTLSClient), name);
			if (!pStream)
				continue;
			streams.emplace(it.first, pStream);
			keyToRemove.emplace_back(it2.first.c_str());
		}
	}
	// Erase key which was streams, security => to remove it of metadata publication!
	for (const char* key : keyToRemove)
		erase(key);
}

void Server::startStreams(multimap<string, Media::Stream*>& streams, set<Publication*>& publications, set<Subscription*>& subscriptions) {
	Exception ex;
	Publication* pLastPublication(NULL);
	auto it = streams.begin();
	while (it!=streams.end()) {
		Media::Stream* pStream(it->second);
		if (Media::Target* pTarget = dynamic_cast<Media::Target*>(pStream)) {
			INFO(pStream->description(), " loaded on publication ", it->first);
			Subscription* pSubscription(new Subscription(*pTarget));
			pStream->start(self); // Start stream before subscription to call Stream::start before Target::beginMedia
			if (!subscribe(ex, it->first, *pSubscription, pStream->query.c_str())) {  // logs already displaid by subscribe
				delete pSubscription;
				streams.erase(it++);
				delete pStream;
				continue;
			}
			subscriptions.emplace(pSubscription);
		} else {
			// publish
			Publication* pPublication = publish(ex=nullptr, it->first); // logs already displaid by publish
			if (!pPublication) {
				if (!pLastPublication || pLastPublication->name() != it->first) {
					streams.erase(it++);
					delete pStream;
					continue;
				}
				pPublication = pLastPublication; // was same publication!
			}
			INFO(pStream->description()," loaded on publication ", it->first);
			publications.emplace(pLastPublication = pPublication);
			pStream->start(*pPublication, self);
		}
		++it;
	}
}



} // namespace Mona
