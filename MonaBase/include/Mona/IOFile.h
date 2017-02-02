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
#include "Mona/File.h"
#include "Mona/ThreadPool.h"
#include "Mona/Packet.h"
#include "Mona/Handler.h"

namespace Mona {

/*!
IOFile performs asynchrone writing and reading operation */
struct IOFile : virtual Object {

	IOFile(const Handler& handler, ThreadPool& threadPool) : handler(handler), threadPool(threadPool) {}
	~IOFile() { join(); }

	const Handler&		handler;
	const ThreadPool&	threadPool;

	/*!
	Async open file to read */
	void open(shared<File>& pFile, const Path& path, const File::OnReaden& onReaden, const File::OnError& onError) { shared<File::Decoder> pDecoder;  open(pFile, path, std::move(pDecoder), onReaden, onError); }
	void open(shared<File>& pFile, const Path& path, shared<File::Decoder>&& pDecoder, const File::OnReaden& onReaden, const File::OnError& onError);
	/*!
	Sync open file to read, usefull when call from Thread (already threaded) */
	bool open(Exception& ex, shared<File>& pFile, const Path& path, const File::OnReaden& onReaden, const File::OnError& onError) { shared<File::Decoder> pDecoder; return open(ex, pFile, path, std::move(pDecoder), onReaden, onError); }
	bool open(Exception& ex, shared<File>& pFile, const Path& path, shared<File::Decoder>&& pDecoder, const File::OnReaden& onReaden, const File::OnError& onError);
	
	/*!
	Async open file to write */
	void open(shared<File>& pFile, const Path& path, const File::OnError& onError, bool append = false) { open(pFile, path, nullptr, onError, append); }
	void open(shared<File>& pFile, const Path& path, const File::OnFlush& onFlush, const File::OnError& onError, bool append = false);
	/*!
	Sync open file to write, usefull when call from Thread (already threaded) */
	bool open(Exception& ex, shared<File>& pFile, const Path& path, const File::OnError& onError, bool append = false) { return open(ex, pFile, path, nullptr, onError, append); }
	bool open(Exception& ex, shared<File>& pFile, const Path& path, const File::OnFlush& onFlush, const File::OnError& onError, bool append = false);

	/*!
	size default = 0xFFFF (Best buffer performance, see http://zabkat.com/blog/buffered-disk-access.htm) */
	void read(const shared<File>& pFile, UInt32 size=0xFFFF);
	void write(const shared<File>& pFile, const Packet& packet);
	/*!
	Close */
	void close(shared<File>& pFile);

	void join();
private:
	struct Action;
	struct OpenFile;
	
	void dispatch(File& file, const shared<Action>& pAction);
};


} // namespace Mona
