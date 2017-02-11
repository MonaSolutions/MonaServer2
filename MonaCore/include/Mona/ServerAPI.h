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
#include "Mona/IOSocket.h"
#include "Mona/Publication.h"
#include "Mona/ThreadPool.h"
#include "Mona/IOFile.h"
#include "Mona/Timer.h"
#include "Mona/TLS.h"
#include "Mona/Protocols.h"
#include "Mona/Client.h"
#include <set>

namespace Mona {

struct ServerAPI : virtual Object, Parameters {
	// invocations
	const Handler&			handler;
	const Timer&			timer;

	const Path&				application;
	const Path&				www;

	const Protocols&			protocols;
	const Entity::Map<Client>	clients;
	
	const std::map<std::string, Publication>&	publications;

	IOSocket				ioSocket;
	IOFile					ioFile;
	const ThreadPool&		threadPool;

	shared<TLS>	pTLSClient;
	shared<TLS>	pTLSServer;

	Publication*			publish(Exception& ex, std::string& stream) { return publish(ex, stream, NULL); }
	Publication*			publish(Exception& ex, const Path& stream, const char* query = NULL) { return publish(ex, stream.baseName(), stream.extension().empty() ? NULL : stream.extension().c_str(), query, NULL); }
	Publication*			publish(Exception& ex, Client& client, std::string& stream) { return publish(ex, stream, &client); }
	Publication*			publish(Exception& ex, Client& client, const Path& stream, const char* query = NULL) { return publish(ex, stream.baseName(), stream.extension().empty() ? NULL : stream.extension().c_str(), query, &client); }

	void					unpublish(Publication& publication) { unpublish(publication, NULL); }
	void					unpublish(Publication& publication, Client& client) { unpublish(publication, &client);  }

	/*!
	Subscribe to a publication
	query parameters:
		mode
			"play", like a file, want play immediatly the media
			"call", like a phone, wait publication without timeout on stop */
	bool					subscribe(Exception& ex, std::string& stream, Subscription& subscription) { return subscribe(ex, stream, subscription, NULL); }
	bool					subscribe(Exception& ex, const std::string& stream, Subscription& subscription, const char* queryParameters = NULL) { return subscribe(ex, stream, subscription, queryParameters, NULL); }
	bool					subscribe(Exception& ex, std::string& stream, Client& client, Subscription& subscription) { return subscribe(ex, stream, subscription, &client); }
	bool					subscribe(Exception& ex, const std::string& stream, Client& client, Subscription& subscription, const char* queryParameters = NULL) { return subscribe(ex, stream, subscription, queryParameters, &client); }

	void					unsubscribe(Subscription& subscription) { unsubscribe(subscription, NULL); }
	void					unsubscribe(Client& client, Subscription& subscription) { unsubscribe(subscription, &client); }

	void					addBanned(const IPAddress& ip) { _bannedAddresses.emplace(ip); }
	void					removeBanned(const IPAddress& ip) { _bannedAddresses.erase(ip); }
	void					clearBannedList() { _bannedAddresses.clear(); }
	bool					isBanned(const IPAddress& ip) { return _bannedAddresses.find(ip) != _bannedAddresses.end(); }

	// events	
	virtual void			onStart(){}
	virtual void			onStop(){}

	virtual void			onHandshake(const std::string& protocol,const SocketAddress& address,const std::string& path,const Parameters& properties,UInt8 attempts,std::set<SocketAddress>& addresses){}
	virtual void			onConnection(Exception& ex,Client& client,DataReader& arguments,DataWriter& response) {} // Exception::SOFTWARE, Exception::APPLICATION
	virtual void			onDisconnection(Client& client) {}
	virtual void			onAddressChanged(Client& client,const SocketAddress& oldAddress) {}
	virtual bool			onInvocation(Exception& ex, Client& client, const std::string& name, DataReader& arguments, UInt8 responseType) { return false; } // Exception::SOFTWARE, Exception::APPLICATION
	virtual bool			onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties, Client* pClient){return true;}  // Exception::SOFTWARE

	virtual bool			onPublish(Exception& ex, const Publication& publication, Client* pClient){return true;}
	virtual void			onUnpublish(const Publication& publication, Client* pClient){}

	virtual bool			onSubscribe(Exception& ex, const Subscription& subscription, const Publication& publication, Client* pClient){return true;}
	virtual void			onUnsubscribe(const Subscription& subscription, const Publication& publication, Client* pClient){}

protected:
	ServerAPI(const Path& application, const Path& www, const Handler& handler, const Protocols& protocols, const Timer& timer, ThreadPool& threadPool);
	
	void					manage();
private:

	bool					subscribe(Exception& ex, std::string& stream, Subscription& subscription, Client* pClient);
	bool					subscribe(Exception& ex, const std::string& stream, Subscription& subscription, const char* queryParameters, Client* pClient);
	void					unsubscribe(Subscription& subscription, Client* pClient);
	void					unsubscribe(Subscription& subscription, Publication& publication, Client* pClient);

	Publication*			publish(Exception& ex, std::string& stream, Client* pClient);
	Publication*			publish(Exception& ex, const std::string& stream, const char* ext, const char* query, Client* pClient);
	void					unpublish(Publication& publication, Client* pClient);


	std::set<IPAddress>					_bannedAddresses;
	std::map<std::string,Publication>	_publications;

	struct WaitingKey : virtual Object {
		WaitingKey(ServerAPI& api, Publication& publication) : _api(api), publication(publication) {
			publication.onKeyFrame = [this](UInt16 track) {
				auto it = subscriptions.begin();
				while (it != subscriptions.end()) {
					if (it->first->videos.enabled(track)) {
						raise(*it->first, it->second.second);
						it = subscriptions.erase(it);
					} else
						++it;
				}
			};
		}
		~WaitingKey() {
			publication.onKeyFrame = nullptr;
		}
		void raise(Subscription& subscription, Client* pClient) {
			_api.unsubscribe(subscription, *subscription.pPublication, pClient);
			subscription.pNextPublication = NULL;
			subscription.pPublication = &publication;
		}
		Publication&										publication;
		std::map<Subscription*, std::pair<Int64, Client*>>	subscriptions;
	private:
		ServerAPI&					  _api;
	};

	std::map<Publication*, WaitingKey>	_waitingKeys;
	
};


} // namespace Mona
