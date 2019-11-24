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

struct FileReader : virtual Object {
	typedef File::OnReaden	 ON(Readen);
	typedef File::OnError	 ON(Error);
	NULLABLE(!_pFile)

	FileReader(IOFile& io) : io(io) {}
	~FileReader() { close(); }

	IOFile&	io;

	File* operator->() const { return _pFile.get(); }
	File& operator*() { return *_pFile; }

	bool opened() const { return operator bool(); }

	/*!
	Async open file to read, return *this to allow a open(path).read(...) call 
	/!\ don't open really the file, because performance are better if opened on first read operation */
	FileReader& open(const Path& path) {
		close();
		_pFile.set(path, File::MODE_READ);
		io.subscribe(_pFile, newDecoder(), onReaden, onError);
		return self;
	}
	/*!
	Read data */
	void		read(UInt32 size = 0xFFFF) { DEBUG_ASSERT(_pFile);  io.read(_pFile, size); }
	void		close() { if (_pFile) io.unsubscribe(_pFile); }

private:
	virtual File::Decoder* newDecoder() { return NULL; }

	shared<File> _pFile;
};

} // namespace Mona
