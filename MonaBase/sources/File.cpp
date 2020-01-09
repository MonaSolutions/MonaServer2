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
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#define INVALID_HANDLE_VALUE -1
#include <unistd.h>
#if defined(_BSD) && !defined(lseek64) // not defined on 64 bit systems
	#define lseek64 lseek
	#define off64_t off_t
#endif
#endif




using namespace std;

namespace Mona {

File::File(const Path& path, Mode mode) : _flushing(0), _loaded(false), _pDecoder(NULL),
	_written(0), _readen(0), _path(path), mode(mode), _decodingTrack(0),
	_queueing(0), _ioTrack(0), _handle(INVALID_HANDLE_VALUE), _externDecoder(false) {
}

File::~File() {
	if (_externDecoder) {
		_pDecoder->onRelease(self);
		delete _pDecoder;
	}
	// No CPU expensive
	if (_handle == INVALID_HANDLE_VALUE)
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
	if (_loaded)
		return true;
	if (!_path) {
		ex.set<Ex::Intern>("Empty path can not be opened");
		return false;
	}
	if (_path.isFolder()) {
		ex.set<Ex::Intern>("Cannot load a ", _path, " folder");
		return false;
	}
	if (mode == MODE_DELETE) {
		ex.set<Ex::Permission>(_path, " load unauthorized in delete mode");
		return false;
	}
	// file READ, WRITE or APPEND
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
	
	_handle = CreateFileW(wFile, mode ? GENERIC_WRITE : GENERIC_READ, mode ? FILE_SHARE_READ : (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, flags, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (_handle != INVALID_HANDLE_VALUE) {
		if(mode==File::MODE_APPEND)
			SetFilePointer((HANDLE)_handle, 0, NULL, FILE_END);
		LARGE_INTEGER size;
		GetFileSizeEx((HANDLE)_handle, &size);
		FILETIME access;
		FILETIME change;
		GetFileTime((HANDLE)_handle, NULL, &access, &change);
		ULARGE_INTEGER ulAccess;
		ulAccess.LowPart = access.dwLowDateTime;
		ulAccess.HighPart = access.dwHighDateTime;
		ULARGE_INTEGER ulChange;
		ulChange.LowPart = change.dwLowDateTime;
		ulChange.HighPart = change.dwHighDateTime;
		// round to seconds precision to get same precision than with stat::
		_path._pImpl->setAttributes(size.QuadPart, (ulAccess.QuadPart / 10000000ll - 11644473600ll) * 1000ll, (ulChange.QuadPart / 10000000ll - 11644473600ll) * 1000ll);
		_loaded = true;
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
	_handle = ::open(_path.c_str(), flags, S_IRWXU);
	while (_handle != INVALID_HANDLE_VALUE) {
		if (mode && flock(_handle, LOCK_EX | LOCK_NB) != 0) { // exclusive write!
			// fail to lock!
			::close(_handle);
			_handle = INVALID_HANDLE_VALUE;
			break;
		}
#if !defined(__APPLE__) && !defined(__ANDROID__)
		posix_fadvise(_handle, 0, 0, 1);  // ADVICE_SEQUENTIAL
#endif
		struct stat status;
		::fstat(_handle, &status);
		_path._pImpl->setAttributes(status.st_mode&S_IFDIR ? 0 : (UInt64)status.st_size, status.st_atime * 1000ll, status.st_mtime * 1000ll);
		_loaded = true;
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
	if (!_loaded)
		return _path.size(refresh);

	if (!refresh)
		return _path.size(false); // more faster because already loaded (setAttributes has been called in load)

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

void File::reset(UInt64 position) {
	if(!_loaded)
		return;
	_readen = position;
#if defined(_WIN32)
	LARGE_INTEGER offset;
	offset.QuadPart = -(LONGLONG)_written.exchange(position) + position; // move relating APPEND possible mode!
	SetFilePointerEx((HANDLE)_handle, offset, NULL, FILE_CURRENT);
#else
	lseek64(_handle, -(off64_t )_written.exchange(position) + position, SEEK_CUR);
#endif
}

int File::read(Exception& ex, void* data, UInt32 size) {
	if (_path.isFolder()) {
		ex.set<Ex::Intern>("Cannot read data from a ", _path, " folder");
		return false;
	}
	if (!load(ex))
		return -1;
	if (mode) {
		ex.set<Ex::Permission>(_path, " read unauthorized in writing, append or deletion mode");
		return -1;
	}
#if defined(_WIN32)
	DWORD readen;
	if (!ReadFile((HANDLE)_handle, data, size, &readen, NULL))
		readen = -1;
#else
	ssize_t readen = ::read(_handle, data, size);
#endif
	if (readen < 0) {
		ex.set<Ex::System::File>("Impossible to read ", _path, " (size=", size, ")");
		return -1;
	}
	_readen += readen;
	return int(readen);
}

bool File::write(Exception& ex, const void* data, UInt32 size) {
	if (_path.isFolder()) {
		if (size)
			ex.set<Ex::Intern>("Cannot write data to a ", _path, " folder");
		return FileSystem::CreateDirectory(ex, _path);
	}
	if (!load(ex))
		return false;
	if (!mode || mode > MODE_APPEND) {
		ex.set<Ex::Permission>(_path, " write unauthorized in reading or deletion mode");
		return false;
	}
	if (!size)
		return true; // nothing todo!
#if defined(_WIN32)
	DWORD written;
	if (!WriteFile((HANDLE)_handle, data, size, &written, NULL))
		written = 0;
#else
	ssize_t written = ::write(_handle, data, size);
#endif
	if (written <= 0) {
		ex.set<Ex::System::File>("Impossible to write ", _path, " (size=", size, ")");
		return false;
	}
	_written += written;
	if (UInt32(written) < size) {
		ex.set<Ex::System::File>("No more disk space to write ", _path, " (size=", size, ")");
		return false;
	}
	return true;
}

bool File::erase(Exception& ex) {
	if (mode != MODE_DELETE && mode != MODE_WRITE) {
		ex.set<Ex::Permission>(_path, " deletion unauthorized in reading or append mode");
		return false;
	}
	if (!FileSystem::Delete(ex, _path))
		return false;
	if (_loaded) {
		_readen = 0;
		_written = 0;
	}
	_path._pImpl->setAttributes(0, 0, 0); // update attributes (no exists!)
	return true;
}

} // namespace Mona
