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

#include "Mona/FileWatcher.h"


namespace Mona {

using namespace std;

FileWatcher::FileWatcher(const Path& path, FileSystem::Mode mode) : path(path), mode(mode), _firstWatch(true) {
	_baseName = path.baseName() == "*" ? NULL : path.baseName().c_str();
	_ext = path.extension().empty() ? NULL : path.extension().c_str();
	_justFolder = path.isFolder();
}

int FileWatcher::watch(Exception &ex, const OnUpdate& onUpdate) {
	map<Path, pair<Time, bool>, String::IComparator> lastChanges = move(_lastChanges);

	UInt32 count = 0;
	if (mode == FileSystem::MODE_HEAVY || !_baseName || (_ext && *_ext == '*')) {

		// List files/folders from parent folder!
		FileSystem::ForEach forEach([this, &onUpdate, &count, &lastChanges](const string& file, UInt16 level) {
			Path path(file);
			if (_justFolder && !path.isFolder())
				return true; // just folders!
			if (_ext) { // just files!
				if (path.isFolder())
					return true;
				if (*_ext != '*' && String::ICompare(path.extension(), _ext) != 0)
					return true; // don't match *.extension!
			}
			if (_baseName && String::ICompare(path.baseName(), _baseName) != 0)
				return true; // don't match *.extension!
			++count;
			watchFile(lastChanges, path, onUpdate);
			return !_baseName || (_ext && *_ext == '*');
		});
		int result = FileSystem::ListFiles(ex, path.parent(), forEach, mode) < 0 ? -1 : count;
		if (result < 0 && !ex)
			ex.set<Ex::Intern>();
		
		// these files have been deleted, signal it!
		for (const auto& it : lastChanges) {
			++count;
			// test exists to refresh "it.first" and can happen that file is recreated at this moment so ignore deletion a new watching will update again this file
			if (!it.first.exists(true)) 
				onUpdate(it.first, false);
		}
	} else { // Watch a simple file or folder!
		watchFile(lastChanges, path, onUpdate);
		++count;
	}
	_firstWatch = false;
	return ex ? -1 : count;

}

void FileWatcher::watchFile(map<Path, pair<Time, bool>, String::IComparator>& lastChanges, const Path& path, const OnUpdate& onUpdate) {
	// if path doesn't exist path.lastChange()==0
	pair<Time, bool>& lastChange = _lastChanges.emplace(path, pair<Time, bool>(0, false)).first->second;
	const auto& itChange = lastChanges.find(path);
	if (itChange != lastChanges.end()) {
		lastChange = move(itChange->second);
		lastChanges.erase(itChange);
	}
	if (path.lastChange(true) != lastChange.first) {
		lastChange.first = path.lastChange();
		if((lastChange.second = _firstWatch)) // no update signal immediatly
			onUpdate(path, true);
	} else if (!lastChange.second) {
		lastChange.second = true; // update signal done, wait stability before to update to anticipate progressive file replacement (download for example)
		onUpdate(path, false);
	}
}

} // namespace Mona
