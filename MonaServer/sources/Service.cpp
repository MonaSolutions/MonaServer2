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

#include "Service.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {



Service::Service(lua_State* pState, const string& wwwPath, Handler& handler) : _handler(handler), _wwwPath(wwwPath), _lastCheck(0), _reference(LUA_REFNIL), _pParent(NULL), _pState(pState), FileWatcher(wwwPath, "/main.lua") {
	init();
}

Service::Service(lua_State* pState, const string& wwwPath, Service& parent, const string& name) : _handler(parent._handler), _wwwPath(wwwPath), name(name), _lastCheck(0), _reference(LUA_REFNIL), _pParent(&parent), _pState(pState), FileWatcher(wwwPath,parent.path,'/',name,"/main.lua") {
	String::Assign((string&)path,parent.path,'/',name);
	init();
}

Service::~Service() {
	// clean children
	for (auto& it : _services)
		delete it.second;
	// clean environnment
	clearFile();
	// release reference
	luaL_unref(_pState, LUA_REGISTRYINDEX, _reference);
}

void Service::init() {
	//// create environment

	// table environment
	lua_newtable(_pState);

	// metatable
	Script::NewMetatable(_pState);

	// index
	lua_pushliteral(_pState, "__index");
	lua_newtable(_pState);

	// set name
	lua_pushliteral(_pState, "name");
	lua_pushlstring(_pState, name.data(), name.size());
	lua_rawset(_pState, -3);

	// set path
	lua_pushliteral(_pState, "path");
	lua_pushlstring(_pState, path.data(), path.size());
	lua_rawset(_pState, -3);

	// set this
	lua_pushliteral(_pState, "this");
	lua_pushvalue(_pState, -5);
	lua_rawset(_pState, -3);

	// metatable of index = parent or global (+ set super)
	Script::NewMetatable(_pState); // metatable
	lua_pushliteral(_pState, "__index");
	if (_pParent)
		lua_rawgeti(_pState, LUA_REGISTRYINDEX, _pParent->reference());
	else
		lua_pushvalue(_pState, LUA_GLOBALSINDEX);
	/// set super
	lua_pushliteral(_pState, "super");
	lua_pushvalue(_pState, -2);
	lua_rawset(_pState, -6);
	lua_rawset(_pState, -3); // set __index
	lua_setmetatable(_pState, -2);

	lua_rawset(_pState, -3); // set __index

	// set metatable
	lua_setmetatable(_pState, -2);

	// record in registry
	_reference = luaL_ref(_pState, LUA_REGISTRYINDEX);
}

Service* Service::open(Exception& ex) {
	if (_lastCheck.isElapsed(2000)) { // already checked there is less of 2 sec!
		_lastCheck.update();
		if (!watchFile() && !path.empty() && !FileSystem::Exists(file.parent())) // no path/main.lua file, no main service, no path folder
			_ex.set<Ex::Application::Unfound>("Application ", path, " doesn't exist");
	}
	if (!_ex)
		return this;
	ex = _ex;
	return NULL;
}

Service* Service::open(Exception& ex, const string& path) {
	// remove first '/'
	string name;
	if(!path.empty())
		name.assign(path[0] == '/' ? &path.c_str()[1] : path.c_str());

	// substr first "service"
	size_t pos = name.find('/');
	string nextPath;
	if (pos != string::npos) {
		nextPath = &name.c_str()[pos];
		name.resize(pos);
	}

	Service* pSubService(this);
	auto it = _services.end();
	if (!name.empty()) {
		it = _services.emplace(name, new Service(_pState, _wwwPath, self, name)).first;
		pSubService = it->second;
	}

	// if file or folder exists, return the service (or sub service)
	if (pSubService->open(ex)) {
		if (nextPath.empty())
			return pSubService;	
		return pSubService->open(ex, nextPath);
	}

	// service doesn't exist (and no children possible here!)
	if (it != _services.end() && ex.cast<Ex::Application::Unfound>()) {
		delete it->second;
		_services.erase(it);
	}
	return NULL;
}

void Service::loadFile() {
	_ex = nullptr;

	SCRIPT_BEGIN(_pState)
		lua_rawgeti(_pState, LUA_REGISTRYINDEX, _reference);
		if(luaL_loadfile(_pState,file.c_str())==0) {
			lua_pushvalue(_pState, -2);
			lua_setfenv(_pState, -2);
			if (lua_pcall(_pState, 0, 0, 0) == 0) {
				SCRIPT_FUNCTION_BEGIN("onStart", _reference)
					SCRIPT_WRITE_DATA(path.data(), path.size())
					SCRIPT_FUNCTION_CALL
				SCRIPT_FUNCTION_END
				SCRIPT_INFO("Application www", path, " loaded")
			} else
				SCRIPT_ERROR(_ex.set<Ex::Application::Invalid>(Script::LastError(_pState)));
		} else
			SCRIPT_ERROR(_ex.set<Ex::Application::Invalid>(Script::LastError(_pState)))
		lua_pop(_pState, 1); // remove environment
		if(_ex)
			clearFile();
	SCRIPT_END
}

void Service::clearFile() {

	if (!_ex) { // loaded!
		SCRIPT_BEGIN(_pState)
			SCRIPT_FUNCTION_BEGIN("onStop", _reference)
				SCRIPT_WRITE_DATA(path.data(), path.size())
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	} else
		_ex = nullptr;

	// update signal, after onStop because will disconnects clients
	_handler.onUpdate(self);

	// Clear environment
	/// clear environment table
	lua_rawgeti(_pState, LUA_REGISTRYINDEX, _reference);
	lua_pushnil(_pState);  // first key 
	while (lua_next(_pState, -2)) {
		// uses 'key' (at index -2) and 'value' (at index -1) 
		// remove the raw!
		lua_pushvalue(_pState, -2); // duplicate key
		lua_pushnil(_pState);
		lua_rawset(_pState, -5);
		lua_pop(_pState, 1);
	}
	/// clear index of metatable (used by few object like Timers, see LUATimer)
	lua_getmetatable(_pState, -1);
	int count = lua_objlen(_pState, -1);
	for (int i = 1; i <= count; ++i) {
		lua_pushnil(_pState);
		lua_rawseti(_pState, -2, i);
	}
	lua_pop(_pState, 2); // remove metatable + environment
	lua_gc(_pState, LUA_GCCOLLECT, 0); // collect garbage!
}



} // namespace Mona
