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

#include "MonaServer.h"
#include "ScriptWriter.h"
#include "ScriptReader.h"
#include "LUASocketAddress.h"
#include "Mona/Session.h"


using namespace std;

namespace Mona {

MonaServer::MonaServer(const Parameters& configs, TerminateSignal& terminateSignal) : _starting(false),
	Server(configs.getNumber<UInt16>("cores")), _terminateSignal(terminateSignal), _dataPath(configs.getString("data.dir", "data/")) {

}

MonaServer::~MonaServer() {
	stop();
}

void MonaServer::onStart() {
	Util::Scoped<bool> starting(_starting, true);
	
	_pState = Script::CreateState();

	// init root server application
	_pService.set(_pState, www, (Service::Handler&)self);

	// Init few global variable
	Script::AddObject(_pState, api());
	lua_setglobal(_pState,"mona");

	// load database
	Exception ex;
	bool firstData(true);
	PersistentData::ForEach forEach([this,&firstData](const string& path, const Packet& packet) {
		if (firstData) {
			INFO("Database loading...")
			firstData = false;
		}
		_data.setString(path, STR packet.data(), packet.size());
	});
	_persistentData.load(ex, _dataPath, forEach);
	if (ex)
		WARN("Database load, ",ex)
	else if (!firstData)
		NOTE("Database loaded")
	_data.onChange = [this](const string& key, const string* pValue) {
		if (pValue)
			_persistentData.add(key, Packet(pValue->data(), pValue->size()));
		else
			_persistentData.remove(key);
	};
	_data.onClear = [this]() {
		_persistentData.clear();
	};

	Script::AddObject(_pState, _data);
	lua_setglobal(_pState, "data");
	
	// start the application (if exists)
	_pService->open(ex = nullptr);
}

void MonaServer::onStop() {
	// delete service before servers.stop() to avoid a crash bug
	if(_pService)
		_pService.reset();
	Script::CloseState(_pState);
	_data.onChange = nullptr;
	_data.onClear = nullptr;
	if (_persistentData.writing()) {
		INFO("Database flushing...")
		_persistentData.flush();
	}
	_terminateSignal.set();
}


void MonaServer::onManage() {
	// control script size to debug!
	if (lua_gettop(_pState) != 0)
		CRITIC("LUA stack corrupted, contains ",lua_gettop(_pState)," irregular values");

	Exception ex;
	_pService->open(ex); // TODO remove when FilesWatching implemented!
}

void MonaServer::onUpdate(Service& service) {
	// When a application is reloaded, the resource are released (to liberate port for example of one lua socket)
	// So client which have record their member function with the onConnection of this application can not more interact with, we have to close them!
	// All client in the inheritance application branch have to be disconnected. Ex: "/live" is updating, clients with path "/", "/live", "/live/sub" have to reconnect, clients with path "/live2" can stay alive!
	auto it = clients.begin();
	while (it != clients.end()) { // while loop because the close can remove immediatly client from clients!
		Client& client = *it++->second;
		size_t minSize = min(service.path.size(), client.path.size());
		if (String::ICompare(service.path, client.path, minSize) != 0)
			continue;
		const string& maxPath = client.path.size()>minSize ? client.path : service.path;
		if(!maxPath[minSize] || maxPath[minSize] == '/')
			client.close(Session::ERROR_UPDATE);
	}
}

SocketAddress& MonaServer::onHandshake(const string& path, const string& protocol, const SocketAddress& address, const Parameters& properties, SocketAddress& redirection) {
	Exception ex;
	Service* pService(_pService->open(ex, path));
	if (!pService)
		return redirection;
	SCRIPT_BEGIN(_pState)
		SCRIPT_FUNCTION_BEGIN("onHandshake",pService->reference())
			SCRIPT_WRITE_DATA(protocol.data(), protocol.size())
			Script::NewObject(_pState, new SocketAddress(address));
			SCRIPT_WRITE_DATA(path.data(), path.size())
			SCRIPT_WRITE_VALUE(properties);
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_NEXT_READABLE)
				SCRIPT_AUTO_ERROR(LUASocketAddress::From(ex, _pState, SCRIPT_READ_NEXT(1), redirection));
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return redirection;
}


//// CLIENT_HANDLER /////
void MonaServer::onConnection(Exception& ex, Client& client, DataReader& inParams, DataWriter& outParams) {
	Service* pService = _pService->open(ex,client.path);
	if (!pService) {
		if (ex)
			ERROR(ex)
		return;
	}
	
	Script::AddObject(_pState, client);

	SCRIPT_BEGIN(_pState)
		SCRIPT_FUNCTION_BEGIN("onConnection", pService->reference())
			lua_pushvalue(_pState, 1); // client! (see Script::AddObject above)
			ScriptWriter writer(_pState);
			inParams.read(writer);
			SCRIPT_FUNCTION_CALL
			if (SCRIPT_FUNCTION_ERROR)
				ex.set<Ex::Application::Error>(SCRIPT_FUNCTION_ERROR);
			else
				SCRIPT_READ_NEXT(ScriptReader(_pState, SCRIPT_NEXT_READABLE).read(outParams));
		SCRIPT_FUNCTION_END
	SCRIPT_END

	lua_pop(_pState, 1); // remove Script::AddObject (see above)

	if (!ex) // connection accepted
		client.setCustomData(pService);
}

void MonaServer::onDisconnection(Client& client) {
	Service* pService = client.getCustomData<Service>();
	SCRIPT_BEGIN(_pState)
		SCRIPT_FUNCTION_BEGIN("onDisconnection", pService->reference())
			Script::FromObject(_pState, client);
			SCRIPT_FUNCTION_CALL
		SCRIPT_FUNCTION_END
	SCRIPT_END
	Script::RemoveObject(_pState,client);
}

void MonaServer::onAddressChanged(Client& client,const SocketAddress& oldAddress) {
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(client, "onAddressChanged")
			Script::NewObject(_pState, new SocketAddress(oldAddress));
			SCRIPT_FUNCTION_CALL
		SCRIPT_FUNCTION_END
	SCRIPT_END
}

bool MonaServer::onInvocation(Exception& ex, Client& client,const string& name,DataReader& reader, UInt8 responseType) {
	bool found(false);
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(client, name.empty() ? "onMessage" : name.c_str())
			ScriptWriter writer(_pState);
			reader.read(writer);
			found=true;
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR)
				ex.set<Ex::Application::Error>(SCRIPT_FUNCTION_ERROR);
			else
				SCRIPT_READ_NEXT(ScriptReader(_pState, SCRIPT_NEXT_READABLE).read(client.writer().writeResponse(responseType)));
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return found;
}

bool MonaServer::onFileAccess(Exception& ex, File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties, Client* pClient) {
	if (!pClient)
		return true; // intern access, publication recording for example!

	bool result(!mode);
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(*pClient, mode ? "onRead" : (mode==File::MODE_DELETE ? "onDelete" : "onWrite"))
			// file
			SCRIPT_WRITE_STRING(file.name()); // path can be refind with client.path + name!
			// arguments
			ScriptWriter writer(_pState);
			arguments.read(writer);
			SCRIPT_FUNCTION_CALL
			if (SCRIPT_FUNCTION_ERROR) {
				ex.set<Ex::Application::Error>(SCRIPT_FUNCTION_ERROR);
				result = false;
			} else {
				// first argument is "false/true/nil" OR "file name" redirection
				// next arguments are properties!
				ScriptReader reader(_pState, SCRIPT_NEXT_READABLE);
				if (!reader.readBoolean(result)) {
					if (!reader.readNull()) {
						// Redirect to the file (get name to prevent path insertion)
						string name;
						reader.readString(name);
						if (!file.setName(name))
							file.set(file.parent()); // redirect to folder view
					} else
						result = false;
				}
				SCRIPT_READ_NEXT(reader.position() + reader.read(properties));
			}
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return result;
}


//// PUBLICATION_HANDLER /////
bool MonaServer::onPublish(Exception& ex, Publication& publication, Client* pClient) {
	if (pClient == &Script::Client())
		return true; // mona::newPublication, already added!
	Script::Pop pop(_pState);
	if(!publication) // can be added by subscription!
		pop += Script::AddObject<Publication, Media::Source>(_pState, publication);
	if (!pClient)
		return true; // intern publication!
	bool result = true;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(*pClient, "onPublish")
			if(publication)
				Script::FromObject(_pState, publication);
			else
				lua_pushvalue(_pState, 1); // publication! (see Script::AddObject above)
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR) {
				ex.set<Ex::Application::Error>(SCRIPT_FUNCTION_ERROR);
				result = false;
			} else if(SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	if (!result && !publication)
		Script::RemoveObject<Publication, Media::Source>(_pState, publication);
	return result;
}

void MonaServer::onUnpublish(Publication& publication, Client* pClient) {
	if (pClient) { // else intern publication
		if (pClient == &Script::Client())
			return; // publication managed already by mona::newPublication
		SCRIPT_BEGIN(_pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(*pClient,"onUnpublish")
				Script::FromObject(_pState, publication);
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	}
	if (!publication)
		Script::RemoveObject<Publication, Media::Source>(_pState, publication);
}

bool MonaServer::onSubscribe(Exception& ex, Subscription& subscription, Publication& publication, Client* pClient) {
	Script::Pop pop(_pState);
	if(!publication && !(pop += Script::FromObject(_pState, publication) )) // test FromObject in the case where we are here with a mona::newSubscription inside a client::onPublish => publication already added! (no performance issue, happen just when !publication)
		Script::AddObject<Publication, Media::Source>(_pState, publication);
	if (!pClient || pClient == &Script::Client())
		return true; // intern publication or mona::newSubscription (already added)
	bool result = true;
	pop += Script::AddObject<Subscription, Media::Source>(_pState, subscription);
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(*pClient, "onSubscribe")
			if(publication)
				Script::FromObject(_pState, publication);
			else
				lua_pushvalue(_pState, 1); // publication! (see Script::AddObject above)
			lua_pushvalue(_pState, 2); // subscription! (see Script::AddObject above)
			SCRIPT_FUNCTION_CALL
			if (SCRIPT_FUNCTION_ERROR) {
				ex.set<Ex::Application::Error>(SCRIPT_FUNCTION_ERROR);
				result = false;
			} else if (SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	if (!result) {
		if(!publication)
			Script::RemoveObject<Publication, Media::Source>(_pState, publication);
		Script::RemoveObject<Subscription, Media::Source>(_pState, subscription);
	}
	return result;
}

void MonaServer::onUnsubscribe(Subscription& subscription, Publication& publication, Client* pClient) {
	if (pClient != &Script::Client()) { // else subscription managed already by mona::newPublication
		SCRIPT_BEGIN(_pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(*pClient, "onUnsubscribe")
				Script::FromObject(_pState, publication);
				Script::FromObject(_pState, subscription);
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
		Script::RemoveObject<Subscription, Media::Source>(_pState, subscription);
	}
	if (!publication)
		Script::RemoveObject<Publication, Media::Source>(_pState, publication);
}



} // namespace Mona

