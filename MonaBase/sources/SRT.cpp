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


#include "Mona/SRT.h"

using namespace std;

namespace Mona {


#if defined(SRT_API)

SRT::SRT() {
	if (::srt_startup())
		CRITIC("SRT startup, ", ::srt_getlasterror_str());
	::srt_setloghandler(nullptr, Log);
	::srt_setloglevel(4);
}

SRT::~SRT() {
	::srt_setloghandler(nullptr, nullptr);
	::srt_cleanup();
}

void SRT::Log(void* opaque, int level, const char* file, int line, const char* area, const char* message) {
	if (!Logs::GetLevel() || level>4)
		return;
	static LOG_LEVEL levels[5] = { LOG_CRITIC, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_TRACE};
	Logs::Log(levels[level], file, line, area, ", ", message);
}


int SRT::LastError() {
	int lastError;
	int error = ::srt_getlasterror(&lastError);
	if (lastError) {
#if !defined(_WIN32)
		if (lastError == EAGAIN)
			return NET_EWOULDBLOCK;
#endif
		return lastError;
	}
	switch (error) {
		case SRT_EUNKNOWN:
		//case SRT_SUCCESS:
			return NET_ENOTSUP;

		case SRT_ECONNSETUP:
		case SRT_ENOSERVER:
		case SRT_ECONNREJ:
			return NET_ECONNREFUSED;

		case SRT_ESOCKFAIL:
		case SRT_ESECFAIL:
			return NET_ENOTSUP;

		case SRT_ECONNFAIL:
		case SRT_ECONNLOST:
			return NET_ESHUTDOWN;
		case SRT_ENOCONN:
			return NET_ENOTCONN;

		case SRT_ERESOURCE:
		case SRT_ETHREAD:
		case SRT_ENOBUF:
			return NET_ENOTSUP;

		case SRT_EFILE:
		case SRT_EINVRDOFF:
		case SRT_ERDPERM:
		case SRT_EINVWROFF:
		case SRT_EWRPERM:
			return NET_ENOTSUP;

		case SRT_EINVOP:
			return NET_EINVAL;
		case SRT_EBOUNDSOCK:
		case SRT_ECONNSOCK:
			return NET_EISCONN;
		case SRT_EINVPARAM:
			return NET_EINVAL;
		case SRT_EINVSOCK:
			return NET_ENOTSOCK;
		case SRT_EUNBOUNDSOCK:
		case SRT_ENOLISTEN:
			return NET_ENOTCONN;
		case SRT_ERDVNOSERV:
			return NET_ENOTSUP;
		case SRT_ERDVUNBOUND:
			return NET_ENOTCONN;
		case SRT_EINVALMSGAPI:
		case SRT_EINVALBUFFERAPI:
		case SRT_EDUPLISTEN:
			return NET_ENOTSUP;
		case SRT_ELARGEMSG:
			return NET_EMSGSIZE;
		case SRT_EINVPOLLID:
			return NET_ENOTSUP;

		case SRT_EASYNCFAIL:
		case SRT_EASYNCSND:
		case SRT_EASYNCRCV:
			return NET_EWOULDBLOCK;
		case SRT_ETIMEOUT:
			return NET_ETIMEDOUT;
		case SRT_ECONGEST:
			return NET_ENOBUFS;
		case SRT_EPEERERR:
			return NET_EPROTONOSUPPORT;
	}
	return error; // not found
}

int SRT::Socket::ListenCallback(void* opaq, SRTSOCKET ns, int hsversion, const struct sockaddr* peeraddr, const char* streamid) {
	if (streamid) {
		SRT::Socket* pSRT = (SRT::Socket*)opaq;
		pSRT->_streamId = streamid; // streamid for next accept
	}
	return 0; // always accept the connection
};

SRT::Socket::Socket() : Mona::Socket(TYPE_OTHER), _shutdownRecv(false) {
	_id = ::srt_socket(AF_INET6, SOCK_DGRAM, 0);
	if (_id == ::SRT_INVALID_SOCK) {
		_id = NET_INVALID_SOCKET; // to avoid NET_CLOSESOCKET in Mona::Socket destruction
		SetException(_ex);
		return;
	}
	init();
	::srt_listen_callback(_id, &ListenCallback, this);
}

SRT::Socket::Socket(SRTSOCKET id, const sockaddr& addr) : Mona::Socket(id, addr, TYPE_OTHER), _shutdownRecv(false) {
	init();
}

bool SRT::Socket::processParams(Exception& ex, const Parameters& parameters, const char* prefix) {
	bool result = Mona::Socket::processParams(ex, parameters); // in first to override "net." possible same property by its "srt." version
	if (!result && ex.cast<Ex::Net::Socket>().code == NET_EISCONN) {
		// impossible to change srt socket params of base (low-level) one time connected
		result = true;
		ex = nullptr;
	}
	int value;
	Int64 i64Value;
	bool bValue;
	string stValue;
	// search always in priority with srt. prefix to be prioritary on general common version
	if (processParam(parameters, "pktdrop", bValue, prefix))
		result = setPktDrop(ex, bValue) && result;
	if (processParam(parameters, "encryption", value, prefix))
		result = setEncryptionSize(ex, value) && result;
	if (processParam(parameters, "passphrase", stValue, prefix))
		result = setPassphrase(ex, stValue.data(), stValue.size()) && result;
	if (processParam(parameters, "latency", value, prefix))
		result = setLatency(ex, value) && result;
	if (processParam(parameters, "peerlatency", value, prefix))
		result = setPeerLatency(ex, value) && result;
	if (processParam(parameters, "mss", value, prefix))
		result = setMSS(ex, value) && result;
	if (processParam(parameters, "overheadbw", value, prefix))
		result = setOverheadBW(ex, value) && result;
	if (processParam(parameters, "maxbw", i64Value, prefix))
		result = setMaxBW(ex, i64Value) && result;
	return result;
}

SRT::Socket::~Socket() {
	if (_id == NET_INVALID_SOCKET)
		return;
	// gracefull disconnection => flush + shutdown + close
	/// shutdown + flush
	Exception ignore;
	flush(ignore, true);
	close();
	::srt_listen_callback(_id, NULL, NULL); // remove callback if set
	_id = NET_INVALID_SOCKET; // to avoid NET_CLOSESOCKET in Mona::Socket destruction
}

bool SRT::Socket::close(Socket::ShutdownType type) {
	if (!type)
		return _shutdownRecv = true;
	return ::srt_close(_id) == 0;
}

UInt32 SRT::Socket::available() const {
	if(!_shutdownRecv) {
		Int32 events;
		Exception ignore;
		if(getOption(ignore, ::SRTO_EVENT, events) && (events&SRT_EPOLL_IN))
			return SRT_LIVE_DEF_PLSIZE; // just one packet!
	}
	return 0;
}

bool SRT::Socket::bind(Exception& ex, const SocketAddress& address) {
	if (_ex) {
		ex = _ex;
		return false;
	}

	if (_address) {
		SetException(NET_EISCONN, ex, " (address=", address, ")");
		return this->address() == address; // warn or error
	}

	// http://stackoverflow.com/questions/3757289/tcp-option-so-linger-zero-when-its-required =>
	/// If you must restart your server application which currently has thousands of client connections you might consider
	// setting this socket option to avoid thousands of server sockets in TIME_WAIT(when calling close() from the server end)
	// as this might prevent the server from getting available ports for new client connections after being restarted.
	setLinger(ex, true, 0); // avoid a SRT port overriding on server usage
	setReuseAddress(ex, true); // to avoid a conflict between address bind on not exactly same address (see http://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t)

	if (::srt_bind(_id, address.data(), address.size())) {
		SetException(ex, " (address=", address, ")");
		return false;
	}

	if (address)
		_address = address; // if port = 0, will be computed!
	else
		_address.set(IPAddress::Loopback(), 0); // to advise that address must be computed

	return true;
}

bool SRT::Socket::listen(Exception& ex, int backlog) {
	if (_ex) {
		ex = _ex;
		return false;
	}

	if (::srt_listen(_id, backlog) == 0) {
		_listening = true;
		return true;
	}
	SetException(ex, " (backlog=", backlog, ")");
	return false;
}

bool SRT::Socket::setNonBlockingMode(Exception& ex, bool value) {
	if (!setOption(ex, ::SRTO_SNDSYN, !value) || !setOption(ex, ::SRTO_RCVSYN, !value))
		return false;
	_nonBlockingMode = value;
	return true;
}

void SRT::Socket::computeAddress() {
	union {
		struct sockaddr_in  sa_in;
		struct sockaddr_in6 sa_in6;
	} addr;
	int addrSize = sizeof(addr);
	if (::srt_getsockname(_id, (sockaddr*)&addr, &addrSize) == 0)
		_address.set((sockaddr&)addr);
}

bool SRT::Socket::accept(Exception& ex, shared<Mona::Socket>& pSocket) {
	if (_ex) {
		ex = _ex;
		return false;
	}

	sockaddr_in6 scl;
	int sclen = sizeof scl;
	::SRTSOCKET sockfd = ::srt_accept(_id, (sockaddr*)&scl, &sclen);
	if (sockfd == SRT_INVALID_SOCK) {
		_streamId.clear();
		SetException(ex);
		return false;
	}
	SRT::Socket* pSRTSocket = new SRT::Socket(sockfd, (sockaddr&)scl);
	pSRTSocket->_streamId = move(_streamId);
	pSocket = pSRTSocket;
	return true;
}

bool SRT::Socket::connect(Exception& ex, const SocketAddress& address, UInt16 timeout) {
	if (_ex) {
		ex = _ex;
		return false;
	}

	if (timeout && !setConnectionTimeout(ex, timeout))
		return false;

	int result = ::srt_connect(_id, address.data(), address.size());
	if (result) {
		result = ::srt_getlasterror(NULL);
		if (_peerAddress || result == SRT_ECONNSOCK) { // if already connected (_peerAddress is true OR error ISCONN)
			if (_peerAddress == address)
				return true; // already connected to this address => no error
			SetException(NET_EISCONN, ex, " (address=", address, ")");
			return false;  // already connected to one other address => error
		}
		if (result != SRT_EASYNCFAIL && result != SRT_EASYNCSND && result != SRT_EASYNCRCV) {
			SetException(ex, " (address=", address, ")");
			return false; // fail
		}
		SetException(NET_EWOULDBLOCK, ex, " (address=", address, ")");
	}

	_address.set(IPAddress::Loopback(), 0); // to advise that address must be computed
	_peerAddress = address;
	return true;
}

int SRT::Socket::receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress) {
	if (_ex) {
		ex = _ex;
		return -1;
	}
	if (_shutdownRecv) {
		SetException(NET_ESHUTDOWN, ex);
		return -1;
	}

	// assign pAddress (no other way possible here)
	if(pAddress)
		pAddress->set(peerAddress());

	// Read the message
	int result = ::srt_recvmsg(_id, STR buffer, size); // /!\ SRT expect at least a buffer of 1316 bytes
	if (result == SRT_ERROR) {
		if (::srt_getlasterror(NULL) == SRT_ECONNLOST)
			return 0; // disconnection!
		SetException(ex, " (address=", pAddress ? *pAddress : _peerAddress, ", size=", size, ", flags=", flags, ")");
		return -1;
	}
	Mona::Socket::receive(result);

	if (!_address)
		_address.set(IPAddress::Loopback(), 0); // to advise that address is computable

	return result;
}

int SRT::Socket::sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags) {
	if (_ex) {
		ex = _ex;
		return -1;
	}

	if (!size || size > ::SRT_LIVE_DEF_PLSIZE) {
		SetException(NET_EINVAL, ex, " SRT: data sent must be in the range [1,1316]");
		return -1;
	}

	/*size_t chunk = 0;
	for (size_t i = 0; i < size; i += chunk) {
		chunk = min<size_t>(size - i, (size_t)SRT_LIVE_DEF_PLSIZE);
		if (::srt_send(_id, ((STR data) + i), chunk) < 0) {
			ex.set<Ex::Net::Socket>("SRT: send error; ", " ", ::srt_getlasterror_str());
			return -1;
		}
	}*/
	int sent = ::srt_send(_id, STR data, size);
	if (sent <= 0) {
		SetException(ex);
		return -1;
	}

	Mona::Socket::send(sent);

	if (!_address)
		_address.set(IPAddress::Loopback(), 0); // to advise that address is computable

	return sent;
}

bool SRT::Socket::setRecvBufferSize(Exception& ex, UInt32 size) {
	if (!setOption(ex, ::SRTO_UDP_RCVBUF, size))
		return false;
	_recvBufferSize = size;
	return true;
}
bool SRT::Socket::setSendBufferSize(Exception& ex, UInt32 size) {
	if (!setOption(ex, ::SRTO_UDP_SNDBUF, size))
		return false;
	_sendBufferSize = size;
	return true;
}

bool SRT::Socket::setPassphrase(Exception& ex, const char* data, UInt32 size) {
	if (_ex) {
		ex = _ex;
		return false;
	}
	if (::srt_setsockflag(_id, SRTO_PASSPHRASE, data, size) != -1)
		return true;
	SetException(ex);
	return false;
}

bool SRT::Socket::setStreamId(Exception& ex, const char* data, UInt32 size) {
	if (_ex) {
		ex = _ex;
		return false;
	}
	if (::srt_setsockflag(_id, SRTO_STREAMID, data, size) != -1)
		return true;
	SetException(ex);
	return false;
}

bool SRT::Socket::setLinger(Exception& ex, bool on, int seconds) {
	if (_ex) {
		ex = _ex;
		return false;
	}
	struct linger l;
	l.l_onoff = on ? 1 : 0;
	l.l_linger = seconds;
	return setOption(ex, ::SRTO_LINGER, l);
}

bool SRT::Socket::getLinger(Exception& ex, bool& on, int& seconds) const {
	if (_ex) {
		ex = _ex;
		return false;
	}
	struct linger l;
	if (!getOption(ex, ::SRTO_LINGER, l))
		return false;
	seconds = l.l_linger;
	on = l.l_onoff != 0;
	return true;
}

bool SRT::Socket::getStats(Exception& ex, SRT::Stats& stats) const {
	if (_ex) {
		ex = _ex;
		return false;
	}
	if (::srt_bistats(_id, &stats, 0, 0) != -1)
		return true;
	SetException(ex);
	return false;
}

#endif

}  // namespace Mona


