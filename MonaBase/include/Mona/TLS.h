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
#include "Mona/Crypto.h"
#include "Mona/Socket.h"
#include OpenSSL(ssl.h)


namespace Mona {

struct TLS : virtual Object {
	static bool Create(Exception& ex, shared<TLS>& pTLS, const SSL_METHOD* method = SSLv23_method());
	static bool Create(Exception& ex, const std::string& cert, const std::string& key, shared<TLS>& pTLS, const SSL_METHOD* method = SSLv23_method()) { return Create(ex, cert.c_str(), key.c_str(), pTLS, method); }
	static bool Create(Exception& ex, const char* cert, const char* key, shared<TLS>& pTLS, const SSL_METHOD* method = SSLv23_method());


	struct Socket : virtual Object, Mona::Socket {
		// http://fm4dd.com/openssl/sslconnect.htm
		// https://wiki.openssl.org/index.php/Simple_TLS_Server
		/*
		If pTLS is null it becomes a normal socket */
		Socket(Type type, const shared<TLS>& pTLS);
		~Socket();

		const shared<TLS>	pTLS;

		bool  isSecure() const { return pTLS ? true : false; }

		UInt32  available() const;
	
		bool connect(Exception& ex, const SocketAddress& address, UInt16 timeout = 0);
		int	 receive(Exception& ex, void* buffer, UInt32 size, int flags = 0) { return Mona::Socket::receive(ex, buffer, size, flags); }
		int	 sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags = 0);
		bool flush(Exception& ex) { return Mona::Socket::flush(ex); }

	private:
		int	 receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress);
		bool flush(Exception& ex, bool deleting);
		bool close(Socket::ShutdownType type = SHUTDOWN_BOTH);

		Mona::Socket* newSocket(Exception& ex, NET_SOCKET sockfd, const sockaddr& addr);

		// Create a socket from Socket::accept
		Socket(NET_SOCKET sockfd, const sockaddr& addr, const shared<TLS>& pTLS);

		template <typename ...Args>
		int catchResult(Exception& ex, int result, Args&&... args) {
			switch (SSL_get_error(_ssl, result)) {
				case SSL_ERROR_NONE:
					return result;
				case SSL_ERROR_ZERO_RETURN:
					return 0; // can happen when socket recv returns 0 
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					SetException(NET_EWOULDBLOCK, ex, std::forward<Args>(args)...);
					break;
					/*
					SSL_ERROR_SYSCALL
					Some I/O error occurred. The OpenSSL error queue may contain more information on the error.
					If the error queue is empty (i.e. ERR_get_error() returns 0), ret can be used to find out more about the error:
					If ret == 0, an EOF was observed that violates the protocol.
					If ret == -1, the underlying BIO reported an I/O error (for socket I/O on Unix systems, consult errno for details).

					SSL_ERROR_SSL
					A failure in the SSL library occurred, usually a protocol error. The OpenSSL error queue contains more information on the error. */
				case SSL_ERROR_SYSCALL:
					if (!Crypto::LastError()) {
						// Socket error!
						if(!result)
							return 0; // happen when socket recv returns 0
						SetException(ex, std::forward<Args>(args)...);
						break;
					}
				default: // mainly SSL_ERROR_SSL
					ex.set<Ex::Extern::Crypto>(Crypto::LastErrorMessage(), std::forward<Args>(args)...);
			}
			return -1;
		}

		ssl_st*				_ssl;
		mutable std::mutex	_mutex;
	};


	~TLS() { SSL_CTX_free(_pCTX); }
private:
	TLS(SSL_CTX* pCTX) : _pCTX(pCTX) {}
	
	SSL_CTX*  _pCTX;
};


} // namespace Mona
