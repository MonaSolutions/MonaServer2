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

struct FileReader : virtual NullableObject {
	typedef File::OnReaden	 ON(Readen);
	typedef File::OnError	 ON(Error);

	FileReader(IOFile& io) : io(io) {}
	~FileReader() { close(); }

	IOFile&	io;

	File* operator->() const { return _pFile.get(); }
	explicit operator bool() const { return _pFile.operator bool(); }
	bool opened() const { return operator bool(); }

	/*!
	Async open file to read, return *this to allow a open(path).read(...) right */
	FileReader& open(const Path& path) { close(); io.open(_pFile, path, newDecoder(), onReaden, onError); return *this; }
	/*!
	Sync open file to read, usefull when call from Thread (already threaded) */
	bool		open(Exception& ex, const Path& path) { close(); return io.open(ex, _pFile, path, newDecoder(), onReaden, onError); }
	/*!
	Read data */
	void		read(UInt32 size = 0xFFFF) { io.read(_pFile, size); }
	void		close() { if (_pFile) io.close(_pFile); }

private:
	virtual shared<File::Decoder> newDecoder() { return nullptr; }

	shared<File> _pFile;
};

} // namespace Mona
