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
#include "Mona/FileWatcher.h"


namespace Mona {

/*!
IOFile performs asynchrone writing and reading operation,
It uses a Thread::ProcessorCount() threads with low priority to load/read/write files
Indeed even if SSD drive allows parallel reading and writing operation every operation sollicate too the CPU,
so it's useless to try to exceeds number of CPU core (Thread::ProcessorCount() has been tested and approved with file load) */
struct IOFile : virtual Object, Thread { // Thread is for file watching!

	IOFile(const Handler& handler, const ThreadPool& threadPool, UInt16 cores=0);
	~IOFile();

	const Handler&	  handler;
	const ThreadPool& threadPool;

	/*!
	Subscribe read */
	template<typename FileType>
	void subscribe(const shared<FileType>& pFile, const File::OnReaden& onReaden, const File::OnError& onError) {
		pFile->_onError = onError;
		pFile->_onReaden = onReaden;
	}
	/*!
	Subscribe read with decoder*/
	template<typename FileType>
	void subscribe(const shared<FileType>& pFile, File::Decoder* pDecoder, const File::OnReaden& onReaden, const File::OnError& onError) {
		subscribe(pFile, onReaden, onError);
		pFile->_externDecoder = pDecoder && pFile.get() != (FileType*)pDecoder;
		pFile->_pDecoder = pDecoder;
	}
	/*!
	Subscribe write/delete */
	void subscribe(const shared<File>& pFile, const File::OnError& onError, const File::OnFlush& onFlush = nullptr);
	/*!
	Unsubscribe */
	template<typename FileType>
	void unsubscribe(shared<FileType>& pFile) {
		pFile->_onFlush = nullptr;
		pFile->_onReaden = nullptr;
		pFile->_onError = nullptr;
		pFile.reset();
	}
	/*!
	Async file loads */
	void load(const shared<File>& pFile);
	/*!
	Async read with file load if file not loaded
	size default = 0xFFFF (Best buffer performance, see http://zabkat.com/blog/buffered-disk-access.htm) */
	void read(const shared<File>& pFile, UInt32 size=0xFFFF);
	/*!
	Async write with file load if file not loaded */
	void write(const shared<File>& pFile, const Packet& packet);
	/*!
	Async file/folder deletion*/
	void erase(const shared<File>& pFile);
	/*!
	Async file/folder creation */
	void create(const shared<File>& pFile) { write(pFile, Packet::Null()); }
	/*!
	Async file watcher, watch until pFileWatcher becomes unique */
	void watch(const shared<const FileWatcher>& pFileWatcher, const FileWatcher::OnUpdate& onUpdate);

	void join();
private:
	bool run(Exception& ex, const volatile bool& requestStop);

	struct Action;
	struct WAction;
	struct SAction;


	ThreadPool								_threadPool; // Pool of threads for writing/reading disk operation
	std::vector<shared<const FileWatcher>>	_watchers;
	std::mutex								_mutexWatchers;
};


} // namespace Mona
