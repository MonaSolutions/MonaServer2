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

struct IODevice : virtual Object, ThreadQueue { IODevice() : ThreadQueue("IODevice") {} };
static struct IODevices : virtual Object {
	IODevice& operator[](UInt8 device) {
		lock_guard<mutex> lock(_mutex);
		return _devices[device];
	}
	UInt32 join() {
		UInt32 count(0);
		lock_guard<mutex> lock(_mutex);
		for (auto& it : _devices) {
			if (it.second.running())
				++count;
			it.second.stop();
		}
		return count;
	}
private:
	map<UInt8, IODevice> _devices;
	mutex			     _mutex;
} _IODevices;

struct IOFile::Action : Runner, virtual Object {
	NULLABLE
	Action(const char* name, const Handler& handler, const shared<File>& pFile) : loading(false), handler(handler), _weakFile(pFile), Runner(name) {}

	bool loading;
	shared<File> file() { return _weakFile.lock(); }
	virtual operator bool() const { return false; }

	struct Handle : Runner, virtual Object {
		Handle(const char* name, const weak<File>& weakFile) : Runner(name), _weakFile(weakFile) {}
	private:
		bool run(Exception& ex) {
			shared<File> pFile(_weakFile.lock());
			if (pFile)
				handle(*pFile);
			return true;
		}
		virtual void handle(File& file) = 0;
		weak<File>	_weakFile;
	};

	void onError(Exception& ex) {
		struct ErrorHandle : Handle, virtual Object {
			ErrorHandle(const char* name, const weak<File>& weakFile, Exception& ex) : Handle(name, weakFile), _ex(move(ex)) {}
		private:
			void handle(File& file) { file.onError(_ex); }
			Exception		_ex;
		};
		handle<ErrorHandle>(ex);
	}

protected:
	template<typename HandleType, typename ...Args>
	void handle(Args&&... args) {
		handler.queue(make_shared<HandleType>(name, _weakFile, forward<Args>(args)...));
	}

	const Handler&	handler;
private:
	bool run(Exception& ex) {
		shared<File> pFile(_weakFile.lock());
		if (!pFile)
			return true;
		if(!run(ex, pFile))
			onError(ex);
		if (loading)
			--pFile->_loading;
		return true;
	}
	virtual bool run(Exception& ex, const shared<File>& pFile) { return true; }

	weak<File>	_weakFile;
};

void IOFile::join() {
	// wait end of dispath file
	((ThreadPool&)threadPool).join(); // can cast because IOFile constructor takes a non-const threadPool object
	// join devices (reading and writing operation)
	while(_IODevices.join()) // while reading/writing operation
		((ThreadPool&)threadPool).join(); // wait possible decoding!
}

void IOFile::dispatch(File& file, const shared<Action>& pAction) {
	Exception ex;
	IODevice* pDevice = NULL;
	if (file) {// loaded! assign pDevice!
		pDevice = (IODevice*)file._pDevice.load();
		if(!pDevice)
			file._pDevice = pDevice = &_IODevices[file.device()];
	}
	if (!pDevice || file._loading) {
		struct Dispatch : Runner {
			Dispatch(const shared<Action>& pAction) : _pAction(pAction), Runner("DispatchFile") {}
		private:
			bool run(Exception& ex) {
				shared<File> pFile = _pAction->file();
				if (!pFile)
					return true;
				IODevice* pDevice = (IODevice*)pFile->_pDevice.load();
				if (!pDevice) {
					if (!pFile->load(ex)) {
						_pAction->onError(ex);
						--pFile->_loading;
						return true;
					}
					pFile->_pDevice = pDevice = &_IODevices[pFile->device()];
				}
				if(*_pAction && !pDevice->queue(ex, _pAction))
					_pAction->onError(ex);
				--pFile->_loading;
				return true;
			}
			shared<Action> _pAction;
		};
		++file._loading;
		pAction->loading = true;
		if (threadPool.queue(ex, make_shared<Dispatch>(pAction), file._loadingTrack))
			return;
		--file._loading;
	} else if (pDevice->queue(ex, pAction))
		return;
	pAction->onError(ex);
}

bool IOFile::open(Exception& ex, shared<File>& pFile, const Path& path, shared<File::Decoder>&& pDecoder, const File::OnReaden& onReaden, const File::OnError& onError) {
	pFile.reset(new File(path, File::MODE_READ));
	if (!pFile->load(ex)) {
		pFile.reset();
		return false;
	}
	pFile->onError = onError;
	pFile->onReaden = onReaden;
	pFile->pDecoder = move(pDecoder);
	return true;
}
bool IOFile::open(Exception& ex, shared<File>& pFile, const Path& path, const File::OnFlush& onFlush, const File::OnError& onError, bool append) {
	pFile.reset(new File(path, append ? File::MODE_APPEND : File::MODE_WRITE));
	if (!pFile->load(ex)) {
		pFile.reset();
		return false;
	}
	pFile->onError = onError;
	pFile->onFlush = onFlush;
	return true;
}

void IOFile::open(shared<File>& pFile, const Path& path, shared<File::Decoder>&& pDecoder, const File::OnReaden& onReaden, const File::OnError& onError) {
	pFile.reset(new File(path, File::MODE_READ));
	pFile->onError = onError;
	pFile->onReaden = onReaden;
	pFile->pDecoder = move(pDecoder);
	dispatch(*pFile, make_shared<Action>("OpenFile", handler, pFile));
}
void IOFile::open(shared<File>& pFile, const Path& path, const File::OnFlush& onFlush, const File::OnError& onError, bool append) {
	pFile.reset(new File(path, append ? File::MODE_APPEND : File::MODE_WRITE));
	pFile->onError = onError;
	pFile->onFlush = onFlush;
	dispatch(*pFile, make_shared<Action>("OpenFile", handler, pFile));
}


void IOFile::close(shared<File>& pFile) {
	pFile->onReaden = nullptr;
	pFile->onFlush = nullptr;
	pFile->onError = nullptr;
	pFile.reset();
}

void IOFile::read(const shared<File>& pFile, UInt32 size) {
	struct ReadFile : Action {
		ReadFile(const Handler& handler, const shared<File>& pFile, const ThreadPool& threadPool, UInt32 size) : Action("ReadFile", handler, pFile), _threadPool(threadPool), _size(size) {}
		operator bool() const { return true; }
	private:
		struct Handle : Action::Handle, virtual Object {
			Handle(const char* name, const weak<File>& weakFile, shared<Buffer>& pBuffer, bool end) :
				Action::Handle(name, weakFile), _pBuffer(move(pBuffer)), _end(end) {}
		private:
			void handle(File& file) { file.onReaden(_pBuffer, _end); }
			shared<Buffer>	_pBuffer;
			bool   _end;
		};
		bool run(Exception& ex, const shared<File>& pFile) {
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
					Decoding(const Handler& handler, const shared<File>& pFile, const ThreadPool& threadPool, shared<Buffer>& pBuffer, bool end) : _threadPool(threadPool), _end(end), Action("DecodingFile", handler, pFile), _pBuffer(move(pBuffer)) {}
				private:
					bool run(Exception& ex, const shared<File>& pFile) {
						UInt32 decoded = pFile->pDecoder->decode(_pBuffer, _end);
						if(_pBuffer) {
							if(decoded<_pBuffer->size())
								_pBuffer->resize(decoded);
							handle<ReadFile::Handle>(_pBuffer, _end);
							return true;
						}
						// here decoded=wantToRead!
						return !decoded || _end ? true : ((IODevice*)pFile->_pDevice.load())->queue(ex, make_shared<ReadFile>(handler, pFile, _threadPool, decoded));
					}
					shared<Buffer>		_pBuffer;
					weak<File>			_weakFile;
					bool				_end;
					const ThreadPool&	_threadPool;
				};
				if (!_threadPool.queue(ex, make_shared<Decoding>(handler, pFile, _threadPool, pBuffer, _size == available), pFile->_decodingTrack))
					return false;
			} else
				handle<Handle>(pBuffer, _size == available);
			return true;
		}
		UInt32				_size;
		const ThreadPool&	_threadPool;
	};
	dispatch(*pFile, make_shared<ReadFile>(handler, pFile, threadPool, size));
}

void IOFile::write(const shared<File>& pFile, const Packet& packet) {
	struct WriteFile : Action {
		WriteFile(const Handler& handler, const shared<File>& pFile, const Packet& packet) : _pFile(pFile), _packet(move(packet)), Action("WriteFile", handler, pFile) {
			_flushing = (pFile->_queueing += _packet.size())>0xFFFF;
		}
		operator bool() const { return true; }
	private:
		struct Handle : Action::Handle, virtual Object {
			Handle(const char* name, const weak<File>& weakFile) : Action::Handle(name, weakFile) {}
		private:
			void handle(File& file) { file.onFlush(); }
		};
		bool run(Exception& ex, const shared<File>& pFile) {
			_flushing = _flushing && (pFile->_queueing -= _packet.size())<= 0xFFFF;
			if (!pFile->write(ex, _packet.data(), _packet.size()))
				return false;
			if(_flushing)
				handle<Handle>();
			return true;
		}
		Packet		 _packet;
		shared<File> _pFile; // maintain alive, Write operation must be reliable!
		bool		 _flushing;
	};
	if (packet) // useless if empty
		dispatch(*pFile, make_shared<WriteFile>(handler, pFile, packet));
}


} // namespace Mona
