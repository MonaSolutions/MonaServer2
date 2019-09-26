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

struct Service : private FileWatcher {
	struct Handler {
		virtual void onUpdate(Service& service) {}
	};
	Service(lua_State* pState, const std::string& wwwPath, Handler& handler);
	virtual ~Service();

	int					reference() const { return _reference; }

	Service*			open(Exception& ex);
	Service*			open(Exception& ex, const std::string& path);


	const std::string				name;
	const std::string				path;

private:
	Service(lua_State* pState,  const std::string& wwwPath, Service& parent, const std::string& name);

	void		init();

	// FileWatcher implementation
	void		loadFile();
	void		clearFile();
	

	Exception				_ex;
	int						_reference;
	Service*				_pParent;
	lua_State*				_pState;
	Time					_lastCheck;


	std::map<std::string,Service*>	_services;
	const std::string&				_wwwPath;
	Handler&						_handler;
};

} // namespace Mona
