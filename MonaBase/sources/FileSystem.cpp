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

#include "Mona/Util.h"
#include "Mona/FileSystem.h"
#include "Mona/Path.h"
#include <sys/stat.h>
#include <cctype>
#if defined(_WIN32)
#include "direct.h"
#else
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_BSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#include "dirent.h"
#include <unistd.h>
#include "limits.h"
#include "pwd.h"
#endif


using namespace std;

namespace Mona {


FileSystem::Home::Home() {
#if defined(_WIN32)
	// windows

	const char* path(Util::Environment().getString("HOMEPATH"));
	if (path) {
		if (!Util::Environment().getString("HOMEDRIVE", *this) && !Util::Environment().getString("SystemDrive", *this))
			assign("C:");
		append(path);
	} else if (!Util::Environment().getString("USERPROFILE", *this)) {
		clear();
		return;
	}
#else
	struct passwd* pwd = getpwuid(getuid());
	if (pwd)
		assign(pwd->pw_dir);
	else {
		pwd = getpwuid(geteuid());
		if (pwd)
			assign(pwd->pw_dir);
		else if(!Util::Environment().getString("HOME", *this)) {
			clear();
			return;
		}
	}
#endif
	
	MakeFolder(self);
}


FileSystem::CurrentApp::CurrentApp() {
// http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
	resize(PATH_MAX);
#ifdef _WIN32
	int n = GetModuleFileNameA(0, &(*this)[0], PATH_MAX);
#elif defined(__APPLE__)
	uint32_t n(PATH_MAX);
	if (_NSGetExecutablePath(&(*this)[0], &n) < 0)
		n = 0;
#elif defined(_BSD)
	ssize_t n = readlink("/proc/curproc/file", &(*this)[0], PATH_MAX); 
	if (n<=0) {
		n = readlink("/proc/curproc/exe", &(*this)[0], PATH_MAX); 
		if(n<=0) {
			n = PATH_MAX;
			int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
			if(sysctl(mib, 4, &(*this)[0], (size_t*)&n, NULL, 0)<0)
				n = 0;
		}
	}
#else
	// read the link target into variable linkTarget
	ssize_t n(readlink("/proc/self/exe", &(*this)[0], PATH_MAX)); 
#endif
	resize(n < 0 ? 0 : n);
}



FileSystem::CurrentDirs::CurrentDirs() {
	string current;
	int n(0);
	current.resize(PATH_MAX);
#if defined(_WIN32)
	char* test(&current[0]);
	n = GetCurrentDirectoryA(PATH_MAX, test);
	if (n < 0 || n > PATH_MAX)
		n = 0;
#else
	if (getcwd(&current[0], PATH_MAX)) {
		n = strlen(current.c_str());
		emplace_back("/");
	}
#endif
	current.resize(n);
	if (n == 0) {
		// try with parrent of CurrentApp
		if (GetCurrentApp())
			GetParent(GetCurrentApp(), current);
		else // else with Home
			current = GetHome();
	}

	String::ForEach forEach([this](UInt32 index,const char* value){
		Directory dir(empty() ? String::Empty() : back());
		dir.append(value).append("/");
		dir.name.assign(value);
		dir.extPos = dir.name.find_last_of('.');
		emplace_back(move(dir));
		return true;
	});
	String::Split(current, "/\\", forEach, SPLIT_IGNORE_EMPTY);
	if (empty())
		emplace_back("/"); // if nothing other possible, set current directory as root (must be absolute)
}


#if defined(_WIN32)
typedef struct __stat64 Status;
#else
typedef struct stat  Status;
#endif


static Int8 Stat(const char* path, size_t size, Status& status) {

	bool isFolder;
	if (size) {
		char c(path[size-1]);
		isFolder = (c=='/' || c=='\\');
	} else
		isFolder = true;

	bool found;

#if defined (_WIN32)
	// windows doesn't accept blackslash in _wstat
	wchar_t wFile[PATH_MAX+2];
	size = MultiByteToWideChar(CP_UTF8, 0, path, size, wFile, PATH_MAX);
	if (isFolder)
		wFile[size++] = '.';
	wFile[size] = 0;
	found = _wstat64(wFile, &status)==0;
#else
	found = ::stat(size ? path : ".", &status)==0;
#endif
	if (!found)
		return 0;
	return status.st_mode&S_IFDIR ? (isFolder ? 1 : -1) : (isFolder ? -1 : 1);
}


FileSystem::Attributes& FileSystem::GetAttributes(const char* path, size_t size, Attributes& attributes) {
	Status status;
	if (Stat(path, size, status) <= 0)
		return attributes.reset();
	attributes.lastChange = status.st_mtime * 1000ll;
	attributes.lastAccess = status.st_atime * 1000ll;
	attributes.size = status.st_mode&S_IFDIR ? 0 : status.st_size;
	return attributes;
}

Time& FileSystem::GetLastModified(Exception& ex, const char* path, size_t size, Time& time) {
	Status status;
	status.st_mtime = time/1000;
	if (Stat(path, size, status)<=0) {
		ex.set<Ex::System::File>(path, " doesn't exist");
		return time;
	}
	time = status.st_mtime*1000ll;
	return time;
}

UInt64 FileSystem::GetSize(Exception& ex,const char* path, size_t size, UInt64 defaultValue) {
	Status status;
	if (Stat(path, size, status)<=0) {
		ex.set<Ex::System::File>(path, " doesn't exist");
		return defaultValue;
	}
	if (status.st_mode&S_IFDIR) { // if was a folder
		ex.set<Ex::System::File>("GetSize works just on file, and ", path, " is a folder");
		return defaultValue;
	}
	return (UInt64)status.st_size;
}

bool FileSystem::Exists(const char* path, size_t size) {
	Status status;
	return Stat(path, size, status)>0;
}

bool FileSystem::Rename(const char* fromPath, const char* toPath) {
	#if defined(_WIN32)
		wchar_t wFrom[PATH_MAX];
		MultiByteToWideChar(CP_UTF8, 0, fromPath, -1, wFrom, sizeof(wFrom));
		wchar_t wTo[PATH_MAX];
		MultiByteToWideChar(CP_UTF8, 0, toPath, -1, wTo, sizeof(wTo));
		return _wrename(wFrom, wTo) == 0;
	#else
		return rename(fromPath, toPath) == 0;
	#endif
}

bool FileSystem::CreateDirectory(Exception& ex, const char* path, size_t size, Mode mode) {
	Status status;
	if (Stat(path, size, status)) {
		if (status.st_mode&S_IFDIR)
			return true;
		ex.set<Ex::System::File>("Cannot create directory ",path," because a file with this path exists");
		return false;
	}

	if (mode==MODE_HEAVY) {
		// try to create the parent (recursive)
		string parent;
		GetParent(path,parent);
		if (parent.compare(path) && !CreateDirectory(ex, parent, MODE_HEAVY))
			return false;
	}

#if defined(_WIN32)
	wchar_t wFile[PATH_MAX];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wFile, sizeof(wFile));
	if (_wmkdir(wFile) == 0)
		return true;
#else
    if (mkdir(path,S_IRWXU | S_IRWXG | S_IRWXO)==0)
		return true;
#endif
	ex.set<Ex::System::File>("Cannot create directory ",path);
	return false;
}

bool FileSystem::Delete(Exception& ex, const char* path, size_t size, Mode mode) {
	Status status;
	if (!Stat(path, size, status))
		return true; // already deleted

	if (status.st_mode&S_IFDIR) {
		if (!size)
			path = ".";
		if (mode==MODE_HEAVY) {
			FileSystem::ForEach forEach([&ex](const string& path, UInt16 level) {
				Delete(ex, path, MODE_HEAVY);
				return true;
			});
			Exception ignore;
			ListFiles(ignore, path, forEach);
			if (ignore)
				return true;; // if exception it's a not existent folder
			if (ex) // impossible to remove a sub file/folder, so the parent folder can't be removed too (keep the exact exception)
				return false;
		}
	}
#if defined(_WIN32)
	wchar_t wFile[PATH_MAX];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wFile, sizeof(wFile));
	if (status.st_mode&S_IFDIR) {
		if (RemoveDirectoryW(wFile) || GetLastError() == ERROR_FILE_NOT_FOUND)
			return true;
		ex.set<Ex::System::File>("Impossible to remove folder ", path);
	} else {
		if (DeleteFileW(wFile) || GetLastError()==ERROR_FILE_NOT_FOUND)
			return true;
		ex.set<Ex::System::File>("Impossible to remove file ", path);
	}
#else
	if(status.st_mode&S_IFDIR) {
		if(rmdir(path)==0 || errno==ENOENT)
			return true;
		ex.set<Ex::System::File>("Impossible to remove folder ", path);
	} else {
		if(unlink(path)==0 || errno==ENOENT)
			return true;
		ex.set<Ex::System::File>("Impossible to remove file ", path);
	}
#endif
	return false;
}

int FileSystem::ListFiles(Exception& ex, const char* path, const ForEach& forEach, Mode mode) {
	string directory(path);
	MakeFolder(directory);
	UInt32 count(0);
	string file;
	bool iterate = true;

#if defined(_WIN32)
	wchar_t wDirectory[PATH_MAX+2];
	wDirectory[MultiByteToWideChar(CP_UTF8, 0, directory.data(), directory.size(), wDirectory, PATH_MAX)] = '*';
	wDirectory[directory.size()+1] = 0;
	
	WIN32_FIND_DATAW fileData;
	HANDLE	fileHandle = FindFirstFileExW(wDirectory, FindExInfoBasic, &fileData, FindExSearchNameMatch, NULL, 0);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		if (GetLastError() != ERROR_NO_MORE_FILES)
			ex.set<Ex::System::File>("Cannot list files of directory ", directory);
		return -1;
	}
	do {
		if (wcscmp(fileData.cFileName, L".") != 0 && wcscmp(fileData.cFileName, L"..") != 0) {
			++count;
			String::Assign(file, directory, fileData.cFileName);
			if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				file += '/';
				if (mode)
					count += ListFiles(ex, file, forEach, Mode(mode+1));
			}
			if (iterate && !forEach(file, mode ? (UInt16(mode) - 1) : 0))
				iterate = false;
		}
	} while (FindNextFileW(fileHandle, &fileData) != 0);
	FindClose(fileHandle);
#else
	DIR* pDirectory = opendir(directory.c_str());
	if (!pDirectory) {
		ex.set<Ex::System::File>("Cannot list files of directory ",directory);
		return -1;
	}
	struct dirent* pEntry(NULL);
	while((pEntry = readdir(pDirectory))) {
		if (strcmp(pEntry->d_name, ".")!=0 && strcmp(pEntry->d_name, "..")!=0) {
			++count;
			String::Assign(file, directory, pEntry->d_name);
			// Cross-platform solution when DT_UNKNOWN or symbolic link
			bool isFolder(false);
			if(pEntry->d_type==DT_DIR) {
				isFolder = true;
			} else if(pEntry->d_type==DT_UNKNOWN || pEntry->d_type==DT_LNK) {
				Status status;
				Stat(file.data(), file.size(), status);
				if ((status.st_mode&S_IFMT) == S_IFDIR)
					isFolder = true;
			}
			if(isFolder) {
				file += '/';
				if (mode)
					count += ListFiles(ex, file, forEach, Mode(mode+1));
			}
			if (iterate && !forEach(file, mode ? (UInt16(mode) - 1) : 0))
				iterate = false;
		}
	}
	closedir(pDirectory);
#endif
	return count;
}

const char* FileSystem::GetFile(const char* path, size_t& size, size_t& extPos, Type& type, Int32& parentPos) {
	const char* cur(path + size);
	size = 0;
	bool   scanDots(true);
	UInt16 level(1);
	extPos = string::npos;
	bool firstChar(true);
	type = TYPE_FOLDER;

	while (cur-- > path) {
		if (*cur == '/' || *cur == '\\') {
			if (firstChar) {
				type = TYPE_FOLDER;
				firstChar = false;
			}
			if (size) {
				if (scanDots)
					level += UInt16(size);
				else {
					if (!level)
						break;
					scanDots = true;
					extPos = string::npos;
				}
				size = 0;
			}
			continue;
		}

		if (firstChar) {
			type = TYPE_FILE;
			firstChar = false;
		}

		if (!scanDots && level)
			continue;
		if (!size++)
			--level;

		if (extPos==string::npos && *cur == '.') {
			if (scanDots) {
				if (size < 3)
					continue;
				extPos = 1;
			} else
				extPos = size;
		}
		scanDots = false;
	}

	if (scanDots) // nothing or . or .. or something with backslash (...../)
		level += UInt16(size);

	if (level) {
		size = 0; // no name!
		if (FileSystem::IsAbsolute(path)) {
#if defined(_WIN32)
			parentPos = isalpha(*path) ? 3 : 1; // C:/ or /
#else
			parentPos = 1; // /
#endif	
			return path+parentPos;
		}
		
		parentPos = -level;
		return NULL; // level!
	}
#if defined(_WIN32)
	else if (cur < path && size == 2 && type== TYPE_FOLDER && path[1] == ':' && isalpha(*path)) {
		parentPos = 3; // C:/
		size = 0; // no name!
		return path+3;
	}
#endif
	
	const char* name(++cur);
	if (extPos!=string::npos)
		extPos = size - extPos;	

	UInt8 offset(1);
	while (--cur >= path && (*cur == '/' || *cur == '\\'))
		offset = 2;

	parentPos = cur - path + offset;
	return name;
}


FileSystem::Type FileSystem::GetFile(const char* path, size_t size, string& name, size_t& extPos, string* pParent) {
	
	Type type;
	Int32 parentPos;
	const char* file(GetFile(path, size, extPos, type, parentPos));
	if (file)
		name.assign(file, size);
	else
		name.clear();

	if (pParent) {
		// parent is -level!
		if (parentPos <= 0) {
			pParent->clear();
			while (parentPos++ < 0)
				pParent->append("../");
		} else 
			pParent->assign(path, 0, parentPos);
	}
	return type;
}


string& FileSystem::GetName(const char* path, string& value) {
	GetFile(path,value);
	return value;
}

string& FileSystem::GetBaseName(const char* path, string& value) {
	size_t extPos;
	GetFile(path,value,extPos);
	if (extPos!=string::npos)
		value.erase(extPos);
	return value;
}

string& FileSystem::GetExtension(const char* path, string& value) {
	size_t extPos;
	GetFile(path,value,extPos);
	if (extPos == string::npos)
		value.clear();
	else
		value.erase(0, extPos+1);
	return value;
}

string& FileSystem::GetParent(const char* path, size_t size, string& value) {
	Type type;
	Int32 parentPos;
	size_t extPos;
	GetFile(path, size, extPos, type, parentPos);
	if (parentPos > 0)
		return value.assign(path, 0, parentPos);
	// parent is -level!
	value.clear();
	while (parentPos++ < 0)
		value.append("../");
	return value;
}

string& FileSystem::Resolve(string& path) {
	Type type(TYPE_FOLDER);
	size_t extPos;
	string newPath;
	size_t size;
	Int32 parentPos;
	const char* file;

	CurrentDirs& dirs(GetCurrentDirs());

	do {
		if (type == TYPE_FILE)
			path += '.';
		file = GetFile(path.data(),size = path.size(), extPos, type,parentPos);

		if (parentPos <= 0) {
			parentPos += dirs.size()-1;
			if (parentPos<0)
				parentPos = 0;
			if (UInt32(++parentPos) < dirs.size()) {
				newPath.insert(0, dirs[parentPos]);
				if (type == TYPE_FILE)
					MakeFile(newPath);
			} else {
				if (!newPath.empty())
					++size; // file contains necessary a "/" in the end to add
				else if (type == TYPE_FOLDER)
					newPath += '/';
				newPath.insert(0, file, size).insert(0, dirs[--parentPos]);
			}
			return path = move(newPath);
		}
		if (size) {
			if(!newPath.empty())
				newPath.insert(0, "/");
			newPath.insert(0, file, size);
		}
		path.resize(parentPos);
	} while (size);

	if (!newPath.empty()) {
		if (type == TYPE_FOLDER)
			newPath += '/';
	} else if(type == TYPE_FILE)
		newPath += '.';
	return path = move(newPath.insert(0, path));
}

bool FileSystem::IsAbsolute(const char* path) {
	if (path[0] == '/')
		return true; // because _stat("/file") for windows or linux search on the current disk file as a absolute path
#if defined(_WIN32)
	return isalpha(path[0]) && path[1]==':' && (path[2]=='/' || path[2]=='\\'); // "C:" is a file (relative path)
#else
	return false;
#endif
}

string& FileSystem::MakeAbsolute(string& path) {
	if (IsAbsolute(path))
		return path;
	return path.insert(0, "/");
}
string& FileSystem::MakeRelative(string& path) {
	UInt32 count(0);
	while (path[count] == '/' || path[count] == '\\')
		++count;
	path.erase(0, count);
#if defined(_WIN32)
	// /c: => ./c:
	// /c:/path => ./c:/path
	// c:/path => ./c:/path
	if (isalpha(path[0]) && path[1] == ':') {
		if(count || path[2] == '/' || path[2] == '\\')
			path.insert(0, "./");
	}
#endif
	return path;
}

bool FileSystem::Find(string& path) {
	const char* paths = Util::Environment().getString("PATH");
	if (!paths)
		return false;

	string temp;
	String::ForEach forEach([&path,&temp](UInt32 index,const char* value) {
		temp.assign(value);
#if defined(_WIN32)
			// "path" => path
			if (!temp.empty() && temp.front() == '"' && temp.back() == '"') {
				temp.erase(temp.front());
				temp.resize(temp.size()-1);
			}
#endif
		if (Exists(MakeFolder(temp).append(path))) {
			path = move(temp);
			return false;
		}
		return true;
	});
#if defined(_WIN32)
	return String::Split(paths, ";", forEach) == string::npos;
#else
	return String::Split(paths, ":", forEach) == string::npos;
#endif
}

bool FileSystem::IsFolder(const char* path) {
	size_t size = strlen(path);
	if (!size--)
		return true; // empty is folder
	if (path[size] == '/' || path[size] == '\\')
		return true; // end with '/' is folder
	// can be folder here just if end with dot '.'
	if (path[size] != '.') {
#if defined(_WIN32)
		// C: is a folder!
		if (size == 1 && path[1] == ':' && isalpha(path[0]))
			return true;
#endif
		return false;
	}
	if (!size)
		return true; // '.' is folder
	if (path[size-1] == '/' || path[size-1] == '\\')
		return true; // '/.' is folder
	if (!--size || path[size-1] == '/' || path[size-1] == '\\')
		return path[size] == '.'; // '..' or '/..' is folder
	return false;
}
	

string& FileSystem::MakeFolder(string& path) {
#if defined(_WIN32)
	// C: can stay identical => concatenation with a file will give a relative file!
	if (path.size() == 2 && isalpha(path[0]) && path[1] == ':')
		return path;
#endif
	if (!path.empty() && path.back() != '/' && path.back() != '\\')
		path += '/';
	return path;
}

string& FileSystem::MakeFile(string& path) {
	if (path.empty())
		return path.assign("."); // the concatenation with a file rest "relative" to current folder!
	UInt32 size = path.size();
	while (size-- && (path[size] == '/' || path[size] == '\\'));
	path.resize(size+1);
	return path;
}


} // namespace Mona
