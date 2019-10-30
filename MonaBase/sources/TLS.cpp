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


#include "Mona/TLS.h"


using namespace std;

namespace Mona {

bool TLS::Create(Exception& ex, shared<TLS>& pTLS, const SSL_METHOD* method) {
	// load and configure in constructor to be thread safe!
	SSL_CTX* pCTX(SSL_CTX_new(method));
	if (pCTX) {
		/* If the underlying BIO is blocking, SSL_read()/SSL_write() will only return, once the read operation has been finished or an error occurred,
		except when a renegotiation take place, in which case a SSL_ERROR_WANT_READ may occur.
		This behaviour can be controlled with the SSL_MODE_AUTO_RETRY flag of the SSL_CTX_set_mode call. */
		SSL_CTX_set_mode(pCTX, SSL_MODE_AUTO_RETRY);
		pTLS = new TLS(pCTX);
		return true;
	}
	ex.set<Ex::Extern::Crypto>(Crypto::LastErrorMessage());
	return false;
}

bool TLS::Create(Exception& ex, const char* cert, const char* key, shared<TLS>& pTLS, const SSL_METHOD* method) {
	// load and configure in constructor to be thread safe!
	SSL_CTX* pCTX(SSL_CTX_new(method));
	if (pCTX) {
		// cert.pem & key.pem
		if (SSL_CTX_use_certificate_file(pCTX, cert, SSL_FILETYPE_PEM) == 1 && SSL_CTX_use_PrivateKey_file(pCTX, key, SSL_FILETYPE_PEM) == 1) {
			/* If the underlying BIO is blocking, SSL_read()/SSL_write() will only return, once the read operation has been finished or an error occurred,
			except when a renegotiation take place, in which case a SSL_ERROR_WANT_READ may occur.
			This behaviour can be controlled with the SSL_MODE_AUTO_RETRY flag of the SSL_CTX_set_mode call. */
			SSL_CTX_set_mode(pCTX, SSL_MODE_AUTO_RETRY);
			pTLS = new TLS(pCTX);
			return true;
		}
		SSL_CTX_free(pCTX);
	}
	ex.set<Ex::Extern::Crypto>(Crypto::LastErrorMessage());
	return false;
}

TLS::Socket::Socket(Type type, const shared<TLS>& pTLS) : pTLS(pTLS), Mona::Socket(type), _ssl(NULL) {}

TLS::Socket::Socket(NET_SOCKET sockfd, const sockaddr& addr, const shared<TLS>& pTLS) : pTLS(pTLS), Mona::Socket(sockfd, addr), _ssl(NULL) {}

TLS::Socket::~Socket() {
	if (!_ssl)
		return;
	// gracefull disconnection => flush + shutdown + close
	/// shutdown + flush
	Exception ignore;
	flush(ignore, true);
	if(!SSL_in_init(_ssl)) // just if required (causes an error while init)
		SSL_shutdown(_ssl);
	SSL_free(_ssl);
}

UInt32 TLS::Socket::available() const {
	UInt32 available = Mona::Socket::available();
	if (!pTLS)
		return available; // normal socket
	lock_guard<mutex> lock(_mutex);
	if (!_ssl)
		return available;
	// don't call SSL_pending while handshake, it create a "wrong error 'don't call this function'"
	if (!SSL_is_init_finished(_ssl))
		return available ? 0x4000 : 0; // max buffer possible size
	if (available)
		return 0x4000 + SSL_pending(_ssl);  // max buffer possible size and has to read!
	return SSL_pending(_ssl); // max buffer possible size (nothing to read)
}

Mona::Socket* TLS::Socket::newSocket(Exception& ex, NET_SOCKET sockfd, const sockaddr& addr) {
	if(!pTLS)
		return Mona::Socket::newSocket(ex, sockfd, addr); // normal socket

	Socket* pSocket(new Socket(sockfd, addr, pTLS));
	pSocket->_ssl = SSL_new(pTLS->_pCTX);
	if (!pSocket->_ssl || SSL_set_fd(pSocket->_ssl, *pSocket) != 1) {
		// Certainly a TLS error context
		ex.set<Ex::Extern::Crypto>(Crypto::LastErrorMessage());
		delete pSocket;
		return NULL;
	}
	SSL_set_accept_state(pSocket->_ssl);
	return pSocket;
}

bool TLS::Socket::connect(Exception& ex, const SocketAddress& address, UInt16 timeout) {
	if (!Mona::Socket::connect(ex, address, timeout))
		return false;
	bool connecting = ex && ex.cast<Ex::Net::Socket>().code == NET_EWOULDBLOCK;
	if (connecting)
		ex = nullptr;
	if (!pTLS)
		return true;  // normal socket or already doing SSL negociation!

	lock_guard<mutex> lock(_mutex);
	if (_ssl) // already connected!
		return true;

	_ssl = SSL_new(pTLS->_pCTX);
	if (!_ssl || SSL_set_fd(_ssl, self) != 1) {
		// Certainly a TLS error context
		ex.set<Ex::Extern::Crypto>(Crypto::LastErrorMessage());
		return false;
	}
	
	SSL_set_connect_state(_ssl);
	// do the handshake now to send the client-hello message! (if non-blocking socket it's set before the call to connect)
	return connecting ? true : catchResult(ex, SSL_do_handshake(_ssl), " (address=", address, ")") >= 0;
}

int TLS::Socket::receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress) {
	if (!pTLS)
		return Mona::Socket::receive(ex, buffer, size, flags, pAddress); // normal socket

	unique_lock<mutex> lock(_mutex);
	if (!_ssl)
		return Mona::Socket::receive(ex, buffer, size, flags, pAddress); // normal socket

	if (!SSL_is_init_finished(_ssl)) {
		if (catchResult(ex, SSL_do_handshake(_ssl)) < 0)
			return -1;
		lock.unlock(); // always unlock to flush because cann call TLS::sendTo which relock _mutex
		// try to flush data queueing after handshake gotten!
		Mona::Socket::flush(ex, false);
		lock.lock();
	}

	int result = catchResult(ex, SSL_read(_ssl, buffer, size), " (from=", peerAddress(), ", size=", size, ")");
	// assign pAddress (no other way possible here)
	if(pAddress)
		pAddress->set(peerAddress());
	if(result > 0)
		Mona::Socket::receive(result);
	return result;
}

int TLS::Socket::sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags) {
	if (!pTLS)
		return Mona::Socket::sendTo(ex, data, size, address, flags); // normal socket
	lock_guard<mutex> lock(_mutex);
	if (!_ssl)
		return Mona::Socket::sendTo(ex, data, size, address, flags); // normal socket
	int result = catchResult(ex, SSL_write(_ssl, data, size), " (address=", address ? address : peerAddress(), ", size=", size, ")");
	if (result > 0)
		Mona::Socket::send(result);
	return result;
}

bool TLS::Socket::flush(Exception& ex, bool deleting) {
	// Call when Writable!
	if (!pTLS || queueing() || deleting) // if queueing a SLL_Write will do the handshake!
		return Mona::Socket::flush(ex, deleting);
	// maybe WRITE event for handshake need!
	unique_lock<mutex> lock(_mutex);
	if (!_ssl || catchResult(ex, SSL_do_handshake(_ssl)) > 0) {
		lock.unlock(); // always unlock to flush because can call TLS::sendTo which relock _mutex
		return Mona::Socket::flush(ex, deleting);
	}
	if (ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
		return false;
	ex = nullptr;
	return true;
}

bool TLS::Socket::close(Socket::ShutdownType type) {
	if(type && pTLS) { // shutdown just if SEND (or BOTH)
		lock_guard<mutex> lock(_mutex);
		if (_ssl && !SSL_in_init(_ssl)) // just if required (causes an error while init)
			SSL_shutdown(_ssl); // ignore error, it's the error of main socket which has priority
	}
	return Mona::Socket::close(type);
}

struct SSLInitializer {
	SSLInitializer() {
		SSL_library_init(); // can't fail!
		ERR_load_BIO_strings();
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
	}
	~SSLInitializer() { EVP_cleanup(); }
} _Initializer;

}  // namespace Mona
