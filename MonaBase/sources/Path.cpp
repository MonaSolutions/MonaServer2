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

#include "Mona/Path.h"

using namespace std;

namespace Mona {

void Path::Impl::init() {
	size_t extPos;
	_type = FileSystem::GetFile(_path, _name, extPos, _parent);
	_baseName.assign(_name, 0, extPos);
	if (extPos != string::npos)
		_extension.assign(_name, extPos + 1, string::npos);
	else
		_extension.clear();
	_isAbsolute = FileSystem::IsAbsolute(_path);
}

const FileSystem::Attributes& Path::Impl::attributes(bool refresh) const {
	if (!_attributesLoaded || refresh) {
		FileSystem::GetAttributes(_path, _attributes);
		_attributesLoaded = true;
	}
	return _attributes;
}

void Path::Impl::setAttributes(UInt64 size, Int64 lastAccess, Int64 lastChange) {
	lock_guard<mutex> lock(_mutex);
	_attributesLoaded = true;
	_attributes.size = size;
	_attributes.lastChange = lastChange;
	_attributes.lastAccess = lastAccess;
}

bool Path::setName(const char* value) {

	const char* name(strrpbrk(value, "/\\"));
	if (name)
		value = name + 1;

	// forbid . or .. or empty name
	if (!*value || strcmp(value, ".") == 0 || strcmp(value, "..") == 0)
		return false;

	String path(parent(), value);
	if (isFolder())
		FileSystem::MakeFolder(path);
	_pImpl.set(move(path));
	return true;
}

bool Path::setBaseName(const char* value) {

	const char* baseName(strrpbrk(value, "/\\"));
	if (baseName)
		value = baseName + 1;

	const string& extension(this->extension());
	// forbid . or .. or empty name
	if ((!*value && (extension.empty() || extension == ".")) || (strcmp(value, ".") == 0 && extension.empty()))
		return false;

	String path(parent(), value, '.', extension);
	if (isFolder())
		FileSystem::MakeFolder(path);
	_pImpl.set(move(path));
	return true;
}

bool Path::setExtension(const char* value) {

	const char* extension(strrpbrk(value, "/.\\"));
	if (extension)
		value = extension + 1;

	const string& baseName(this->baseName());
	// forbid . or .. or empty name
	if ((!*value && (baseName.empty() || baseName == ".")) || (strcmp(value, ".") == 0 && baseName.empty()))
		return false;

	String path(parent(), baseName, '.', value);
	if (isFolder())
		FileSystem::MakeFolder(path);
	_pImpl.set(move(path));
	return true;
}

} // namespace Mona
