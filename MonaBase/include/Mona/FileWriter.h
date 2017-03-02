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
#include "Mona/IOFile.h"

namespace Mona {

struct FileWriter : virtual NullableObject {
	typedef File::OnFlush	 ON(Flush);
	typedef File::OnError	 ON(Error);
	
	FileWriter(IOFile& io) : io(io) {}
	~FileWriter() { close(); }

	IOFile&	io;

	UInt64	queueing() const { return _pFile ? _pFile->queueing() : 0; }

	File* operator->() const { return _pFile.get(); }
	explicit operator bool() const { return _pFile.operator bool(); }
	bool opened() const { return operator bool(); }

	/*!
	Async open file to write, return *this to allow a open(path).read(...) right */
	FileWriter& open(const Path& path, bool append=false) { close();  io.open(_pFile, path, onFlush, onError, append); return *this; }
	/*!
	Sync open file to write, usefull when call from Thread (already threaded) */
	bool		open(Exception& ex, const Path& path, bool append = false) { close();  return io.open(ex, _pFile, path, onFlush, onError, append); }

	/*!
	Write data, if queueing wait onFlush event for large data transfer*/
	void		write(const Packet& packet) { io.write(_pFile, packet); }
	void		close() { if (_pFile) io.close(_pFile); }

private:
	shared<File> _pFile;
};

} // namespace Mona
