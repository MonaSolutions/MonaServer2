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

/*!
Help tool to Write/Delete file in a asynchronous way (can be done also in using directly IOFile + File)
It can be used too for an asynchronous folder deletion in using erase method */
struct FileWriter : virtual Object {
	typedef File::OnFlush	 ON(Flush);
	typedef File::OnError	 ON(Error);
	NULLABLE(!_pFile)
	
	FileWriter(IOFile& io) : io(io) {}
	~FileWriter() { close(); }

	IOFile&	io;

	UInt64	queueing() const { return _pFile ? _pFile->queueing() : 0; }

	File* operator->() const { return _pFile.get(); }
	File& operator*() { return *_pFile; }

	bool opened() const { return operator bool(); }

	/*!
	Async open file to write, return *this to allow a open(path).write(...) call
	/!\ don't open really the file, because performance are better if opened on first write operation */
	FileWriter& open(const Path& path, bool append = false) {
		close();
		_pFile.set(path, append ? File::MODE_APPEND : File::MODE_WRITE);
		io.subscribe(_pFile, onError, onFlush);
		return self;
	}
	/*!
	Write data, if queueing wait onFlush event for large data transfer*/
	void		write(const Packet& packet) { DEBUG_ASSERT(_pFile);  io.write(_pFile, packet); }
	void		erase() { DEBUG_ASSERT(_pFile); io.erase(_pFile); }
	void		close() { if(_pFile) io.unsubscribe(_pFile); }

	static void	Erase(const Path& path, IOFile& io) { io.erase(std::make_shared<File>(path, File::MODE_DELETE)); }

private:
	shared<File> _pFile;
};

} // namespace Mona
