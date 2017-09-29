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

#include "Mona/ServerAPI.h"
#include "Mona/QueryReader.h"
#include "Mona/MapWriter.h"
#include "Mona/MapReader.h"
#include "Mona/Util.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

ServerAPI::ServerAPI(const Path& application, const Path& www, const Handler& handler, const Protocols& protocols, const Timer& timer, UInt16 cores) :
	threadPool(cores * 2), application(application), www(www), protocols(protocols), timer(timer), handler(handler), ioSocket(handler, threadPool), ioFile(handler, threadPool), publications(_publications), clients() {
}

void ServerAPI::manage() {
	auto it = _waitingKeys.begin();
	while (it != _waitingKeys.end()) {
		auto itSubscription = it->second.subscriptions.begin();
		while (itSubscription != it->second.subscriptions.end()) {
			if (itSubscription->first->ejected()) {
				// congested/error (ejected) => switch immediatly!
				it->second.raise(*itSubscription->first, itSubscription->second);
				itSubscription = it->second.subscriptions.erase(itSubscription);
				continue;
			}
			++itSubscription;
		}
		// remove empty WaitingKeys
		if (it->second.subscriptions.empty()) {
			it = _waitingKeys.erase(it);
			continue;
		}
		++it;
	}
}

Publication* ServerAPI::publish(Exception& ex, string& stream, Client* pClient) {
	size_t found = stream.find('?');
	const char* query(NULL);
	if (found != string::npos) {
		query = stream.c_str() + found + 1;
		stream.resize(found);
	}
	found = stream.find_last_of('.');
	const char* ext(NULL);
	if (found != string::npos) {
		ext = stream.c_str() + found + 1;
		stream.resize(found);
	}
	return publish(ex, stream, ext, query, pClient);
}

Publication* ServerAPI::publish(Exception& ex, const string& stream, const char* ext, const char* query, Client* pClient) {
	if (pClient && !pClient->connection) {
		ERROR(ex.set<Ex::Intern>("Client must be connected before ", stream, " publication"));
		return NULL;
	}
	
	const auto& it = _publications.emplace(piecewise_construct, forward_as_tuple(stream), forward_as_tuple(stream));
	Publication& publication(it.first->second);

	if (publication.publishing()) {
		ERROR(ex.set<Ex::Intern>(stream, " is already publishing"));
		return NULL;
	}

	if (query) {
		// write metadata!
		// allow to work with any protocol and easy query writing
		// Ignore the FMLE case which write two times properties (use query and metadata, anyway query are include in metadata (more complete))
		QueryReader reader(query);
		MapWriter<Parameters> writer(publication);
		reader.read(writer);
	}
	if (String::ICompare(getString(stream), EXPAND("publication"))==0) {
		// Write static metadata configured
		String name(stream,'.');
		for (auto& it : range(name))
			publication.setString(it.first.c_str()+name.size(), it.second);
	}

	if (onPublish(ex, publication, pClient)) {
		if(ext) {
			// RECORD!
			MediaWriter* pWriter = MediaWriter::New(ext);
			if (pWriter) {
				Path path(MAKE_FILE(www), pClient ? pClient->path : "", "/", stream,'.', ext);
				bool append(false);
				publication.getBoolean("append", append);
				MapReader<Parameters> arguments(publication);
				Parameters parameters;
				MapWriter<Parameters> properties(parameters);
				if (onFileAccess(ex, append ? File::MODE_APPEND : File::MODE_WRITE, path, arguments, properties, pClient)) {
					parameters.getBoolean("append", append);
					publication.start(new MediaFile::Writer(path, pWriter, ioFile), append);
					return &publication;
				}
				delete pWriter;
			} else
				WARN(ex.set<Ex::Unsupported>(stream, " recording format ", ext, " not supported"));
		}
		publication.start();
		return &publication;
	}

	// Erase properties, we can because just the publisher can write metadata and this publication has no publisher, so should be without metadata
	publication.clear();

	if(!ex)
		ex.set<Ex::Permission>("Not allowed to publish ", stream);
	ERROR(ex);
	if (publication.subscriptions.empty())
		_publications.erase(it.first);
	return NULL;
}


void ServerAPI::unpublish(Publication& publication, Client* pClient) {
	publication.stop();
	const auto& it = _publications.find(publication.name());
	if(it == _publications.end()) {
		WARN("Publication ", publication.name()," unfound");
		return;
	}
	if (pClient && !pClient->connection)
		ERROR("Unpublication client before connection")
	else
		onUnpublish(publication, pClient);
	if(publication.subscriptions.empty())
		_publications.erase(it);
}

bool ServerAPI::subscribe(Exception& ex, string& stream, Subscription& subscription, Client* pClient) {
	size_t found(stream.find('?'));
	const char* query(NULL);
	if (found != string::npos) {
		query = stream.c_str() + found + 1;
		stream.resize(found);
	}
	found = stream.find_last_of('.');
	const char* ext(NULL);
	if (found != string::npos) {
		ext = stream.c_str() + found + 1;
		stream.resize(found);
	}
	return subscribe(ex, stream, ext, subscription, query, pClient);
}

bool ServerAPI::subscribe(Exception& ex, const string& stream, const char* ext, Subscription& subscription, const char* queryParameters, Client* pClient) {
	if (pClient && !pClient->connection) {
		ERROR(ex.set<Ex::Intern>("Client must be connected before ",stream," publication subscription"))
		return false;
	}

	Parameters parameters;
	if (queryParameters)
		Util::UnpackQuery(queryParameters, parameters);
	if (ext)
		parameters.setString("format", ext);

	auto it = _publications.lower_bound(stream);
	if (it == _publications.end() || it->first != stream) {
		// is a new unfound publication => request failed, but keep possibly already subscription (MBR fails! stays on current subscription!)
		UInt32 timeout;
		if (parameters.getNumber("timeout", timeout) && !timeout) {
			WARN(ex.set<Ex::Unfound>("Publication ", stream, " unfound"));
			return false;
		}
		it = _publications.emplace_hint(it, piecewise_construct, forward_as_tuple(stream), forward_as_tuple(stream));
	} 

	Publication& publication(it->second);
	
	// Change parameters just now to limit change event propagation to Target
	(Parameters&)subscription = move(parameters);

	if(subscription.pPublication) {
		// subscribed?
		if (subscription.pNextPublication) {
			if (subscription.pNextPublication == &publication)
				return true; // already subscribed => just a parameters change!
			unsubscribe(subscription, *subscription.pNextPublication, pClient); // unsubscribe previous MBR switching
		} else if (subscription.pPublication == &publication)
			return true; // already subscribed => just a parameters change!
		// MBR switch
	} // else no subscription yet

	if (!onSubscribe(ex, subscription, publication, pClient)) {
		// if ex, error has already been displayed as logby onSubscribe
		// no unsubscribption to keep a possible previous subscription (MBR switch fails)
		if (!publication)
			_publications.erase(it);
		if (!ex)
			WARN(ex.set<Ex::Permission>("Not authorized to play ", stream));
		return false;
	}

	((set<Subscription*>&)publication.subscriptions).emplace(&subscription);

	if (subscription.pPublication) {
		// publication switch (MBR)
		for (UInt8 i = 1; i <= publication.videos.size();++i) {
			if (subscription.target.videoSelected(i)) {
				subscription.pNextPublication = &publication;
				_waitingKeys.emplace(piecewise_construct, forward_as_tuple(&publication), forward_as_tuple(*this, publication)).first->second
					.subscriptions.emplace(piecewise_construct, forward_as_tuple(&subscription), forward_as_tuple(pClient));
				return true;
			}
		}
		// no videos track matching => switch immediatly
		subscription.pNextPublication = NULL;
		unsubscribe(subscription, *subscription.pPublication, pClient);
		subscription.pPublication = &publication;
		subscription.reset(); // to get properties(metadata) and new state of new publication...
	} else
		subscription.pPublication = &publication;
	return true;
}


void ServerAPI::unsubscribe(Subscription& subscription, Client* pClient) {
	if (subscription.pNextPublication)
		unsubscribe(subscription, *subscription.pNextPublication, pClient);
	if (subscription.pPublication)
		unsubscribe(subscription, *subscription.pPublication, pClient);
	subscription.pPublication = subscription.pNextPublication = NULL;
}

void ServerAPI::unsubscribe(Subscription& subscription, Publication& publication, Client* pClient) {
	if (!((set<Subscription*>&)publication.subscriptions).erase(&subscription))
		return; // no subscription
	if (pClient && !pClient->connection)
		ERROR(publication.name()," unsubscription before client connection")
	else
		onUnsubscribe(subscription, publication, pClient);
	if (subscription.pNextPublication == &publication) {
		const auto& itWaitingKey(_waitingKeys.find(&publication));
		itWaitingKey->second.subscriptions.erase(&subscription);
		if (itWaitingKey->second.subscriptions.empty())
			_waitingKeys.erase(itWaitingKey);
	}
	if (!publication)
		_publications.erase(publication.name());
}


} // namespace Mona
