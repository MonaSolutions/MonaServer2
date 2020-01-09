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

#include "Mona/Net.h"
#include "Mona/SocketAddress.h"
#if defined(_WIN32)
#include <Iphlpapi.h>
#else
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/resource.h>
#endif

using namespace std;

namespace Mona {

Net Net::_Net;

Net::Stats& Net::Stats::Null() {
	static struct Null : Stats, virtual Object {
		Time	recvTime() const { return 0; }
		UInt64	recvByteRate() const { return 0; }
		Time	sendTime() const { return 0; }
		UInt64	sendByteRate() const { return 0; }
		UInt64	queueing() const { return 0; }
	} Null;
	return Null;
}

const char* Net::ErrorToMessage(int error) {
	switch (error) {
		case NET_ESYSNOTREADY: return "Net subsystem not ready";
		case NET_ENOTINIT: return "Net subsystem not initialized";
		case NET_EINTR: return "Interrupted";
		case NET_EACCES: return "Permission denied";
		case NET_EFAULT: return "Bad address parameter";
		case NET_EINVAL: return "Invalid argument";
		case NET_EMFILE: return "Too many open sockets";
		case NET_EWOULDBLOCK: return "Operation would block";
		case NET_EINPROGRESS: return "Operation now in progress";
		case NET_EALREADY: return "Operation already in progress";
		case NET_ENOTSOCK: return "Socket operation attempted on a non-socket or uninitialized socket";
		case NET_EDESTADDRREQ: return "Destination address required";
		case NET_EMSGSIZE: return "Message too long";
		case NET_EPROTOTYPE: return "Wrong protocol type";
		case NET_ENOPROTOOPT: return "Protocol not available";
		case NET_EPROTONOSUPPORT: return "Protocol not supported";
		case NET_ESOCKTNOSUPPORT: return "Socket type not supported";
		case NET_ENOTSUP: return "Operation not supported";
		case NET_EPFNOSUPPORT: return "Protocol family not supported";
		case NET_EAFNOSUPPORT: return "Address family not supported";
		case NET_EADDRINUSE: return "Address already in use";
		case NET_EADDRNOTAVAIL: return "Cannot assign requested address";
		case NET_ENETDOWN: return "Network is down";
		case NET_ENETUNREACH: return "Network is unreachable";
		case NET_ENETRESET: return "Network dropped connection on reset";
		case NET_ECONNABORTED: return "Connection aborted";
		case NET_ECONNRESET: return "Connection reseted";
		case NET_ENOBUFS: return "No buffer space available";
		case NET_EISCONN: return "Socket is already connected";
		case NET_ENOTCONN: return "Socket is not connected";
		case NET_ESHUTDOWN: return "Cannot read/write after socket shutdown";
		case NET_ETIMEDOUT: return "Timeout";
		case NET_ECONNREFUSED: return "Connection refused";
		case NET_EHOSTDOWN: return "Host is down";
		case NET_EHOSTUNREACH: return "No route to host";
		case NET_VERNOTSUPPORTED: return "Net subsystem version required not available";
		case NET_EPROCLIM: return "Net subsystem maximum load reached";
#if !defined(_WIN32)
		case EPIPE: return "Broken pipe";
#endif
		default: break;
	}
	return "I/O error";
}


Net::Net() {
#if defined(_WIN32)
	WORD    version = MAKEWORD(2, 2);
	WSADATA data;
	if (WSAStartup(version, &data) != 0)
		FATAL_ERROR("Impossible to initialize network (version ", version, "), ", Net::LastErrorMessage());
#else
	// remove ulimit!
	struct rlimit limit;
	limit.rlim_cur = RLIM_INFINITY;
	limit.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_NOFILE, &limit) != 0 && getrlimit(RLIMIT_NOFILE, &limit) == 0) {
		limit.rlim_cur = limit.rlim_max;
		setrlimit(RLIMIT_NOFILE, &limit);
	}
#endif
	// Get Default Recv/Send buffer size, connect before because on some targets environment it's required (otherwise SO_RCVBUF/SO_SNDBUF returns 0)
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(12345);
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	NET_SOCKET sockfd;;
	if ((sockfd = ::socket(AF_INET, SOCK_DGRAM, 0)) == NET_INVALID_SOCKET || ::connect(sockfd, (const sockaddr*)&address, sizeof(address)))
		FATAL_ERROR("Impossible to initialize socket system, ", Net::LastErrorMessage());
	NET_SOCKLEN length(sizeof(_recvBufferDefaultSize));
	if (::getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&_recvBufferDefaultSize), &length) == -1)
		FATAL_ERROR("Impossible to initialize socket receiving buffer size, ", Net::LastErrorMessage());
	_recvBufferSize = _recvBufferDefaultSize;
	if (::getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&_sendBufferDefaultSize), &length) == -1)
		FATAL_ERROR("Impossible to initialize socket sending buffer size, ", Net::LastErrorMessage());
	_sendBufferSize = _sendBufferDefaultSize;
	NET_CLOSESOCKET(sockfd);
}

Net::~Net() {
#if defined(_WIN32)
	WSACleanup();
#endif
}

UInt16 Net::ResolvePort(Exception& ex, const char* service) {
	struct servent* se = getservbyname(service, NULL);
	if (se)
		return ntohs(se->s_port);
	ex.set<Ex::Net::Address::Port>(service, " port service undefined");
	return 0;
}

UInt32 Net::GetInterfaceIndex(const SocketAddress& address) {
	if (!address.host())
		return 0;
#if defined(_WIN32)
	DWORD index = 0;
	return GetBestInterfaceEx((sockaddr *)address.data(), &index)==NO_ERROR ? index : 0;
#elif !defined(__ANDROID__)
	ifaddrs* firstNetIf = NULL;
	getifaddrs(&firstNetIf);
	ifaddrs* netIf = NULL;
	for (netIf = firstNetIf; netIf; netIf = netIf->ifa_next) {
		if (netIf->ifa_addr && netIf->ifa_addr->sa_family == address.family() && memcmp(address.data(), netIf->ifa_addr, sizeof(sockaddr) - 4) == 0) // -4 trick to compare just host ip (ignore port)
			break;
	}
	if (firstNetIf)
		freeifaddrs(firstNetIf);
	return netIf ? if_nametoindex(netIf->ifa_name) : 0;
#else // getifaddrs is not supported on Android so we use ioctl
	// Create UDP socket
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0)
		return 0;

	// Get ip config
	struct ifconf ifc;
	memset(&ifc, 0, sizeof(ifconf));

	struct ifreq * ifr = NULL;
	if (::ioctl(fd, SIOCGIFCONF, &ifc) >= 0) {
		Buffer buffer(ifc.ifc_len);
		ifc.ifc_buf = (caddr_t)buffer.data();
		if (::ioctl(fd, SIOCGIFCONF, &ifc) >= 0) {

			// Read all addresses
			for (int i = 0; i < ifc.ifc_len; i += sizeof(struct ifreq)) {
				ifr = (struct ifreq *)(ifc.ifc_buf + i);

				if (ifr->ifr_addr.sa_family == address.family() && memcmp(address.data(), &ifr->ifr_addr, sizeof(sockaddr) - 4) == 0) // -4 trick to compare just host ip (ignore port)
					break;
			}
		}
	}
	close(fd); // close socket
	return ifr ? if_nametoindex(ifr->ifr_name) : 0;
#endif
}


} // namespace Mona
