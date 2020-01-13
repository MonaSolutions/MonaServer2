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



Service::Service(lua_State* pState, const string& www, Handler& handler, IOFile& ioFile) : path(""),
	_handler(handler), _reference(LUA_REFNIL), _pParent(NULL), _pState(pState), _started(false),
	_pWatcher(SET, Path(www, "*"), FileSystem::MODE_HEAVY), _file(www, "/main.lua"),
	_onUpdate([this, &www](const Path& file, bool firstWatch) {
		if(!file.isFolder() && file.name() != "main.lua")
			return;
		// file path is a concatenation of _wwwPath given to ListFiles in FileWatcher and file name, so we can get relative file name!
		const char* path = (file.isFolder() ? file.c_str() : file.parent().c_str()) + www.size();
		Service* pService;
		if (file.exists()) {
			DEBUG("Application ", file, " load");
			pService = &open(path);
			if (file.isFolder())
				pService = NULL; // no children update required here!
			else
				pService->start();
		} else {
			DEBUG("Application ", file, " deletion");
			if (!file.isFolder()) {
				Exception ex;
				pService = get(ex, path);
				if (pService)
					pService->stop();
			} else
				pService = close(path);
		}
		// contamine reload to children application!
		if (pService && !firstWatch) {
			for (auto& it : pService->_services)
				it.second.update();
		}
	}) {
	ioFile.watch(_pWatcher, _onUpdate);
	init();
}

Service::Service(lua_State* pState, Service& parent, const char* name) : path(parent.path.parent(), name),
	_handler(parent._handler), _reference(LUA_REFNIL), _pParent(&parent), _pState(pState),
	_file(parent._file.parent(), name, "/main.lua") {
	init();
}

Service::~Service() {
	// clean environnment
	stop();
	// release reference
	luaL_unref(_pState, LUA_REGISTRYINDEX, _reference);
}

Service* Service::get(Exception& ex, const char* path) {
	if (*path == '/')
		++path; // remove first '/'
	if (!*path)
		return (ex = _ex) ? NULL : this;
	const char* subPath = strchr(path, '/');
	string name(path, subPath ? (subPath - path) : strlen(path));
	const auto& it = _services.find(name);
	if (it != _services.end())
		return it->second.get(ex, subPath ? subPath : "");
	ex.set<Ex::Unavailable>("Application ", this->path, '/', name, " doesn't exist");
	return NULL;
}
Service& Service::open(const char* path) {
	if (!*path)
		return self;
	const char* subPath = path;
	while (*subPath != '/' && *subPath)
		++subPath;
	// don't use "emplace" to avoid systematic Service creation/deletion (init + stop!)
	String::Scoped scoped(subPath);
	auto it = _services.lower_bound(path);
	if (it == _services.end() || path != it->first)
		it = _services.emplace_hint(it, SET, forward_as_tuple(path), forward_as_tuple(_pState, self, path));
	return scoped ? it->second.open(subPath+1) : it->second;
}
Service* Service::close(const char* path) {
	if (*path == '/')
		++path; // remove first '/'
	if (!*path)
		return _services.empty() ? NULL : this;
	const char* subPath = strchr(path, '/');
	const auto& it = _services.find(string(path, subPath ? (subPath - path) : strlen(path)));
	if (it == _services.end())
		return NULL;
	Service* pService = it->second.close(subPath ? subPath : "");
	if(!pService)
		_services.erase(it);
	return pService;
}

void Service::update() {
	if(_started)
		start(); // restart!
	for (auto& it : _services)
		it.second.update();
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

	// set path
	lua_pushliteral(_pState, "path");
	Script::NewObject(_pState, new const Path(path));
	lua_rawset(_pState, -3);
	lua_pushliteral(_pState, "__tostring");
	lua_pushlstring(_pState, path.c_str(), path.length());
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

void Service::start() {
	stop();

	_ex = nullptr;

	SCRIPT_BEGIN(_pState)
		lua_rawgeti(_pState, LUA_REGISTRYINDEX, _reference);
		int error = luaL_loadfile(_pState, _file.c_str());
		if(!error) {
			lua_pushvalue(_pState, -2);
			lua_setfenv(_pState, -2);
			if (lua_pcall(_pState, 0, 0, 0) == 0) {
				_started = true;
				SCRIPT_FUNCTION_BEGIN("onStart", _reference)
					SCRIPT_WRITE_STRING(path)
					SCRIPT_FUNCTION_CALL
				SCRIPT_FUNCTION_END
				INFO("Application www", path, " started")
			} else
				SCRIPT_ERROR(_ex.set<Ex::Application::Error>(Script::LastError(_pState)));
		} else {
			const char* message = Script::LastError(_pState); // always pop the error message of stack!
			if (error != LUA_ERRFILE) // else file doesn't exist anymore (can happen on "update()" when a parent directory is deleted)
				SCRIPT_ERROR(_ex.set<Ex::Application::Invalid>(message));
		}
		lua_pop(_pState, 1); // remove environment
		if(_ex)
			stop(); // to release possible ressource loaded by luaL_loadfile
	SCRIPT_END
}

void Service::stop() {

	if (_started) {
		_started = false;
		SCRIPT_BEGIN(_pState)
			SCRIPT_FUNCTION_BEGIN("onStop", _reference)
				SCRIPT_WRITE_STRING(path)
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
		INFO("Application www", path, " stopped");

		// update signal, after onStop because will disconnects clients
		_handler.onStop(self);
	}

	// Clear environment (always do, super can assign value on this app even if empty)
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
