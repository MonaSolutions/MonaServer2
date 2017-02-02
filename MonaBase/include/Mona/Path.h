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
#include "Mona/FileSystem.h"
#include <mutex>

namespace Mona {

/*!
Powerfull feature to manipulate a path to File or Folder
/!\ No move or copy constructor because a shared<Path> usage is a lot more faster */
struct Path : virtual NullableObject {
	CONST_STRING(_pImpl ? _pImpl->path() : String::Empty());

	Path() {}
	/*!
	Build a path */
	template <typename ...Args>
	Path(Args&&... args) : _pImpl(new Impl(std::forward<Args>(args)...)) {}
	Path(const Path& path) : _pImpl(path._pImpl) {}
	Path(Path&& path) : _pImpl(std::move(path._pImpl)) {}

	Path& operator=(const Path& path) { _pImpl = path._pImpl; return *this; }
	Path& operator=(Path&& path) { _pImpl = std::move(path._pImpl); return *this; }
	Path& operator=(std::nullptr_t) { return reset(); }

	explicit operator bool() const { return _pImpl ? true : false; }
	
	// properties
	const std::string&	name() const { return _pImpl ? _pImpl->name() : String::Empty(); }
	const std::string&	baseName() const { return _pImpl ? _pImpl->baseName() : String::Empty(); }
	const std::string&	extension() const { return _pImpl ? _pImpl->extension() : String::Empty(); }
	const std::string&  parent() const { return _pImpl ? _pImpl->parent() : String::Empty(); }
	bool				isFolder() const { return _pImpl ? _pImpl->isFolder() : false; }
	bool				isAbsolute() const { return _pImpl ? _pImpl->isAbsolute() : false; }


	// physical disk file
	bool		exists(bool refresh = false) const { return _pImpl ? _pImpl->exists(refresh) : false; }
	UInt64		size(bool refresh = false) const { return _pImpl ? _pImpl->size(refresh) : false; }
	Int64		lastModified(bool refresh = false) const { return _pImpl ? _pImpl->lastModified(refresh) : false; }
	UInt8		device() const { return _pImpl ? _pImpl->device() : false; }

	// setters
	template <typename ...Args>
	Path& set(Args&&... args) { _pImpl.reset(new Impl(std::forward<Args>(args)...)); return *this; }
	bool setName(const std::string& value) { return setName(value.c_str()); }
	bool setName(const char* value);
	bool setBaseName(const std::string& value) { return setBaseName(value.c_str()); }
	bool setBaseName(const char* value);
	bool setExtension(const std::string& value) { return setExtension(value.c_str()); }
	bool setExtension(const char* value);

	Path& reset() { _pImpl.reset(); return *this; }

	static const Path& Home() { static Path Path(FileSystem::GetHome()); return Path; }
	static const Path& CurrentApp() { static Path Path(FileSystem::GetCurrentApp()); return Path; }
	static const Path& CurrentDir() { static Path Path(FileSystem::GetCurrentDir()); return Path; }
	static const Path& Null() { static Path Null; return Null; }
private:

	struct Impl : virtual Object {

		template <typename ...Args>
		Impl(Args&&... args) : _resolved(false), _attributesLoaded(false) {
			String::Assign(_path, std::forward<Args>(args)...);
			init();
		}

		// properties
		const std::string& path() const { return _path; }
		const std::string& name() const { return _name; }
		const std::string& baseName() const { return _baseName; }
		const std::string& extension() const { return _extension; }
		const std::string& parent() const { return _parent; }
		bool			   isFolder() const { return _type == FileSystem::TYPE_FOLDER; }
		bool			   isAbsolute() const { return _isAbsolute; }
		bool			   resolved() const { return _resolved; }


		bool	exists(bool refresh) const { std::lock_guard<std::mutex> lock(_mutex); return attributes(refresh); }
		UInt64	size(bool refresh) const { std::lock_guard<std::mutex> lock(_mutex); return attributes(refresh).size; }
		Int64	lastModified(bool refresh) const { std::lock_guard<std::mutex> lock(_mutex); return attributes(refresh).lastModified; }
		UInt8	device() const { std::lock_guard<std::mutex> lock(_mutex); return attributes(false).device; }

		void setAttributes(UInt64 size, Int64 lastModified, UInt8 device);
	private:
		void init();
		const FileSystem::Attributes& attributes(bool refresh) const;

		std::string						_path;
		std::string						_name;
		std::string						_baseName;
		std::string						_extension;
		std::string						_parent;
		FileSystem::Type				_type;
		bool							_isAbsolute;
		bool							_resolved;

		mutable std::mutex				_mutex;
		mutable FileSystem::Attributes	_attributes;
		mutable bool					_attributesLoaded;
	};


	shared<Impl> _pImpl;

	friend struct File;
};


} // namespace Mona
