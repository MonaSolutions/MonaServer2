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



FileSystem

Developer notes:
- FileSystem must support just local path, no remoting path (//server/...), mainly because the operation could be blocking
- FileSystem must support ".." and "." but not "~" because on windows a file can be named "~" and because "stat" on linux and on windows doesn't resolve "~"
- A folder path ends with "/" and not a file
- FileSystem must support and resolve the multiple slash and anti-slahs

*/


#pragma once

#include "Mona/Mona.h"
#include "Mona/Time.h"
#include "Mona/Exceptions.h"
#include <vector>
#undef CreateDirectory

namespace Mona {

#define MAKE_FOLDER(PATH)		FileSystem::MakeFolder(PATH)
#define MAKE_FILE(PATH)			FileSystem::MakeFile(PATH)
#define MAKE_ABSOLUTE(PATH)		FileSystem::MakeAbsolute(PATH)
#define MAKE_RELATIVE(PATH)		FileSystem::MakeRelative(PATH)
#define RESOLVE(PATH)			FileSystem::Resolve(PATH)

struct FileSystem : virtual Static {
	enum Type {
		TYPE_FILE,
		TYPE_FOLDER,
	};

	enum Mode {
		MODE_LOW=0,
		MODE_HEAVY
	};


	struct Attributes : virtual Object {
		NULLABLE(!lastChange)
		Attributes() : size(0), lastChange(0), lastAccess(0) {}
		Time	lastChange;
		Time	lastAccess;
		UInt64	size;
		Attributes& reset() { lastAccess = 0;  lastChange = 0; size = 0; return *this; }
	};

	/*!
	File iteration, level is the deep sub directory level (when mode = MODE_HEAVY)
	If returns false it stops current directory iteration (but continue in sub directory if mode = MODE_HEAVY) */
	typedef std::function<bool(const std::string& file, UInt16 level)> ForEach;

	/// Iterate over files under directory path
	static int		ListFiles(Exception& ex, const std::string& path, const ForEach& forEach, Mode mode= MODE_LOW) { return ListFiles(ex, path.c_str(), forEach, mode); }
	static int		ListFiles(Exception& ex, const char* path, const ForEach& forEach, Mode mode= MODE_LOW);

	/// In giving a path with /, it tests one folder existance, otherwise file existance
	static bool			Exists(const std::string& path)  { return Exists(path.data(), path.size()); }
	static bool			Exists(const char* path) { return Exists(path, strlen(path)); }
	static bool			IsAbsolute(const std::string& path) { return IsAbsolute(path.c_str()); }
	static bool			IsAbsolute(const char* path);
	static bool			IsFolder(const std::string& path) { return IsFolder(path.c_str()); }
	static bool			IsFolder(const char* path);
	
	/*!
	End the path with a / if not already present */
	static std::string  MakeFolder(const char* path) { std::string result(path); MakeFolder(result); return result; }
	static std::string	MakeFolder(const std::string& path) { return MakeFolder(path.c_str()); }
	static std::string&	MakeFolder(std::string& path);
	/*!
	Remove end / if present (not guarantee that isFolder returns false now, if is "." for example it's a folder)  */
	static std::string	MakeFile(const char* path) { std::string result(path); MakeFile(result); return result; }
	static std::string	MakeFile(const std::string& path) { return MakeFile(path.c_str()); }
	static std::string&	MakeFile(std::string& path);
	static std::string	MakeAbsolute(const char* path) { std::string result(path); MakeAbsolute(result); return result; }
	static std::string	MakeAbsolute(const std::string& path) { return MakeAbsolute(path.c_str()); }
	static std::string& MakeAbsolute(std::string& path);
	static std::string	MakeRelative(const char* path) { std::string result(path); MakeRelative(result); return result; }
	static std::string	MakeRelative(const std::string& path) { return MakeRelative(path.c_str()); }
	static std::string& MakeRelative(std::string& path);
	static std::string	Resolve(const char* path) { std::string result(path); Resolve(result); return result; }
	static std::string	Resolve(const std::string& path) { return Resolve(path.c_str()); }
	static std::string& Resolve(std::string& path);


	/// extPos = position of ".ext"
	static Type		GetFile(const char* path, std::string& name) { std::size_t extPos; return GetFile(path,name,extPos); }
	static Type		GetFile(const char* path, std::string& name, std::size_t& extPos) { return GetFile(path, strlen(path),name,extPos); }
	static Type		GetFile(const char* path, std::string& name, std::string& parent) { std::size_t extPos; return GetFile(path,name,extPos,parent); }
	static Type		GetFile(const char* path, std::string& name, std::size_t& extPos, std::string& parent) { return GetFile(path, strlen(path), name, extPos, &parent); }
	
	static Type		GetFile(const std::string& path, std::string& name) { std::size_t extPos; return GetFile(path,name,extPos); }
	static Type		GetFile(const std::string& path, std::string& name, std::size_t& extPos) { return GetFile(path.data(), path.size(), name,extPos); }
	static Type		GetFile(const std::string& path, std::string& name, std::string& parent) { std::size_t extPos;  return GetFile(path,name, extPos, parent); }
	static Type		GetFile(const std::string& path, std::string& name, std::size_t& extPos, std::string& parent) { return GetFile(path.data(), path.size() , name, extPos, &parent); }
	

	static std::string& GetName(std::string& value) { return GetName(value,value); }
	static std::string& GetName(const char* path, std::string& value);
	static std::string& GetName(const std::string& path, std::string& value) { return GetName(path.c_str(),value); }
	static std::string& GetBaseName(std::string& value) { return GetBaseName(value,value); }
	static std::string& GetBaseName(const char* path, std::string& value);
	static std::string& GetBaseName(const std::string& path, std::string& value) { return GetBaseName(path.c_str(),value); }
	static std::string& GetExtension(std::string& value) { return GetExtension(value,value); }
	static std::string& GetExtension(const char* path, std::string& value);
	static std::string& GetExtension(const std::string& path, std::string& value) { return GetExtension(path.c_str(),value); }
	static std::string& GetParent(std::string& value) { return GetParent(value,value); }
	static std::string& GetParent(const char* path, std::string& value)  { return GetParent(path,strlen(path),value); }
	static std::string& GetParent(const std::string& path, std::string& value)  { return GetParent(path.data(),path.size(),value); }

	static Attributes&	GetAttributes(const std::string& path, Attributes& attributes) { return GetAttributes(path.data(),path.size(),attributes); }
	static Attributes&	GetAttributes(const char* path, Attributes& attributes) { return GetAttributes(path, strlen(path),attributes); }

	static UInt64		GetSize(Exception& ex,const char* path, UInt64 defaultValue=0) { return GetSize(ex, path, strlen(path), defaultValue); }
	static UInt64		GetSize(Exception& ex, const std::string& path, UInt64 defaultValue=0) { return GetSize(ex, path.data(), path.size(), defaultValue); }
	static Time&		GetLastModified(Exception& ex,const std::string& path, Time& time) { return GetLastModified(ex, path.data(),path.size(), time); }
	static Time&		GetLastModified(Exception& ex, const char* path, Time& time) { return GetLastModified(ex, path,strlen(path), time); }

	static const char*	GetHome(const char* defaultPath = NULL) { static Home Path; return Path ? Path.c_str() : defaultPath; }
	static bool			GetHome(std::string& path) { return GetHome() ? (path.assign(GetHome()),true) : false; }
	static const char*	GetCurrentApp(const char* defaultPath = NULL) { static CurrentApp Path;  return Path ? Path.c_str() : defaultPath; }
	static bool			GetCurrentApp(std::string& path) { return GetCurrentApp() ? (path.assign(GetCurrentApp()), true) : false; }
	static const char*	GetCurrentDir() { return GetCurrentDirs().back().c_str(); }

	static bool			CreateDirectory(Exception& ex, const std::string& path, Mode mode = MODE_LOW) { return CreateDirectory(ex, path.data(),path.size(),mode); }
	static bool			CreateDirectory(Exception& ex, const char* path, Mode mode = MODE_LOW) { return CreateDirectory(ex, path,strlen(path),mode); }
	static bool			Rename(const std::string& fromPath, const std::string& toPath) { return Rename(fromPath.c_str(), toPath.c_str()); }
	static bool			Rename(const std::string& fromPath, const char* toPath) { return Rename(fromPath.c_str(), toPath); }
	static bool			Rename(const char* fromPath, const std::string& toPath) { return Rename(fromPath, toPath.c_str()); }
	static bool			Rename(const char* fromPath, const char* toPath);
	static bool			Delete(Exception& ex, const std::string& path, Mode mode = MODE_LOW) { return Delete(ex, path.data(), path.size(), mode); }
	static bool			Delete(Exception& ex, const char* path, Mode mode = MODE_LOW) { return Delete(ex, path, strlen(path), mode); }
	
	static bool			Find(std::string& path);

private:
	struct Home : std::string, virtual Object {
		NULLABLE(empty())
		Home();
	};

	struct Directory : std::string, virtual Object {
		Directory(const std::string& value) : std::string(value) {}
		Directory(const char* value) : std::string(value) {}
		Directory(Directory&&		 other) : std::string(std::move(other)), name(std::move(other.name)), extPos(other.extPos) {}
		std::string name;
		size_t      extPos;
	};

	struct CurrentDirs : std::vector<Directory>, virtual Object {
		CurrentDirs();
	};

	struct CurrentApp : std::string, virtual Object {
		NULLABLE(empty())
		CurrentApp();
	};

	static CurrentDirs& GetCurrentDirs() { static CurrentDirs Dirs; return Dirs; }

	static Attributes& GetAttributes(const char* path, std::size_t size, Attributes& attributes);
	static bool Exists(const char* path, std::size_t size);
	static bool CreateDirectory(Exception& ex, const char* path, std::size_t size, Mode mode);
	static bool Delete(Exception& ex, const char* path, std::size_t size, Mode mode);
	static Time& GetLastModified(Exception& ex, const char* path, std::size_t size, Time& time);
	static UInt64 GetSize(Exception& ex, const char* path, std::size_t size, UInt64 defaultValue);
	static std::string& GetParent(const char* path, std::size_t size, std::string& value);
	static const char*  GetFile(const char* path, std::size_t& size, std::size_t& extPos, Type& type, Int32& parentPos);
	static Type			GetFile(const char* path, std::size_t size, std::string& name, std::size_t& extPos, std::string* pParent = NULL);
};


} // namespace Mona
