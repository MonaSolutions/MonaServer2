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

#include "Mona/File.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(_WIN32)
#include "windows.h"
#else
#include <unistd.h>
#endif

using namespace std;

namespace Mona {

File::File(const Path& path, Mode mode) : _path(path), mode(mode), _decodingTrack(0), _pDevice(NULL), _queueing(0), _loading(0), _loadingTrack(0), _handle(-1) {
}

File::~File() {
	// No CPU expensive
	if (_handle == -1)
		return;
#if defined(_WIN32)
	CloseHandle((HANDLE)_handle);
#else
	::close(_handle);
#endif
}

UInt64 File::queueing() const {
	UInt64 queueing(_queueing);
	// superior to buffer 0xFFFF to limit onFlush usage!
	return queueing > 0xFFFF ? queueing - 0xFFFF : 0;
}

bool File::load(Exception& ex) {
	if (_handle!=-1)
		return true;
	if (!_path) {
		ex.set<Ex::Intern>("Empty path can not be opened");
		return false;
	}
	if (_path.isFolder()) {
		ex.set<Ex::Intern>(_path, " is a directory, can not be opened");
		return false;
	}
#if defined(_WIN32)
	wchar_t wFile[PATH_MAX];
	MultiByteToWideChar(CP_UTF8, 0, _path.c_str(), -1, wFile, sizeof(wFile));
	DWORD flags;
	if (mode) {
		if (mode == MODE_WRITE)
			flags = CREATE_ALWAYS;
		else
			flags = OPEN_ALWAYS; // append
	} else
		flags = OPEN_EXISTING;
	
	_handle = (long)CreateFileW(wFile, mode ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flags, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (_handle != -1) {
		if(mode==File::MODE_APPEND)
			SetFilePointer((HANDLE)_handle, 0, NULL, FILE_END);
		LARGE_INTEGER size;
		GetFileSizeEx((HANDLE)_handle, &size);
		FILETIME time;
		GetFileTime((HANDLE)_handle, NULL, NULL, &time);
		ULARGE_INTEGER ull;
		ull.LowPart = time.dwLowDateTime;
		ull.HighPart = time.dwHighDateTime;
		_path._pImpl->setAttributes(size.QuadPart, ull.QuadPart / 10000000ULL - 11644473600ULL, _path.isAbsolute() ? _path[0] : Path::CurrentDir()[0]);
		return true;
	}
		

#else
	int flags;
	if (mode) {
		flags = O_WRONLY | O_CREAT;
		if (mode == MODE_WRITE)
			flags |= O_TRUNC;
		else
			flags |= O_APPEND;
	} else
		flags = O_RDONLY;
	_handle = open(_path.c_str(), flags);
	if (_handle != -1) {
		posix_fadvise(_handle, 0, 0, 1);  // ADVICE_SEQUENTIAL
		struct stat status;
		::fstat(_handle, &status);
		_path._pImpl->setAttributes(status.st_mode&S_IFDIR ? 0 : (UInt64)status.st_size, status.st_mtime * 1000ll, UInt8(major(status.st_dev)));
		return true;
	}
#endif
	if (mode) {
		if(_path.exists(true))
			ex.set<Ex::Permission>("Impossible to open ", _path, " file to write");
		else
			ex.set<Ex::Permission>("Impossible to create ", _path, " file to write");
	} else {
		if (_path.exists())
			ex.set<Ex::Permission>("Impossible to open ", _path, " file to read");
		else
			ex.set<Ex::Unfound>("Impossible to find ", _path, " file to read");
	}
	return false;
}

UInt64 File::size(bool refresh) const {
	if (_handle==-1)
		return _path.size(refresh);
#if defined(_WIN32)
	LARGE_INTEGER current;
	LARGE_INTEGER size;
	size.QuadPart = 0;
	if (SetFilePointerEx((HANDLE)_handle, size, &current, FILE_CURRENT)) {
		BOOL success = SetFilePointerEx((HANDLE)_handle, size, &size, SEEK_END);
		SetFilePointerEx((HANDLE)_handle, current, NULL, FILE_BEGIN);
		if (success)
			return size.QuadPart;
	}
#else
	Int64 current = lseek64(_handle, 0, SEEK_CUR);
	if(current>=0) {
		Int64 size = lseek64(_handle, 0, SEEK_END);
		lseek64(_handle, current, SEEK_SET);
		if (size >= 0)
			return size;
	}
#endif
	return _path.size(refresh);
}

void File::reset() {
	if(_handle == -1)
		return;
#if defined(_WIN32)
	SetFilePointer((HANDLE)_handle, 0, NULL, FILE_BEGIN);
#else
	lseek(_handle, 0, SEEK_SET);
#endif
}

int File::read(Exception& ex, void* data, UInt32 size) {
	if (!load(ex))
		return -1;
	if (mode) {
		ex.set<Ex::Intern>("Impossible to read ", _path, " opened in writing mode");
		return -1;
	}
#if defined(_WIN32)
	DWORD readen;
	if (!ReadFile((HANDLE)_handle, data, size, &readen, NULL))
		readen = -1;
#else
	ssize_t readen = ::read(_handle, data, size);
#endif
	if (readen >= 0)
		return int(readen);
	ex.set<Ex::System::File>("Impossible to read ", _path," (size=",size,")");
	return -1;
}

bool File::write(Exception& ex, const void* data, UInt32 size) {
	if (!load(ex))
		return false;
	if (!mode) {
		ex.set<Ex::Intern>("Impossible to write ", _path, " opened in reading mode");
		return false;
	}
#if defined(_WIN32)
	DWORD written;
	if (!WriteFile((HANDLE)_handle, data, size, &written, NULL))
		written = -1;
#else
	ssize_t written = ::write(_handle, data, size);
#endif
	if (written == -1) {
		ex.set<Ex::System::File>("Impossible to write ", _path, " (size=", size, ")");
		return false;
	}
	if (UInt32(written) < size) {
		ex.set<Ex::System::File>("No more disk space to write ", _path, " (size=", size, ")");
		return false;
	}
	return true;
}

} // namespace Mona
