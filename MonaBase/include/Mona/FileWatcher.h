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

#include "Mona/Mona.h"
#include "Mona/Path.h"
#include "Mona/Event.h"
#include "Mona/FileSystem.h"


namespace Mona {

/*!
Wath update files
If path is a file or folder in MODE_LOW, it's watching simply this fileor folder.
If path is a file or folder in MODE_HEAVY, it's watching file and folder with this name in all directory and sub directory
If path has the form \directory\*, it's watching all files/folder in directory (and sub directory in MODE_HEAVY)
If path has the form \directory\*.*, it's watching all files in directory (and sub directory in MODE_HEAVY)
If path has the form \directory\*.ext, it's watching all files with ext in directory (and sub directory in MODE_HEAVY)
If path has the form \directory\name.*, it's watching all files with name in directory (and sub directory in MODE_HEAVY)
If path has the form \directory\* /, it's watching all folders in directory (and sub drectory in MODE_HEAVY) */
struct FileWatcher : virtual Object {
	typedef Event<void(const Path& file, bool firstWatch)>	OnUpdate;

	FileWatcher(const Path& path, FileSystem::Mode mode = FileSystem::MODE_LOW);

	const FileSystem::Mode	mode;
	const Path				path;

	/*!
	Check file updates in path, path  */
	int		watch(Exception& ex, const OnUpdate& onUpdate);


private:
	void	watchFile(std::map<Path, std::pair<Time, bool>, String::IComparator>& lastChanges, const Path& file, const OnUpdate& onUpdate);

	std::map<Path, std::pair<Time, bool>, String::IComparator> _lastChanges;
	bool													   _firstWatch;

	const char* _ext;
	const char* _baseName;
	bool		_justFolder;

	//// Used by IOFile /////////////////////
	OnUpdate    onUpdate;
	friend struct IOFile;
};

} // namespace Mona
