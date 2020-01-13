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

namespace Mona {

struct ServerAPI : virtual Object, Parameters {
	// invocations

	const std::string&			www;
	const Handler&				handler;
	const Timer&				timer;

	const Protocols&			protocols;
	const Entity::Map<Client>	clients;
	
	const std::map<std::string, Publication>&	publications() { return _publications; }

	ThreadPool 				threadPool; // keep in first (must be build before ioSocket and ioFile)
	IOSocket				ioSocket;
	IOFile					ioFile;

	shared<TLS>				pTLSClient;
	shared<TLS>				pTLSServer;

	/*!
	Publish a publication
	Query parameters will be passed to publication properties (metadata)
	To change properties dynamically, send a @properties command, with a track it writes properties related one track, without track it overloads all */
	Publication*			publish(Exception& ex, std::string& stream) { return publish(ex, stream, NULL); }
	Publication*			publish(Exception& ex, const Path& stream, const char* query = NULL) { return publish(ex, stream.isFolder() ? String::Empty() : stream.baseName(), stream.extension().empty() ? NULL : stream.extension().c_str(), query, NULL); }
	Publication*			publish(Exception& ex, Client& client, std::string& stream) { return publish(ex, stream, &client); }
	Publication*			publish(Exception& ex, Client& client, const Path& stream, const char* query = NULL) { return publish(ex, stream.isFolder() ? String::Empty() : stream.baseName(), stream.extension().empty() ? NULL : stream.extension().c_str(), query, &client); }

	void					unpublish(Publication& publication) { unpublish(publication, NULL); }
	void					unpublish(Publication& publication, Client& client) { unpublish(publication, &client);  }

	/*!
	Subscribe to a publication
	If the subscription is already done, change just the subscriptions parameters, usefull to allow automatically the support by protocol of subscription dynamic parameters 
	If one other subscription exists already, switch publication in a smooth way (MBR)  */
	bool					subscribe(Exception& ex, std::string& stream, Subscription& subscription) { return subscribe(ex, stream, subscription, NULL); }
	bool					subscribe(Exception& ex, const std::string& stream, Subscription& subscription, const char* query = NULL) { return subscribe(ex, stream, subscription, query, NULL); }
	bool					subscribe(Exception& ex, Client& client, std::string& stream, Subscription& subscription) { return subscribe(ex, stream, subscription, &client); }
	bool					subscribe(Exception& ex, Client& client, const std::string& stream, Subscription& subscription, const char* query = NULL) { return subscribe(ex, stream, subscription, query, &client); }

	void					unsubscribe(Subscription& subscription) { unsubscribe(subscription, NULL); }
	void					unsubscribe(Client& client, Subscription& subscription) { unsubscribe(subscription, &client); }

	virtual bool			running() = 0;

	// events	
	virtual SocketAddress& 	onHandshake(const Path& path, const std::string& protocol, const SocketAddress& address, const Parameters& properties, SocketAddress& redirection) { return redirection; }
	virtual void			onConnection(Exception& ex,Client& client, DataReader& inParams, DataWriter& outParams) {} // Exception::SOFTWARE, Exception::APPLICATION
	virtual void			onDisconnection(Client& client) {}
	virtual void			onAddressChanged(Client& client,const SocketAddress& oldAddress) {}
	virtual bool			onInvocation(Exception& ex, Client& client, const std::string& name, DataReader& arguments, UInt8 responseType) { return false; } // Exception::SOFTWARE, Exception::APPLICATION
	virtual bool			onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties, Client* pClient) { return !mode; }  // Exception::SOFTWARE

	virtual bool			onPublish(Exception& ex, Publication& publication, Client* pClient){return true;}
	virtual void			onUnpublish(Publication& publication, Client* pClient){}

	virtual bool			onSubscribe(Exception& ex, Subscription& subscription, Publication& publication, Client* pClient){return true;}
	virtual void			onUnsubscribe(Subscription& subscription, Publication& publication, Client* pClient){}

protected:
	ServerAPI(std::string& www, std::map<std::string, Publication>& publications, const Handler& handler, const Protocols& protocols, const Timer& timer, UInt16 cores=0);

private:
	bool					subscribe(Exception& ex, std::string& stream, Subscription& subscription, Client* pClient);
	bool					subscribe(Exception& ex, const std::string& stream, Subscription& subscription, const char* query, Client* pClient);
	bool					subscribe(Exception& ex, Publication& publication, Subscription& subscription, Client* pClient);
	void					unsubscribe(Subscription& subscription, Client* pClient);
	void					unsubscribe(Subscription& subscription, Publication* pPublication, Client* pClient);

	Publication*			publish(Exception& ex, std::string& stream, Client* pClient);
	Publication*			publish(Exception& ex, const std::string& stream, const char* ext, const char* query, Client* pClient);
	void					unpublish(Publication& publication, Client* pClient);

	std::map<std::string, Publication>&	_publications;
};


} // namespace Mona
