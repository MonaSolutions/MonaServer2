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
#include "Mona/Exceptions.h"
#include "Mona/Time.h"
#include <atomic>

#if defined(_WIN32)
#include <ws2tcpip.h>
#define NET_INVALID_SOCKET  INVALID_SOCKET
#define NET_SOCKET		    SOCKET
#define NET_SYSTEM			HWND
#define NET_SOCKLEN			int
#define NET_IOCTLREQUEST	int
#define NET_CLOSESOCKET(s)  closesocket(s)
#define NET_EINTR           WSAEINTR
#define NET_EACCES          WSAEACCES
#define NET_EFAULT          WSAEFAULT
#define NET_EINVAL          WSAEINVAL
#define NET_EMFILE          WSAEMFILE
#define NET_EWOULDBLOCK     WSAEWOULDBLOCK
#define NET_EINPROGRESS     WSAEINPROGRESS
#define NET_EALREADY        WSAEALREADY
#define NET_ENOTSOCK        WSAENOTSOCK
#define NET_EDESTADDRREQ    WSAEDESTADDRREQ
#define NET_EMSGSIZE        WSAEMSGSIZE
#define NET_EPROTOTYPE      WSAEPROTOTYPE
#define NET_ENOPROTOOPT     WSAENOPROTOOPT
#define NET_EPROTONOSUPPORT WSAEPROTONOSUPPORT
#define NET_ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#define NET_ENOTSUP         WSAEOPNOTSUPP
#define NET_EPFNOSUPPORT    WSAEPFNOSUPPORT
#define NET_EAFNOSUPPORT    WSAEAFNOSUPPORT
#define NET_EADDRINUSE      WSAEADDRINUSE
#define NET_EADDRNOTAVAIL   WSAEADDRNOTAVAIL
#define NET_ENETDOWN        WSAENETDOWN
#define NET_ENETUNREACH     WSAENETUNREACH
#define NET_ENETRESET       WSAENETRESET
#define NET_ECONNABORTED    WSAECONNABORTED
#define NET_ECONNRESET      WSAECONNRESET
#define NET_ENOBUFS         WSAENOBUFS
#define NET_EISCONN         WSAEISCONN
#define NET_ENOTCONN        WSAENOTCONN
#define NET_ESHUTDOWN       WSAESHUTDOWN
#define NET_ETIMEDOUT       WSAETIMEDOUT
#define NET_ECONNREFUSED    WSAECONNREFUSED
#define NET_EHOSTDOWN       WSAEHOSTDOWN
#define NET_EHOSTUNREACH    WSAEHOSTUNREACH
#define NET_ESYSNOTREADY    WSASYSNOTREADY
#define NET_ENOTINIT        WSANOTINITIALISED
#define NET_HOST_NOT_FOUND  WSAHOST_NOT_FOUND
#define NET_TRY_AGAIN       WSATRY_AGAIN
#define NET_NO_RECOVERY     WSANO_RECOVERY
#define NET_NO_DATA         WSANO_DATA
#define NET_VERNOTSUPPORTED	WSAVERNOTSUPPORTED
#define NET_EPROCLIM		WSAEPROCLIM
#else
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <net/if.h>
#if defined(sun) || defined(__APPLE__)
#include <sys/sockio.h>
#include <sys/filio.h>
#endif
#define NET_INVALID_SOCKET  -1
#define NET_SOCKET           int
#define NET_SOCKLEN          socklen_t
#define NET_SYSTEM			 int
#if defined(_BSD)
#define NET_IOCTLREQUEST     unsigned long
#else
#define NET_IOCTLREQUEST     int
#endif
#define NET_CLOSESOCKET(s)  ::close(s)
#define NET_EINTR           EINTR
#define NET_EACCES          EACCES
#define NET_EFAULT          EFAULT
#define NET_EINVAL          EINVAL
#define NET_EMFILE          EMFILE
#define NET_EWOULDBLOCK     EWOULDBLOCK
#define NET_EINPROGRESS     EINPROGRESS
#define NET_EALREADY        EALREADY
#define NET_ENOTSOCK        ENOTSOCK
#define NET_EDESTADDRREQ    EDESTADDRREQ
#define NET_EMSGSIZE        EMSGSIZE
#define NET_EPROTOTYPE      EPROTOTYPE
#define NET_ENOPROTOOPT     ENOPROTOOPT
#define NET_EPROTONOSUPPORT EPROTONOSUPPORT
#if defined(ESOCKTNOSUPPORT)
#define NET_ESOCKTNOSUPPORT ESOCKTNOSUPPORT
#else
#define NET_ESOCKTNOSUPPORT -1
#endif
#define NET_ENOTSUP         ENOTSUP
#define NET_EPFNOSUPPORT    EPFNOSUPPORT
#define NET_EAFNOSUPPORT    EAFNOSUPPORT
#define NET_EADDRINUSE      EADDRINUSE
#define NET_EADDRNOTAVAIL   EADDRNOTAVAIL
#define NET_ENETDOWN        ENETDOWN
#define NET_ENETUNREACH     ENETUNREACH
#define NET_ENETRESET       ENETRESET
#define NET_ECONNABORTED    ECONNABORTED
#define NET_ECONNRESET      ECONNRESET
#define NET_ENOBUFS         ENOBUFS
#define NET_EISCONN         EISCONN
#define NET_ENOTCONN        ENOTCONN
#if defined(ESHUTDOWN)
#define NET_ESHUTDOWN       ESHUTDOWN
#else
#define NET_ESHUTDOWN       -2
#endif
#define NET_ETIMEDOUT       ETIMEDOUT
#define NET_ECONNREFUSED    ECONNREFUSED
#if defined(EHOSTDOWN)
#define NET_EHOSTDOWN       EHOSTDOWN
#else
#define NET_EHOSTDOWN       -3
#endif
#define NET_EHOSTUNREACH    EHOSTUNREACH
#define NET_ESYSNOTREADY    -4
#define NET_ENOTINIT        -5
#define NET_HOST_NOT_FOUND  HOST_NOT_FOUND
#define NET_TRY_AGAIN       TRY_AGAIN
#define NET_NO_RECOVERY     NO_RECOVERY
#define NET_NO_DATA         NO_DATA
#if defined(VERNOTSUPPORTED)
#define NET_VERNOTSUPPORTED VERNOTSUPPORTED
#else
#define NET_VERNOTSUPPORTED -6
#endif
#if defined(EPROCLIM)
#define NET_EPROCLIM		EPROCLIM
#else
#define NET_EPROCLIM		-7
#endif
#endif


#if !defined(AI_ADDRCONFIG)
#define AI_ADDRCONFIG 0
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xFFFFFFFF
#endif

#if !defined(IPV6_ADD_MEMBERSHIP) && defined(IPV6_JOIN_GROUP)
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif
#if !defined(IPV6_DROP_MEMBERSHIP) && defined(IPV6_LEAVE_GROUP)
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif


//
// Automatically link Base library.
//
#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif

namespace Mona {


struct SocketAddress;
struct Net : virtual Object {
	enum {
		RTO_MIN = 1000u, // see https://tools.ietf.org/html/rfc2988
		RTO_INIT = 3000u,
		RTO_MAX = 10000u,
		MTU_RELIABLE_SIZE = 1200u, // 1280 - ~30 for DTLS - 40 for IPv6 - 8 for UDP
		SCTP_HEADER_SIZE = 28u
	};


	static UInt32 GetRecvBufferSize() { return _Net._recvBufferSize; }
	static void   SetRecvBufferSize(UInt32 size) { _Net._recvBufferSize = size; }
	static void	  ResetRecvBufferSize() { _Net._recvBufferSize = _Net._recvBufferDefaultSize; }
	static UInt32 GetSendBufferSize() { return _Net._sendBufferSize; }
	static void	  SetSendBufferSize(UInt32 size) { _Net._sendBufferSize = size; }
	static void	  ResetSendBufferSize() { _Net._sendBufferSize = _Net._sendBufferDefaultSize; }

	static UInt32 GetInterfaceIndex(const SocketAddress& address);

	static UInt16 ResolvePort(Exception& ex, const char* service);
	static UInt16 ResolvePort(Exception& ex, const std::string& service) { return ResolvePort(ex, service.c_str()); }

#if defined(_WIN32)
	static int  LastError() { return WSAGetLastError(); }
#else
	static int  LastError() { int error = errno;  return error == EAGAIN ? NET_EWOULDBLOCK : error; }
#endif

	static const char* ErrorToMessage(int error);
	static const char* LastErrorMessage() { return ErrorToMessage(LastError()); }

	struct Stats : virtual Object {
		virtual Time	recvTime() const = 0;
		virtual UInt64	recvByteRate() const = 0;
		virtual double	recvLostRate() const { return 0; }
		
		virtual Time	sendTime() const = 0;
		virtual UInt64	sendByteRate() const = 0;
		virtual double	sendLostRate() const { return 0; }

		virtual UInt64	queueing() const = 0;

		static Stats& Null();
	};

private:

	Net();
	~Net();

	std::atomic<UInt32> _recvBufferSize;
	std::atomic<UInt32> _sendBufferSize;
	int					_recvBufferDefaultSize;
	int					_sendBufferDefaultSize;

	static Net _Net;
};

} // namespace Mona
