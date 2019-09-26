/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/


#pragma once

#include "Mona/Thread.h"
#include "Mona/Exceptions.h"
#include "Mona/Packet.h"
#include <functional>
#include <deque>

namespace Mona {


struct PersistentData : private Thread, virtual Object {
	PersistentData(const char* name = "PersistentData") : _disableTransaction(false), Thread(name) {}

	typedef std::function<void(const std::string& path, const Packet& packet)> ForEach;

	void load(Exception& ex, const std::string& rootDir, const ForEach& forEach, bool disableTransaction=false);

/*!
	Add an persitant data */
	void add(const char* path, const Packet& packet) { newEntry(path, packet); }
	void add(const std::string& path, const Packet& packet) { add(path.c_str(), packet); }

	void remove(const char* path) { newEntry(path); }
	void remove(const std::string& path) { remove(path.c_str()); }

	void clear() { newEntry(nullptr); }

	void flush() { stop(); }
	bool writing() { return running(); }

private:
	struct Entry : Packet, virtual public Object {
		Entry(const char* path, const Packet& packet) : path(path), Packet(std::move(packet)), clearing(false) {} // add
		Entry(const char* path) : clearing(path ? true : false) { if (!clearing) this->path.assign(path); } // remove or clear
	
		std::string	path;
		const bool	clearing;
	};

	template <typename ...Args>
	void newEntry(Args&&... args) {
		if (_disableTransaction)
			return;
		std::lock_guard<std::mutex> lock(_mutex);
		start(Thread::PRIORITY_LOWEST);
		_entries.emplace_back(SET, std::forward<Args>(args)...);
		wakeUp.set();
	}


	bool run(Exception& ex, const volatile bool& requestStop);
	void processEntry(Exception& ex, Entry& entry);
	bool loadDirectory(Exception& ex, const std::string& directory, const std::string& path, const ForEach& forEach);

	std::string							_rootPath;
	std::mutex							_mutex;
	std::deque<shared<Entry>>	_entries;
	bool								_disableTransaction;
};

} // namespace Mona
