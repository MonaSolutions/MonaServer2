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

#include "Mona/IOFile.h"

using namespace std;

namespace Mona {

struct IOFile::Action : Runner, virtual Object {
	Action(const char* name, const Handler& handler, const shared<File>& pFile) : handler(handler), _pFile(pFile), Runner(name) {}

	struct Handle : Runner, virtual Object {
		Handle(const char* name, const shared<File>& pFile) : Runner(name), _pFile(pFile) {}
	private:
		bool run(Exception& ex) {
			if (!_pFile.unique())
				handle(*_pFile);
			return true;
		}
		virtual void handle(File& file) = 0;
		shared<File>	_pFile;
	};

protected:
	template<typename HandleType, typename ...Args>
	void handle(Args&&... args) { if(!_pFile.unique()) handler.queue(new HandleType(name, _pFile, forward<Args>(args)...)); }

	const Handler&	handler;
private:
	bool run(Exception& ex) {
		if (run(ex, _pFile))
			return true;
		struct ErrorHandle : Handle, virtual Object {
			ErrorHandle(const char* name, const shared<File>& pFile, Exception& ex) : Handle(name, pFile), _ex(move(ex)) {}
		private:
			void handle(File& file) { file.onError(_ex); }
			Exception		_ex;
		};
		handle<ErrorHandle>(ex);
		return true;
	}
	virtual bool run(Exception& ex, const shared<File>& pFile) { return pFile->load(ex); }

	shared<File> _pFile; // shared and not weak to allow to detect current reading/writing process to avoid to recall ioFile.read/write if it's useless!
};

IOFile::IOFile(const Handler& handler, const ThreadPool& threadPool, UInt16 cores) :
	handler(handler), threadPool(threadPool), _threadPool(Thread::PRIORITY_LOW, cores*2) { // 2*CPU => because disk speed can be at maximum 2x more than memory, and Low priority to not impact main thread pool
}

IOFile::~IOFile() {
	join();
}

void IOFile::join() {
	// join devices (reading and writing operation)	
	do {
		((ThreadPool&)threadPool).join(); // wait possible decoding (can cast because IOFile constructor takes a non-const threadPool object)
	} while(_threadPool.join()); // while reading/writing operation
}

void IOFile::subscribe(const shared<File>& pFile, const File::OnError& onError, const File::OnFlush& onFlush) {
	pFile->onError = onError;
	if ((pFile->onFlush = onFlush) && (pFile->mode==File::MODE_WRITE|| pFile->mode == File::MODE_APPEND))
		onFlush(false); // can start write operation immediatly!
}

void IOFile::load(const shared<File>& pFile) {
	if (!*pFile)
		_threadPool.queue(new Action("LoadFile", handler, pFile), pFile->_ioTrack);
}

void IOFile::read(const shared<File>& pFile, UInt32 size) {
	struct ReadFile : Action {
		ReadFile(const Handler& handler, const shared<File>& pFile, const ThreadPool& threadPool, UInt32 size) : Action("ReadFile", handler, pFile), _threadPool(threadPool), _size(size) {}
	private:
		struct Handle : Action::Handle, virtual Object {
			Handle(const char* name, const shared<File>& pFile, shared<Buffer>& pBuffer, bool end) :
				Action::Handle(name, pFile), _pBuffer(move(pBuffer)), _end(end) {}
		private:
			void handle(File& file) { file.onReaden(_pBuffer, _end); }
			shared<Buffer>	_pBuffer;
			bool   _end;
		};
		bool run(Exception& ex, const shared<File>& pFile) {
			if (pFile.unique())
				return true; // useless to read here, nobody to receive it!
			// take the required size just if not exceeds file size to avoid to allocate a too big buffer (expensive)
			// + use pFile->size() without refreshing to use as same size as caller has gotten it (for example to write a content-length in header)
			UInt64 available = pFile->size() - pFile->readen();
			shared<Buffer>	pBuffer(new Buffer(UInt32(min(available, _size))));
			int readen = pFile->read(ex, pBuffer->data(), pBuffer->size());
			if (readen < 0)
				return false;
			if ((_size=readen) < pBuffer->size())
				pBuffer->resize(readen, true);
			if (pFile->pDecoder) {
				struct Decoding : Action {
					Decoding(const Handler& handler, const shared<File>& pFile, const ThreadPool& threadPool, shared<Buffer>& pBuffer, bool end) :
						_pThread(ThreadQueue::Current()), _threadPool(threadPool), _end(end), Action("DecodingFile", handler, pFile), _pBuffer(move(pBuffer)) {
					}
				private:
					bool run(Exception& ex, const shared<File>& pFile) {
						UInt32 decoded = pFile->pDecoder->decode(_pBuffer, _end);
						if (_pBuffer)
							handle<ReadFile::Handle>(_pBuffer, _end);
						// decoded=wantToRead!
						if(decoded && !_end)
							_pThread->queue(new ReadFile(handler, pFile, _threadPool, decoded));
						return true;
					}
					shared<Buffer>		_pBuffer;
					bool				_end;
					const ThreadPool&	_threadPool;
					ThreadQueue*		_pThread;
				};
				_threadPool.queue(new Decoding(handler, pFile, _threadPool, pBuffer, _size == available), pFile->_decodingTrack);
			} else
				handle<Handle>(pBuffer, _size == available);
			return true;
		}
		UInt32				_size;
		const ThreadPool&	_threadPool;
	};
	// always do the job even if size==0 to get a onReaden event!
	_threadPool.queue(new ReadFile(handler, pFile, threadPool, size), pFile->_ioTrack);
}

void IOFile::write(const shared<File>& pFile, const Packet& packet) {
	struct WriteFile : Action {
		WriteFile(const Handler& handler, const shared<File>& pFile, const Packet& packet) : _packet(move(packet)), Action("WriteFile", handler, pFile) {
			pFile->_queueing += _packet.size();
		}
	private:
		struct Handle : Action::Handle, virtual Object {
			Handle(const char* name, const shared<File>& pFile) : Action::Handle(name, pFile) {}
		private:
			void handle(File& file) {
				if(!--file._flushing)
					file.onFlush(!file.loaded());
			}
		};
		bool run(Exception& ex, const shared<File>& pFile) {
			UInt64 queueing = (pFile->_queueing -= _packet.size());
			if (!pFile->write(ex, _packet.data(), _packet.size()))
				return false;
			if (queueing)
				return true;
			if(!pFile->_flushing++) // To signal end of write!
				handle<Handle>();
			else
				--pFile->_flushing;
			return true;
		}
		Packet		 _packet;
	};
	// do the WriteFile even if packet is empty when not loaded to allow to open the file and clear its content or create the file
	// or to allow to create the folder => if File is a Folder opened in WRITE/APPEND mode loaded is always false and write an empty packet create the folder => allow a folder creation asynchrone!
	if(packet || !pFile->loaded())
		_threadPool.queue(new WriteFile(handler, pFile, packet), pFile->_ioTrack);
}

void IOFile::erase(const shared<File>& pFile) {
	struct EraseFile : Action {
		EraseFile(const Handler& handler, const shared<File>& pFile) : Action("EraseFile", handler, pFile) {}
	private:
		struct Handle : Action::Handle, virtual Object {
			Handle(const char* name, const shared<File>& pFile) : Action::Handle(name, pFile) {}
		private:
			void handle(File& file) {
				if (!--file._flushing)
					file.onFlush(!file.loaded());
			}
		};
		bool run(Exception& ex, const shared<File>& pFile) {
			if (!pFile->erase(ex))
				return false;
			if (!pFile->_flushing++) // To signal end of write!
				handle<Handle>();
			else
				--pFile->_flushing;
			return true;
		}
	};
	_threadPool.queue(new EraseFile(handler, pFile), pFile->_ioTrack);
}


} // namespace Mona
