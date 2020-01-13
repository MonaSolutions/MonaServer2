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

#include "MonaTiny.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

/*
#include "Mona/TCPClient.h"
#include "Mona/HTTP/HTTPDecoder.h"
// A appli FFMPEG to allow to stream from HTTP listen ffmpeg to WebSocket 
struct FFMPEG : App {
	FFMPEG(ServerAPI& api) : App(api), _api(api)  {}
private:
	struct Client : App::Client {
		Client(Mona::Client& client, ServerAPI& api) : App::Client(client), _http(client, api) {
			Exception ex;
			SocketAddress address(IPAddress::Loopback(), 8080);
			_http.connect(ex, address);
			shared<Buffer> pBuffer(SET);
			String::Append(*pBuffer,"GET /test HTTP/1.1\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache\r\nConnection: close\r\nUser-Agent: MonaServer\r\nHost: ");
			String::Append(*pBuffer, address, "\r\n\r\n");
			_http.send(ex, Packet(pBuffer));
		}
		struct HTTPClient : TCPClient {
			HTTPClient(Mona::Client& client, ServerAPI& api) : _api(api), TCPClient(api.ioSocket),
				_onResponse([this, &client](HTTP::Response& response) {
					client.writer().writeResponse(2).writeByte(response.data(), response.size());
					client.writer().flush();
				}) {
			}
		private:
			shared<Socket::Decoder> newDecoder() {
				shared<HTTPDecoder> pDecoder(new HTTPDecoder(_api.handler, _api.www));
				pDecoder->onResponse = _onResponse;
				return pDecoder;
			}
			ServerAPI& _api;
			HTTPDecoder::OnResponse _onResponse;
		} _http;
	};
	App::Client* newClient(Exception& ex, Mona::Client& client, DataReader& parameters, DataWriter& response) {
		return new Client(client, _api);
	}
	ServerAPI& _api;
};
*/

//// Server Events /////
void MonaTiny::onStart() {
	// create applications
	//_applications["ffmpeg/"] = new FFMPEG(*this);
}

void MonaTiny::onManage() {
	// manage application!
	for (auto& it : _applications)
		it.second->manage();
}

void MonaTiny::onStop() {
	
	// delete applications
	for (auto& it : _applications)
		delete it.second;
	_applications.clear();

	// unblock ctrl+c waiting
	_terminateSignal.set();
}

//// Client Events /////
SocketAddress& MonaTiny::onHandshake(const Path& path, const string& protocol, const SocketAddress& address, const Parameters& properties, SocketAddress& redirection) {
	DEBUG(protocol, " ", address, " handshake to ", path.length() ? path : "/");
	const auto& it(_applications.find(path));
	return it == _applications.end() ? redirection : it->second->onHandshake(protocol, address, properties, redirection);
}


void MonaTiny::onConnection(Exception& ex, Client& client, DataReader& inParams, DataWriter& outParams) {
	DEBUG(client.protocol, " ", client.address, " connects to ", client.path.length() ? client.path : "/")
	const auto& it(_applications.find(client.path));
	if (it == _applications.end())
		return;
	client.setCustomData<App::Client>(it->second->newClient(ex,client, inParams, outParams));
}

void MonaTiny::onDisconnection(Client& client) {
	DEBUG(client.protocol, " ", client.address, " disconnects from ", client.path.length() ? client.path : "/");
	if (client.hasCustomData()) {
		delete client.getCustomData<App::Client>();
		client.setCustomData<App::Client>(NULL);
	}
}

void MonaTiny::onAddressChanged(Client& client, const SocketAddress& oldAddress) {
	if (client.hasCustomData())
		client.getCustomData<App::Client>()->onAddressChanged(oldAddress);
}

bool MonaTiny::onInvocation(Exception& ex, Client& client, const string& name, DataReader& arguments, UInt8 responseType) {
	/* Haivision Encoder Patch
	if (name == "FCPublish") {
		FlashWriter* pWriter = dynamic_cast<FlashWriter*>(&client.writer());
		if (pWriter) {
			string stream(client.path);
			arguments.readString(stream);
			pWriter->writeAMFState("onFCPublish", "NetStream.Publish.Start", "status", stream + " is now published");
			return true;
		}
	}*/
	// on client message, returns "false" if "name" message is unknown
	DEBUG(name, " call from ", client.protocol, " to ", client.path.length() ? client.path : "/");
	if (client.hasCustomData())
		return client.getCustomData<App::Client>()->onInvocation(ex, name, arguments,responseType);
	return false;
} 


bool MonaTiny::onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties, Client* pClient) {
	// on client file access, returns "false" if acess if forbiden
	if(pClient) {
		DEBUG(file.name(), " file access from ", pClient->protocol, " to ", pClient->path.length() ? pClient->path : "/");
		if (pClient->hasCustomData())
			return pClient->getCustomData<App::Client>()->onFileAccess(ex, mode, file, arguments, properties);
	} else
		DEBUG(file.name(), " file access to ", file.parent().empty() ? "/" : file.parent());
	// arguments.read(properties); to test HTTP page properties (HTTP parsing!)
	return !mode;
}


//// Publication Events /////

bool MonaTiny::onPublish(Exception& ex, Publication& publication, Client* pClient) {
	if (pClient) {
		NOTE("Client publish ", publication.name());
		if (pClient->hasCustomData())
			return pClient->getCustomData<App::Client>()->onPublish(ex, publication);
	} else
		NOTE("Publish ",publication.name())

	return true; // "true" to allow, "false" to forbid
}

void MonaTiny::onUnpublish(Publication& publication, Client* pClient) {
	if (pClient) {
		NOTE("Client unpublish ", publication.name());
		if(pClient->hasCustomData())
			pClient->getCustomData<App::Client>()->onUnpublish(publication);
	} else
		NOTE("Unpublish ",publication.name())
}

bool MonaTiny::onSubscribe(Exception& ex, Subscription& subscription, Publication& publication, Client* pClient) {
	if (pClient) {
	//	_test.start(*publish(ex, publication.name()));
		INFO(pClient->protocol, " ", pClient->address, " subscribe to ", publication.name());
		if (pClient->hasCustomData())
			return pClient->getCustomData<App::Client>()->onSubscribe(ex, subscription, publication);
	} else
		INFO("Subscribe to ", publication.name());
	return true; // "true" to allow, "false" to forbid
} 

void MonaTiny::onUnsubscribe(Subscription& subscription, Publication& publication, Client* pClient) {
	if (pClient) {
	//	_test.stop();
	//	unpublish((Publication&)publication);
	//	return;
		INFO(pClient->protocol, " ", pClient->address, " unsubscribe to ", publication.name());
		if (pClient->hasCustomData())
			return pClient->getCustomData<App::Client>()->onUnsubscribe(subscription, publication);
	} else
		INFO("Unsubscribe to ", publication.name());
}

} // namespace Mona
