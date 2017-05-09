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
#if defined(_BSD)
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
#if !defined(EPOLLRDHUP)  // ANDROID
#define EPOLLRDHUP 0x2000 // looks be just a SDL include forget for Android, but the event is implemented in epoll of Android
#endif  // !defined(EPOLLRDHUP) 
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

	Action(const char* name, int error, const shared<Socket>& pSocket) : Runner(name), _weakSocket(pSocket), _handler(*pSocket->_pHandler) {
		if (error)
			Socket::SetException(_ex, error);
	}

protected:

	
	struct Handle : Runner, virtual Object {
		Handle(const char* name, const shared<Socket>& pSocket, const Exception& ex) : Runner(name), _ex(ex), _weakSocket(pSocket) {}
	private:
		bool run(Exception&) {
			// Handler safe thread!
			shared<Socket> pSocket(_weakSocket.lock());
			if (!pSocket)
				return true; // socket dies
			if (_ex)
				pSocket->onError(_ex); // call onError before handle (ex: onError, onDisconnection)
			handle(pSocket);
			return true;
		}
		virtual void	handle(const shared<Socket>& pSocket) {}

		weak<Socket>	_weakSocket;
		Exception		_ex;
	};

	template<typename HandleType, typename ...Args>
	void handle(const shared<Socket>& pSocket, Args&&... args) {
		_handler.queue(make_shared<HandleType>(name, pSocket, _ex, std::forward<Args>(args)...));
		_ex = NULL;
	}

private:
	bool run(Exception&) {
		shared<Socket> pSocket(_weakSocket.lock());
		if (!pSocket)
			return true; // socket dies
		if (!process(_ex, pSocket) && !_ex)
			_ex.set<Ex::Net::Socket>();
		if (_ex)
			handle<Handle>(pSocket);
		return true;
	}

	virtual bool	process(Exception& ex, const shared<Socket>& pSocket) { return true; }
	
	weak<Socket>	_weakSocket;
	Exception		_ex;
	const Handler&  _handler;
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

	bool block = false;
	if (!pSocket->getNonBlockingMode()) {
		block = true;
		if( !pSocket->setNonBlockingMode(ex, true))
			return false;
	}

	// check duplication
	pSocket->onError = onError;
	pSocket->onAccept = onAccept;
	pSocket->onDisconnection = onDisconnection;
	pSocket->onReceived = onReceived;
	pSocket->pDecoder = move(pDecoder);
	pSocket->onFlush = onFlush;
	pSocket->_pHandler = &handler;

	{
		lock_guard<mutex> lock(_mutex); // must protect "start" + _system (to avoid a write operation on restarting) + _subscribers increment
		if (!running()) {
			_initSignal.reset();
			if (!start(ex)) // only way to start IOSocket::run
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
		int res;
#if defined(_BSD)
		struct kevent events[2];
		EV_SET(&events[0], *pSocket, EVFILT_READ, EV_ADD | EV_CLEAR | EV_EOF, 0, 0, pSocket->_pWeakThis);
		EV_SET(&events[1], *pSocket, EVFILT_WRITE, EV_ADD | EV_CLEAR | EV_EOF, 0, 0, pSocket->_pWeakThis);
		res = kevent(_system, events, 2, NULL, 0, NULL);
#else
		epoll_event event;
		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN  | EPOLLRDHUP | EPOLLOUT | EPOLLET;
		event.data.fd = *pSocket;
		event.data.ptr = pSocket->_pWeakThis;
		res = epoll_ctl(_system, EPOLL_CTL_ADD,*pSocket, &event);
#endif
		if(res<0) {
			delete pSocket->_pWeakThis;
			pSocket->_pWeakThis = NULL;
			ex.set<Ex::Net::System>(Net::LastErrorMessage(),", ",name()," can't manage sockets");
			goto FAIL;
		}
#endif
		++_subscribers;
	}
	return true;

FAIL:
	if (block)
		pSocket->setNonBlockingMode(ex, false);
	pSocket->pDecoder = nullptr;
	pSocket->onFlush = nullptr;
	pSocket->onReceived = nullptr;
	pSocket->onDisconnection = nullptr;
	pSocket->onAccept = nullptr;
	pSocket->onError = nullptr;
	return false;
}

void IOSocket::unsubscribe(shared<Socket>& pSocket) {
	// don't touch to pDecoder because can be accessing by receiving thread (thread safety)
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
#if defined(_BSD)
		struct kevent events[2];
		EV_SET(&events[0], *pSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		EV_SET(&events[1], *pSocket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
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
#endif
	pSocket.reset();
}


struct IOSocket::Send : IOSocket::Action {
	Send(int error, const shared<Socket>& pSocket) : Action("SocketSend", error, pSocket) {}

	bool process(Exception& ex, const shared<Socket>& pSocket) {
		if (!pSocket->flush(ex))
			return false;
		// handle if no more queueing data (and on first time allow to get connection info!)
		if (!pSocket->queueing())
			handle<Handle>(pSocket);
		return true;
	}

private:
	struct Handle : Action::Handle {
		Handle(const char* name, const shared<Socket>& pSocket, const Exception& ex) : Action::Handle(name, pSocket, ex) {}
	private:
		void handle(const shared<Socket>& pSocket) {
			if (!pSocket->queueing()) // check again on handle, "queueing" can had changed
				pSocket->onFlush();
		}
	};
};


void IOSocket::write(const shared<Socket>& pSocket, int error) {
	//::printf("WRITE(%d) socket %d\n", error, pSocket->_sockfd);
#if !defined(_WIN32)
	if (pSocket->_firstWritable)
		pSocket->_firstWritable = false;
#endif
	Action::Run(threadPool, make_shared<Send>(error, pSocket));
}

void IOSocket::read(const shared<Socket>& pSocket, int error) {
	//::printf("READ(%d) socket %d\n", error, pSocket->_sockfd);
	if(pSocket->_reading && !error)
		return; // useless!
	++pSocket->_reading;

	if (pSocket->_listening) {

		struct Accept : Action {
			Accept(int error, const shared<Socket>& pSocket) : Action("SocketAccept", error, pSocket) {}
		private:
			struct Handle : Action::Handle {
				Handle(const char* name, const shared<Socket>& pSocket, const Exception& ex, shared<Socket>& pConnection, bool& stop) :
					Action::Handle(name, pSocket, ex), _pConnection(move(pConnection)), _pThread(NULL) {
					if (++pSocket->_receiving < Socket::BACKLOG_MAX)
						return;
					stop = true;
					_pThread = ThreadQueue::Current();
					++pSocket->_reading;
				}
			private:
				void handle(const shared<Socket>& pSocket) {
					pSocket->onAccept(_pConnection);
					UInt32 receiving = --pSocket->_receiving;
					if (!_pThread)
						return;
					if (receiving < Socket::BACKLOG_MAX) {
						// REARM
						Exception ex;
						if (!_pThread->queue(ex, make_shared<Accept>(0, pSocket)))
							pSocket->onError(ex);
					} else
						--pSocket->_reading;
				}
				shared<Socket>		_pConnection;
				ThreadQueue*		_pThread;
			};
			bool process(Exception& ex, const shared<Socket>& pSocket) {
				if (!pSocket->_reading--) // me and something else! useless!
					return true;
				shared<Socket> pConnection;
				bool stop(false);
				while(!stop && pSocket->accept(ex, pConnection))
					handle<Handle>(pSocket, pConnection, stop);
				if (ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
					return false;
				ex = nullptr;
				return true;
			}
		};

		return Action::Run(threadPool, make_shared<Accept>(error, pSocket), pSocket->_threadReceive);
	}


	struct Receive : Action {
		Receive(int error, const shared<Socket>& pSocket) : Action("SocketReceive", error, pSocket) {}

	private:
		struct Handle : Action::Handle {
			Handle(const char* name, const shared<Socket>& pSocket, const Exception& ex, shared<Buffer>& pBuffer, const SocketAddress& address, bool& stop) :
				Action::Handle(name, pSocket, ex), _address(address), _pBuffer(move(pBuffer)), _pThread(NULL) {
				if ((pSocket->_receiving += _pBuffer->size()) < pSocket->recvBufferSize())
					return;
				stop = true;
				_pThread = ThreadQueue::Current();
				++pSocket->_reading;
			}
		private:
			void handle(const shared<Socket>& pSocket) {
				UInt32 receiving = _pBuffer->size();
				pSocket->onReceived(_pBuffer, _address);
				receiving = pSocket->_receiving -= receiving;
				if (!_pThread)
					return;
				if(receiving < pSocket->recvBufferSize()) {
					// REARM
					Exception ex;
					if (!_pThread->queue(ex, make_shared<Receive>(0, pSocket)))
						pSocket->onError(ex);
				} else
					--pSocket->_reading;
			}
			shared<Buffer>		_pBuffer;
			SocketAddress		_address;
			ThreadQueue*		_pThread;
		};

		bool process(Exception& ex, const shared<Socket>& pSocket) {
			if (!pSocket->_reading--) // me and something else! useless!
				return true;
			UInt32 available = pSocket->available();
			bool stop(false);
			while (!stop) {
				if (!available) // always get something (maybe a new reception has been gotten since the last pSocket->available() call)
					available = 2048; // in UDP allows to avoid a NET_EMSGSIZE error (where packet is lost!), and 2048 to be greater than max possible MTU (~1500 bytes)
				shared<Buffer>	pBuffer(new Buffer(available));
				SocketAddress	address;
				bool queueing(pSocket->queueing() ? true : false);
				int received = pSocket->receive(ex, pBuffer->data(), available, 0, &address);
				if (received < 0) {
					// if NET_EMSGSIZE => UDP packet lost! (can happen on windows! error displaid!)
					// error, but not necessary a disconnection
					if (ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
						return false;
					// ::printf("NET_EWOULDBLOCK %d\n", pSocket->_sockfd);
					ex = nullptr;
					// Should almost never happen, when happen it's usually a "would block" relating a writing request (SSL handshake for example)
					if (queueing)
						Send(0, pSocket).process(ex, pSocket);
					available = pSocket->available();
					if (available)
						continue;
					break;
				}

				// a recv returns 0 without any error can happen on TCP socket one time disconncted!
				if (!received && pSocket->type == Socket::TYPE_STREAM) {
					pSocket->_reading = 0xFF; // block reception!
					return true;
				}

				pBuffer->resize(received);

				// decode can't happen BEFORE onDisconnection because this call decode + push to _handler in this call!
				if (pSocket->pDecoder) {
					UInt32 decoded = pSocket->pDecoder->decode(pBuffer, address, pSocket);
					if (pBuffer && decoded < pBuffer->size())
						pBuffer->resize(decoded);
				}
				if(pBuffer)
					handle<Handle>(pSocket, pBuffer, address, stop);
				available = pSocket->available();
			};
			return true;
		}
	};

	Action::Run(threadPool, make_shared<Receive>(error, pSocket), pSocket->_threadReceive);
}

void IOSocket::close(const shared<Socket>& pSocket, int error) {
	//::printf("CLOSE(%d) socket %d\n", error, pSocket->_sockfd);
	if (pSocket->type != Socket::TYPE_STREAM)
		return; // no Disconnection requirement!
	struct Close : Action {
		Close(int error, const shared<Socket>& pSocket) : Action("SocketClose", error, pSocket) {}
	private:
		struct Handle : Action::Handle {
			Handle(const char* name, const shared<Socket>& pSocket, const Exception& ex) : Action::Handle(name, pSocket, ex) {}
		private:
			void handle(const shared<Socket>& pSocket) {
				pSocket->onDisconnection();
				pSocket->onDisconnection = nullptr; // just one onDisconnection!
			}
		};
		bool process(Exception& ex, const shared<Socket>& pSocket) {
			pSocket->_reading = 0xFF; // block reception!
			handle<Handle>(pSocket);
			return true;
		}
	};
	Action::Run(threadPool, make_shared<Close>(error, pSocket), pSocket->_threadReceive);
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
	int pipefds[2] = {};
	int readFD(0);
	_system=_eventFD=0;
	if(pipe(pipefds)==0) {
		readFD = pipefds[0];
		_eventFD = pipefds[1];
	}

// max possible socket, must be limited to the maximum threads possible on the system (and not too high because can exceeds stack limitation of 1Mo like on Android)
#define MAXEVENTS  1024

#if defined(_BSD)
	struct kevent events[MAXEVENTS];
	if(readFD>0 && _eventFD>0 && fcntl(readFD, F_SETFL, fcntl(readFD, F_GETFL, 0) | O_NONBLOCK)!=-1)
        _system = kqueue();
#else
	epoll_event events[MAXEVENTS];
	if(readFD>0 && _eventFD>0 && fcntl(readFD, F_SETFL, fcntl(readFD, F_GETFL, 0) | O_NONBLOCK)!=-1)
		_system = epoll_create(MAXEVENTS); // Argument is ignored on new system, otherwise must be >= to events[] size
#endif
	if(_system<=0) {
		if(_eventFD>0)
			::close(_eventFD);
		if(readFD>0)
			::close(readFD);
		_system = 0;
	} else {
		// Add the event to terminate the epoll_wait!
#if defined(_BSD)
		struct kevent event;
        EV_SET(&event, readFD, EVFILT_READ, EV_ADD, 0, 0, NULL);
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
		ex.set<Ex::Net::System>("impossible to start IOSocket");
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
		// Map to display event string when need
		/*static map<UInt32, const char*> events({
			{ FD_READ, "READ" },
			{ FD_WRITE, "WRITE" },
			{ FD_ACCEPT, "ACCEPT" },
			{ FD_CLOSE, "CLOSE" },
			{ FD_CONNECT, "CONNECT" }
		});
		::printf("%s(%d) socket %u\n", events[event], WSAGETSELECTERROR(msg.lParam), msg.wParam);*/

		// FD_READ, FD_WRITE, FD_CLOSE, FD_ACCEPT, FD_CONNECT
		int error(WSAGETSELECTERROR(msg.lParam));
		if (event == FD_CLOSE || (event == FD_CONNECT && error)) // IF CONNECT+SUCCESS NO NEED, WAIT WRITE EVENT RATHER
			close(pSocket, error);
		else if (event == FD_READ || event == FD_ACCEPT)
			read(pSocket, error);
		else if (event == FD_WRITE) 
			write(pSocket, error);
	}
	DestroyWindow(_system);

	if (result < 0) { // error
		ex.set<Ex::Net::System>("impossible to manage sockets (error ", GetLastError(), ")");
		return false;
	}

#else
	vector<weak<Socket>*>	removedSockets;

	for (;;) {

#if defined(_BSD)
		result = kevent(_system, NULL, 0, events, MAXEVENTS, NULL);
#else
		result = epoll_wait(_system,events, MAXEVENTS, -1);
#endif

		int i;
		if (result < 0) {
			if (errno == NET_EINTR)
				continue;
			break;
		}

		// for each ready socket
		for(i=0;i<result;++i) {

#if defined(_BSD)

			struct kevent& event(events[i]);
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
			// EVFILT_READ | EVFILT_WRITE | EV_EOF
			int error = 0;
			if (event.flags&EV_ERROR) {
				socklen_t len(sizeof(error));
				if (getsockopt(pSocket->_sockfd, SOL_SOCKET, SO_ERROR, (void *)&error, &len) == -1)
					error = Net::LastError();
			}
			if (event.flags&EV_EOF) // if close or shutdown RD = read (recv) => disconnection
				close(pSocket, error);
			else if (event.filter==EVFILT_READ)
				read(pSocket, error);
			else if (event.filter==EVFILT_WRITE)
				write(pSocket, error);
			else if (event.flags&EV_ERROR) // on few unix system we can get an error without anything else
				Action::Run(threadPool, make_shared<Action>("SocketError", error, pSocket), pSocket->_threadReceive);

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
			// EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP
			//printf("%u\n", event.events);
			int error = 0;
			if(event.events&EPOLLERR) {
				socklen_t len(sizeof(error));
				if(getsockopt(pSocket->_sockfd, SOL_SOCKET, SO_ERROR, (void *)&error, &len)==-1)
					error = Net::LastError();
			}
			if (event.events&EPOLLHUP) {
				event.events &= ~EPOLLIN;
				event.events &= ~EPOLLOUT;
			}
			if (event.events&EPOLLRDHUP) // => disconnection
				close(pSocket, error);
			else if (event.events&EPOLLIN) {
				if (event.events&EPOLLOUT && !error && pSocket->_firstWritable) // for first Flush requirement!
					write(pSocket, 0); // before read! Connection!
				read(pSocket, error);
			} else if (event.events&EPOLLOUT)
				write(pSocket, error);
			else if (event.events&EPOLLERR) // on few unix system we can get an error without anything else
				Action::Run(threadPool, make_shared<Action>("SocketError", error, pSocket), pSocket->_threadReceive);
#endif
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

	if (result < 0) { // error
		ex.set<Ex::Net::System>("impossible to manage sockets (error ", errno, ")");
		return false;
	}
	
#endif
	
	if (!_subscribers)
		return true; // IOSocket deletion
		
	ex.set<Ex::Net::System>("dies with remaining sockets managed");
	return false;
}
	


} // namespace Mona
