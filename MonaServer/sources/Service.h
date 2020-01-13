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

#include "Mona/Client.h"
#include "Script.h"
#include "Mona/FileWatcher.h"
#include "Mona/Path.h"


namespace Mona {

struct Service {
	struct Handler {
		virtual void onStop(Service& service) = 0;
	};
	Service(lua_State* pState, const std::string& www, Handler& handler, IOFile& ioFile);
	Service(lua_State* pState, Service& parent, const char* name);
	virtual ~Service();

	int					reference() const { return _reference; }

	const Path			path;
	
	Service*			get(Exception& ex, const std::string& path) { return get(ex, path.c_str()); }
	Service*			get(Exception& ex, const char* path);


private:
	void		init();

	Service&	open(const char* path);
	Service*	close(const char* path);

	void		update();

	void		start();
	void		stop();

	
	
	FileWatcher::OnUpdate	_onUpdate;

	Exception				_ex; // is set if application has not a valid main.lua started!
	int						_reference;
	Service*				_pParent;
	lua_State*				_pState;
	bool					_started;
	Path					_file;



	std::map<std::string, Service>	_services;
	Handler&						_handler;
	shared<const FileWatcher>		_pWatcher;
};

} // namespace Mona
