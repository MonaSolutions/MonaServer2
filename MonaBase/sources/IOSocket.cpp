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

#include "Mona/IOSocket.h"
#if defined(_OS_BSD)
    #include <sys/types.h>
    #include <sys/event.h>
    #include <sys/time.h>
    #include <vector>
	#include <fcntl.h>
#elif !defined(_WIN32)
    #include <unistd.h>
    #include "sys/epoll.h"
    #include <vector>
	#include <fcntl.h>
#endif

using namespace std;

namespace Mona {

struct IOSocket::Action : Runner, virtual Object {
	template<typename ActionType>
	static void Run(const ThreadPool& threadPool, const shared<ActionType>& pAction) {
		if (!threadPool.queue(pAction->_ex, pAction))
			pAction->run(pAction->_ex);
	}
	template<typename ActionType>
	static void Run(const ThreadPool& threadPool, const shared<ActionType>& pAction, UInt16& track) {
		if (!threadPool.queue(pAction->_ex, pAction, track))
			pAction->run(pAction->_ex);
	}

protected:
	const Handler&		handler;

	struct Handle : Runner, virtual Object {
		Handle(const char* name, const weak<Socket>& weakSocket, const Exception& ex) : Runner(name), _ex(ex), _weakSocket(weakSocket) {}
	private:
		bool run(Exception& ex) {
			// Handler safe thread!
			shared<Socket> pSocket(_weakSocket.lock());
			if (!pSocket)
				return true; // socket dies
			if (_ex)
				pSocket->onError(_ex);
			handle(pSocket);
			return true;
		}
		virtual void	handle(const shared<Socket>& pSocket) {}

		weak<Socket>	_weakSocket;
		Exception		_ex;
	};

	template<typename ActionType>
	struct Rearm : Handle {
		Rearm(const char* name, const weak<Socket>& weakSocket, const Exception& ex, const Handler& handler) :
			_handler(handler), _pThread(ThreadQueue::Current()), Action::Handle(name, weakSocket, ex) {}
	private:
		void handle(const shared<Socket>& pSocket) {
			if (--pSocket->_reading, !pSocket->_readable)
				return;
			Exception ex;
			++pSocket->_reading;
			if (!_pThread->queue(ex, make_shared<ActionType>(0, _handler, pSocket)))
				pSocket->onError(ex);
		}
		ThreadQueue*		_pThread;
		const Handler&		_handler;
	};



	Action(const char* name, int error, const Handler& handler, const shared<Socket>& pSocket) : Runner(name), _weakSocket(pSocket), handler(handler) {
		if (error)
			Socket::SetException(_ex, error);
	}

	template<typename HandleType, typename ...Args>
	void handle(Args&&... args) {
		handler.queue(make_shared<HandleType>(name, _weakSocket, _ex, std::forward<Args>(args)...));
		_ex = NULL;
	}

private:
	bool run(Exception& ex) {
		shared<Socket> pSocket(_weakSocket.lock());
		if (!pSocket)
			return true; // socket dies
		if (!process(_ex, pSocket) && !_ex)
			_ex.set<Ex::Net::Socket>();
		if (_ex)
			handle<Handle>();
		return true;
	}

	virtual bool	process(Exception& ex, const shared<Socket>& pSocket) = 0;
	
	weak<Socket>	_weakSocket;
	Exception		_ex;
};


IOSocket::IOSocket(const Handler& handler, const ThreadPool& threadPool, const char* name) : _initSignal(false),
   _system(0), Thread(name),_subscribers(0),handler(handler), threadPool(threadPool) {
}

IOSocket::~IOSocket() {
	if (!running())
		return;
	_initSignal.wait(); // wait _eventSystem assignment
	#if defined(_WIN32)
		if (_system)
			PostMessage(_system, WM_QUIT, 0, 0);
	#else
		if (_system)
			::close(_eventFD);
	#endif
	Thread::stop();
}


bool IOSocket::subscribe(Exception& ex, const shared<Socket>& pSocket,
											shared<Socket::Decoder>&& pDecoder,
											const Socket::OnReceived& onReceived,
											const Socket::OnFlush& onFlush,
											const Socket::OnDisconnection& onDisconnection,
											const Socket::OnAccept& onAccept,
											const Socket::OnError& onError) {

	// check duplication
	pSocket->onError = onError;
	pSocket->onAccept = onAccept;
	pSocket->onDisconnection = onDisconnection;
	pSocket->onReceived = onReceived;
	pSocket->pDecoder = move(pDecoder);
	pSocket->onFlush = onFlush;

	{
		lock_guard<mutex> lock(_mutex); // must protect "start" + _system (to avoid a write operation on restarting) + _subscribers increment
		if (!running()) {
			_initSignal.reset();
			if (!start(ex)) // alone way to start IOSocket::run
				goto FAIL;
			_initSignal.wait(); // wait _system assignment
		}

		if (!_system) {
			ex.set<Ex::Net::System>(name(), " hasn't been able to start, impossible to manage sockets");
			goto FAIL;
		}

#if defined(_WIN32)
		lock_guard<mutex> lockSockets(_mutexSockets); // must protected _sockets

		if (WSAAsyncSelect(*pSocket, _system, 104, FD_CONNECT | FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE) != 0) { // FD_CONNECT useless (FD_WRITE is sent on connection!)
			ex.set<Ex::Net::System>(Net::LastErrorMessage(), ", ", name(), " can't manage socket ", *pSocket);
			goto FAIL;
		}
		_sockets.emplace(*pSocket, pSocket);
#else
		pSocket->_pWeakThis = new weak<Socket>(pSocket);
		pSocket->ioctl(FIONBIO, true);
		int res;
#if defined(_OS_BSD)
		struct kevent events[2];
		EV_SET(&events[0], sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, pSocket->_pWeakThis);
		EV_SET(&events[1], sockfd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, pSocket->_pWeakThis);
		res = kevent(_system, events, 2, NULL, 0, NULL);
#else
		epoll_event event;
		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
		event.data.fd = *pSocket;
		event.data.ptr = pSocket->_pWeakThis;
		res = epoll_ctl(_system, EPOLL_CTL_ADD,*pSocket, &event);
#endif
		if(res<0) {
			delete pSocket->_pWeakThis;
			pSocket->_pWeakThis = NULL;
			pSocket->ioctl(FIONBIO, false);
			ex.set<Ex::Net::System>(Net::LastErrorMessage(),", ",name()," can't manage sockets");
			goto FAIL;
		}
#endif
		++_subscribers;
	}
	return true;

FAIL:
	pSocket->onFlush = nullptr;
	pSocket->onReceived = nullptr;
	pSocket->onDisconnection = nullptr;
	pSocket->onAccept = nullptr;
	pSocket->onError = nullptr;
	return false;
}

void IOSocket::unsubscribe(shared<Socket>& pSocket) {

	pSocket->onFlush = nullptr;
	pSocket->onReceived = nullptr;
	pSocket->onDisconnection = nullptr;
	pSocket->onAccept = nullptr;
	pSocket->onError = nullptr;


#if defined(_WIN32)
	{
		// decrements _count before the PostMessage
		lock_guard<mutex> lock(_mutexSockets);
		if (!_sockets.erase(*pSocket)) {
			pSocket.reset();
			return;
		}
	}
#else
	if (!pSocket->_pWeakThis) {
		pSocket.reset();
		return;
	}
#endif

	lock_guard<mutex> lock(_mutex); // to avoid a restart during _system reading + protected _count decrement
	--_subscribers;
	
	 // if running _initSignal is set, so _system is assigned
#if defined(_WIN32)
	if (running() && _system) {
		WSAAsyncSelect(*pSocket, _system, 0, 0); // ignore error
		if (!_subscribers)
			PostMessage(_system, 0, 0, 0); // to get stop if no more socket (checking count), ignore error
	}
#else
	if (running() && _system) {
#if defined(_OS_BSD)
		struct kevent events[2];
		EV_SET(&events[0], sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		EV_SET(&events[1], sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		kevent(_system, events, 2, NULL, 0, NULL);
#else
		epoll_event event;
		memset(&event, 0, sizeof(event));
		epoll_ctl(_system, EPOLL_CTL_DEL, *pSocket, &event);
#endif
		if (::write(_eventFD, &pSocket->_pWeakThis, sizeof(pSocket->_pWeakThis)) >= 0)
			pSocket->_pWeakThis = NULL; // success!
	}
	if (pSocket->_pWeakThis) {
		delete pSocket->_pWeakThis;
		pSocket->_pWeakThis = NULL;
	}
	pSocket->ioctl(FIONBIO, false);
#endif
	pSocket.reset();
}


struct IOSocket::Send : IOSocket::Action {
	Send(int error, const Handler& handler, const shared<Socket>& pSocket) :
		Action("SocketSend", error, handler, pSocket) {}

	bool process(Exception& ex, const shared<Socket>& pSocket) {
		if (pSocket->type == Socket::TYPE_STREAM) {
			if (ex || !pSocket->flush(ex)) {
				handle<Handle>(true);
				return false;
			}
		} else
			pSocket->flush(ex);
		// handle if no more queueing data
		// On TCP connection, socket becomes writable and onFlush has to raise
		if (!pSocket->queueing())
			handle<Handle>();
		return true;
	}

private:
	struct Handle : Action::Handle {
		Handle(const char* name, const weak<Socket>& weakSocket, const Exception& ex, bool disconnection = false) : _disconnection(disconnection), Action::Handle(name, weakSocket, ex) {}
	private:
		void handle(const shared<Socket>& pSocket) {
			if (_disconnection) {
				pSocket->onDisconnection();
				pSocket->onDisconnection = nullptr;
				return;
			}
			if (!pSocket->queueing()) // check again on handle, "queueing" can had changed
				pSocket->onFlush();
		}
		bool _disconnection;
	};
};


void IOSocket::write(const shared<Socket>& pSocket, int error) {
	// printf("WRITE(%d) socket %d\n", error, pSocket->_sockfd);
	Action::Run(threadPool, make_shared<Send>(error, handler, pSocket));
}

void IOSocket::read(const shared<Socket>& pSocket, int error) {
	// printf("READ(%d) socket %d\n", error, pSocket->_sockfd);
	++pSocket->_readable;
	if (!error && pSocket->_reading)
		return; // useless!
	++pSocket->_reading;

	if (pSocket->_listening) {

		struct Accept : Action {
			Accept(int error, const Handler& handler, const shared<Socket>& pSocket) :
				Action("SocketAccept", error, handler, pSocket) {}
		private:
			struct Handle : Action::Handle {
				Handle(const char* name, const weak<Socket>& weakSocket, const Exception& ex, const shared<Socket>& pSocket) :
					_pSocket(pSocket), Action::Handle(name, weakSocket, ex) {}
			private:
				void handle(const shared<Socket>& pSocket) { pSocket->onAccept(_pSocket); }
				shared<Socket>		_pSocket;
			};
			bool process(Exception& ex, const shared<Socket>& pSocket) {
				if (--pSocket->_reading) // myself and someone else, no need, return!
					return true;

				bool rearm(false);
				UInt8 max(200); // blacklog maximum, see http://tangentsoft.net/wskfaq/advanced.html#backlog
				while (max && pSocket->_readable) { // /!\ Continue just if is in a possible blacklog range, Then wait rearm!
					--pSocket->_readable;
					shared<Socket> pConnection;
					if (!pSocket->accept(ex, pConnection)) {
						if (ex.cast<Ex::Net::Socket>().code == NET_EWOULDBLOCK) {
							ex = nullptr;
							continue;
						}
						if (!ex)
							ex.set<Ex::Net::Socket>();
						Action::handle<Action::Handle>(); // to call onError immediatly (and continue reception!)
						continue;
					}
					if (!rearm) {
						++pSocket->_reading;
						rearm = true;
					}
					handle<Handle>(pConnection);
					--max;
				}
				if (rearm)
					handle<Rearm<Accept>>(handler);
				return true;
			}
		};

		Action::Run(threadPool, make_shared<Accept>(error, handler, pSocket), pSocket->_threadReceive);
		return;
	}


	struct Receive : Action {
		Receive(int error, const Handler& handler, const shared<Socket>& pSocket) :
			Action("SocketReceive", error, handler, pSocket) {}

	private:
		struct Handle : Action::Handle {
			Handle(const char* name, const weak<Socket>& weakSocket, const Exception& ex, const shared<Buffer>& pBuffer, const SocketAddress& address) :
				Action::Handle(name, weakSocket, ex), _address(address), _pBuffer(pBuffer) {}
		private:
			void handle(const shared<Socket>& pSocket) {
				if (pSocket->type == Socket::TYPE_STREAM && !_pBuffer->size()) {
					pSocket->onDisconnection();
					pSocket->onDisconnection = nullptr; // just one onDisconnection!
				} else
					pSocket->onReceived(_pBuffer, _address);
			}
			shared<Buffer>		_pBuffer;
			SocketAddress		_address;
		};
		bool process(Exception& ex, const shared<Socket>& pSocket) {
			if (--pSocket->_reading) // myself and someone else, no need, return!
				return true;
			bool rearm(false);
			UInt32 bufferSize = pSocket->recvBufferSize();
			if (!bufferSize)
				bufferSize = 1;
			while (bufferSize && pSocket->_readable) {
				--pSocket->_readable;

				UInt32 available(pSocket->available());
				shared<Buffer>	pBuffer(new Buffer(available));
				SocketAddress	address;

				bool queueing(pSocket->queueing() ? true : false);
				int received = pSocket->receive(ex, pBuffer->data(), available ? available : 1, 0, &address); // always get something! Read!
				if (received < 0) {
					// error, but not necessary a disconnection
					if (ex.cast<Ex::Net::Socket>().code == NET_EWOULDBLOCK) {
						ex = nullptr;
						// Should almost never happen, when happen it's usually a "would block" relating a writing request (SSL handshake for example)
						if (queueing)
							Send(0, handler, pSocket).process(ex, pSocket);
						continue;
					}
					if (!ex)
						ex.set<Ex::Net::Socket>();
					if (pSocket->type != Socket::TYPE_STREAM) {
						handle<Action::Handle>(); // to call onError immediatly (will continue reception!)
						continue; // continue to rearm on end if need!
					}
					// else TCP reception error => not reliable => disconnection!
					received = 0;
				}

				pBuffer->resize(received); // if received==0 => disconnection!
				// /!\ Continue just if less than socket.recvBufferSize! Then wait rearm!
				if (bufferSize > available)
					bufferSize -= available;
				else
					bufferSize = 0;

				if (pSocket->type != Socket::TYPE_STREAM || received) {
					// if udp or not TCP disconnection
					// decode can't happen BEFORE onDisconnection because this call decode + push to _handler in this call!
					if (pSocket->pDecoder) {
						UInt32 decoded = pSocket->pDecoder->decode(pBuffer, address, pSocket);
						if (!pBuffer)
							continue;
						if (decoded < pBuffer->size())
							pBuffer->resize(decoded);
					}
					if (!rearm) {
						++pSocket->_reading;
						rearm = true;
					}
					// no decoding subscription
				} // else TCP disconnection

				handle<Handle>(pBuffer, address);
			}
			if(rearm)
				handle<Rearm<Receive>>(handler);
			return true;
		}
	};

	Action::Run(threadPool, make_shared<Receive>(error, handler, pSocket), pSocket->_threadReceive);
}


bool IOSocket::run(Exception& ex, const volatile bool& stopping) {
#if defined(_WIN32)
	WNDCLASSEX wc;
	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = name();
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	RegisterClassEx(&wc);
	_system = CreateWindow(name(), name(), WS_EX_LEFT, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

#else
	UInt16 maxevents(0xFFFF); // max possible sockets = port available
	int pipefds[2] = {};
	int readFD(0);
	_system=_eventFD=0;
	if(pipe(pipefds)==0) {
		readFD = pipefds[0];
		_eventFD = pipefds[1];
	}
#if defined(_OS_BSD)
	struct kevent events[maxevents];
	if(readFD>0 && _eventFD>0 && fcntl(readFD, F_SETFL, fcntl(readFD, F_GETFL, 0) | O_NONBLOCK)!=-1)
        _system = kqueue();
#else
	epoll_event events[maxevents];
	if(readFD>0 && _eventFD>0 && fcntl(readFD, F_SETFL, fcntl(readFD, F_GETFL, 0) | O_NONBLOCK)!=-1)
		_system = epoll_create(maxevents); // Argument is ignored on new system, otherwise must be >= to events[] size
#endif
	if(_system<=0) {
		if(_eventFD>0)
			::close(_eventFD);
		if(readFD>0)
			::close(readFD);
		_system = 0;
	} else {
		// Add the event to terminate the epoll_wait!
#if defined(_OS_BSD)
		struct kevent event;
        EV_SET(&event, readFD, EVFILT_READ | EV_CLEAR, EV_ADD, 0, 0, NULL);
        kevent(_system, &event, 1, NULL, 0, NULL);
#else    
        epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = EPOLLIN;
        event.data.fd = readFD;
        epoll_ctl(_system, EPOLL_CTL_ADD,readFD, &event);
#endif
	}
#endif

	_initSignal.set();

	if (!_system) {
		ex.set<Ex::Net::System>(name(), " starting failed, impossible to manage sockets");
		return false;
	}
	

	int result(0);

#if defined(_WIN32)
	Exception ignore;
	MSG msg;
	
	while ((result=GetMessage(&msg, _system, 0, 0)) > 0) {
		
		if (!_subscribers) {
			lock_guard<mutex> lock(_mutex);
			// no more socket to manage?
			if (!_subscribers) {
				DestroyWindow(_system);
				stop(); // to set stop=true before unlock!
				return true;
			}
		}

		if (msg.wParam == 0 || msg.message != 104)
			continue;
	
		shared<Socket> pSocket;
		{
			lock_guard<mutex> lock(_mutexSockets);
			auto& it = _sockets.find(msg.wParam);
			if (it != _sockets.end())
				pSocket = it->second.lock();
		}
		if (!pSocket)
			continue; // socket error

		UInt32 event(WSAGETSELECTEVENT(msg.lParam));
/*		// Map to display event string when need
		static map<UInt32, const char*> events({
			{ FD_READ, "READ" },
			{ FD_WRITE, "WRITE" },
			{ FD_ACCEPT, "ACCEPT" },
			{ FD_CLOSE, "CLOSE" },
			{ FD_CONNECT, "CONNECT" }
		});
		printf("%s(%d) socket %u\n", events[event], WSAGETSELECTERROR(msg.lParam), msg.wParam);*/

		// FD_READ, FD_WRITE, FD_CLOSE, FD_ACCEPT, FD_CONNECT
		int error(WSAGETSELECTERROR(msg.lParam));
		if (event == FD_READ || event == FD_CLOSE || event == FD_ACCEPT)
			read(pSocket, error);
		else if (event == FD_WRITE || (event == FD_CONNECT && error)) // IF CONNECT+SUCCESS NO NEED, WAIT WRITE EVENT RATHER
			write(pSocket, error);
	}
	DestroyWindow(_system);
#else
	vector<weak<Socket>*>	removedSockets;

	while(result>=0) {

#if defined(_OS_BSD)
		result = kevent(_system, NULL, 0, events, maxevents, NULL);
#else
		result = epoll_wait(_system,events,maxevents, -1);
#endif

		int i;
		if(result>=0 || errno==NET_EINTR) {
			// for each ready socket
			
			for(i=0;i<result;++i) {

#if defined(_OS_BSD)

				kevent& event(events[i]);
				if(event.ident==readFD) {
					if(event.flags & EV_EOF) {
						i=-1; // termination signal on IOSocket deletion
						break;
					}

					// socket deletion signal!
					weak<Socket>* pWeakSocket;
					while (::read(readFD, &pWeakSocket, sizeof(pWeakSocket)) > 0)
						removedSockets.emplace_back(pWeakSocket);
					continue;
				}

				shared<Socket> pSocket(reinterpret_cast<weak<Socket>*>(event.udata)->lock());
				if(!pSocket)
					continue; // socket error
				// EVFILT_READ | EVFILT_WRITE
				int error = 0;
				if(event.flags&EV_ERROR) {
					socklen_t len(sizeof(error));
					if(getsockopt(pSocket->_sockfd, SOL_SOCKET, SO_ERROR, (void *)&error, &len)==-1)
						error = Net::LastError();
				}
				if (event.filter&EVFILT_READ) {
					read(pSocket, error);
					error = 0;
				}
				if (event.filter&EVFILT_WRITE)
					write(pSocket, error);

#else
				epoll_event& event(events[i]);
				if(event.data.fd==readFD) {
					if(event.events&EPOLLHUP) {
						i=-1; // termination signal on IOSocket deletion
						break;
					}

					// socket deletion signal!
					weak<Socket>* pWeakSocket;
					while (::read(readFD, &pWeakSocket, sizeof(pWeakSocket)) > 0)
						removedSockets.emplace_back(pWeakSocket);
					continue;
				}

				shared<Socket> pSocket(reinterpret_cast<weak<Socket>*>(event.data.ptr)->lock());
				if(!pSocket)
					continue; // socket error
				// EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLRDHUP
				// printf("%u socket %u\n", event.events, *pSocket);
				int error = 0;
				if(event.events&EPOLLERR) {
					socklen_t len(sizeof(error));
					if(getsockopt(pSocket->_sockfd, SOL_SOCKET, SO_ERROR, (void *)&error, &len)==-1)
						error = Net::LastError();
				}
				if ((event.events&EPOLLIN) || (event.events&EPOLLRDHUP)) {
					read(pSocket, error);
					error = 0;
				}
				if (event.events&EPOLLOUT)
					write(pSocket, error);

#endif
				
			}
		}

		// remove sockets
		for (weak<Socket>* pWeakSocket : removedSockets)
			delete pWeakSocket;
		removedSockets.clear();

		if(i==-1)
			break; // termination signal on IOSocket deletion

		if (!_subscribers) {
			lock_guard<mutex> lock(_mutex);
			// no more socket to manage?
			if (!_subscribers) {
				::close(readFD);  // close reader pipe side
				::close(_system); // close the system message
				stop(); // to set running=false!
				return true;
			}
		}
	}
	::close(readFD);  // close reader pipe side
	::close(_system); // close the system message

#endif

	// unexpected error OR IOSocket deletion!
	if (result < 0) // error
		ex.set<Ex::Net::System>(name(), " failed, impossible to manage sockets");
	else if (_subscribers) // IOSocket deletion
		ex.set<Ex::Net::System>(name(), " dies with remaining sockets managed");

	return ex ? false : true;
}
	


} // namespace Mona
