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
#include "Mona/MapWriter.h"
#include "Mona/MapReader.h"
#include "Mona/URL.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

ServerAPI::ServerAPI(std::string& www, map<string, Publication>& publications, const Handler& handler, const Protocols& protocols, const Timer& timer, UInt16 cores) :
	www(www), _publications(publications), threadPool(cores), protocols(protocols), timer(timer), handler(handler), ioSocket(handler, threadPool), ioFile(handler, threadPool, cores), clients() {
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
	
	const auto& it = _publications.emplace(SET, forward_as_tuple(stream), forward_as_tuple(stream));
	Publication& publication(it.first->second);

	if (publication.publishing()) {
		ERROR(ex.set<Ex::Intern>(stream, " is already publishing"));
		return NULL;
	}

	if (query) {
		// write metadata!
		// allow to work with any protocol and easy query writing
		// Ignore the FMLE case which write two times properties (use query and metadata, anyway query are include in metadata (more complete))
		URL::ParseQuery(query, publication);
	}

	// Write static metadata configured
	if (String::ICompare(getString(stream), "publication")==0) {
		String name(stream,'.');
		for (auto& it : range(name))
			publication.setString(it.first.c_str()+name.size(), it.second);
	}

	if (onPublish(ex, publication, pClient)) {
		if(ext) {
			// RECORD!
			Path path(www, pClient ? pClient->path : "", stream,'.', ext);
			bool append(false);
			publication.getBoolean("append", append);
			MapReader<Parameters> arguments(publication);
			Parameters parameters;
			MapWriter<Parameters> properties(parameters);
			if (onFileAccess(ex, append ? File::MODE_APPEND : File::MODE_WRITE, path, arguments, properties, pClient)) {
				unique<MediaFile::Writer> pFileWriter = MediaFile::Writer::New(ex, path.c_str(), ioFile);
				if (pFileWriter) {
					parameters.getBoolean("append", append);
					publication.start(move(pFileWriter), append);
					return &publication;
				}
				WARN(stream, " impossible to record, ", ex);
			}
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
	const auto& it = _publications.find(publication.name());
	if (it == _publications.end()) {
		ERROR("Publication ", publication.name(), " unfound");
		return;
	}
	if (publication.publishing()) {
		publication.start(); // = reset without logs: keep publication.running() until its deletion which must happen AFTER onUnpublish (otherwise a unsubscribe could erase too the publication)
		if (pClient && !pClient->connection)
			ERROR("Unpublication client before connection")
		else
			onUnpublish(publication, pClient);
		publication.stop();
	}
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
	return subscribe(ex, stream, subscription, query, pClient);
}

bool ServerAPI::subscribe(Exception& ex, const string& stream, Subscription& subscription, const char* queryParameters, Client* pClient) {
	// check that client is connected before!
	if (pClient && !pClient->connection) {
		ERROR(ex.set<Ex::Intern>("Client must be connected before ", stream, " publication subscription"))
		return false;
	}

	// update parameters
	Parameters parameters;
	if (queryParameters)
		URL::ParseQuery(queryParameters, parameters);
	const char* mbr = parameters.getString("mbr");
	if (mbr) // add "this" mbr if mbr param!
		parameters.emplace("mbr", String(mbr, "|", stream));

	// Change parameters with just one call to limit change event propagation to Target
	subscription.setParams(move(parameters));

	// check if subscription has already subscribed for one of theses stream (more faster than _publication.find(...))
	if (subscription.subscribed(stream))
		return true; // already subscribed = just a parameters change => useless to recall subscribe/unsubscribe (performance reason and no security issue because subscription has been accepted before)

	auto it = _publications.lower_bound(stream);
	if (it == _publications.end() || it->first != stream) {
		// is a new unfound publication => request failed, but keep possibly already subscription (MBR fails! stays on current subscription!)
		UInt32 timeout;
		if (subscription.getNumber("timeout", timeout) && !timeout) {
			WARN(ex.set<Ex::Unfound>("Publication ", stream, " unfound"));
			return false;
		}
		it = _publications.emplace_hint(it, SET, forward_as_tuple(stream), forward_as_tuple(stream));

		// Write static metadata configured
		if (String::ICompare(getString(stream), "publication") == 0) {
			String name(stream, '.');
			for (auto& itProp : range(name))
				it->second.setString(itProp.first.c_str() + name.size(), itProp.second);
		}
	}

	if (!subscribe(ex, it->second, subscription, pClient)) {
		if (!it->second)
			_publications.erase(it);
		return false;
	}

	// update onMBR and onNext with new pClient value!
	subscription.onMBR = nullptr;
	subscription.onMBR = [this, &subscription, pClient](const set<string>& streams, bool down) {
		struct Comparator { bool operator()(const Publication* pA, const Publication* pB) const { return pA->byteRate()<pB->byteRate(); } };
		set<Publication*, Comparator> publications;
		// sort publication by ByteRate
		for (const string& stream : streams) {
			if (stream == subscription.pPublication->name())
				continue; // itself!
			const auto& it = _publications.find(stream);
			if (it != _publications.end())
				publications.emplace(&it->second);
		}
		auto it = publications.lower_bound(subscription.pPublication);
		if (down) {
			while (it != publications.begin()) {
				Exception ex;
				if (subscription.subscribed(**--it) || subscribe(ex, **it, subscription, pClient))
					return;
			}
		} else while (it != publications.end()) {
			Exception ex;
			if (subscription.subscribed(**it) || subscribe(ex, **it, subscription, pClient))
				return;
			++it;
		}
		// if "down" and no more down possible, cancel a possible pNextPublication "up" (return to publication)
		// if "ûp" and no more up possible, cancel a possible pNextPublication "down" (return to publication)
		unsubscribe(subscription, subscription.setNext(NULL), pClient);
	};
	subscription.onNext = nullptr;
	subscription.onNext = [this, &subscription, pClient](Publication& publication) {
		unsubscribe(subscription, subscription.pPublication, pClient);
		subscription.pPublication = &publication; // do next!
	};
	return true;
}


bool ServerAPI::subscribe(Exception& ex, Publication& publication, Subscription& subscription, Client* pClient) {
	// intern call, no need to check subscription.pPublication (alreay checked)
	if (!onSubscribe(ex, subscription, publication, pClient)) {
		// if ex, error has already been displayed as log by onSubscribe
		// no unsubscribption to keep a possible previous subscription (MBR switch fails)
		if (!ex)
			WARN(ex.set<Ex::Permission>("Not authorized to play ", publication.name()));
		return false;
	}
	((set<Subscription*>&)publication.subscriptions).emplace(&subscription);

	if (subscription.pPublication)
		unsubscribe(subscription, subscription.setNext(&publication), pClient); // publication switch (MBR) + cancel possible previous next!
	else
		subscription.pPublication = &publication;
	DEBUG((pClient ? pClient->address : typeof(self)), " subscribes to ", publication.name());
	return true;
}


void ServerAPI::unsubscribe(Subscription& subscription, Client* pClient) {
	subscription.onMBR = nullptr; // public unsubscribe, remove possible onMBR!
	unsubscribe(subscription, subscription.setNext(NULL), pClient);
	unsubscribe(subscription, subscription.pPublication, pClient);
	subscription.pPublication = NULL;
}

void ServerAPI::unsubscribe(Subscription& subscription, Publication* pPublication, Client* pClient) {
	if (!pPublication)
		return;
	if (!((set<Subscription*>&)pPublication->subscriptions).erase(&subscription))
		return; // no subscription
	DEBUG((pClient ? pClient->address : typeof(self)), " unsubscribes to ", pPublication->name());
	if (pClient && !pClient->connection)
		ERROR(pPublication->name()," unsubscription before client connection")
	else
		onUnsubscribe(subscription, *pPublication, pClient);
	if (!*pPublication)
		_publications.erase(pPublication->name());
}


} // namespace Mona
