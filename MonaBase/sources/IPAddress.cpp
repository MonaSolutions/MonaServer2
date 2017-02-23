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



#include "Mona/IPAddress.h"
#include "Mona/Byte.h"
#include "Mona/String.h"
#include "Mona/DNS.h"
#include "Mona/Util.h"
#if defined(_WIN32)
#include <Iphlpapi.h>
#else
#include <ifaddrs.h>
#endif

using namespace std;


namespace Mona {

	/// Returns the length of the mask (number of bits set in val).
	/// The val should be either all zeros or two contiguos areas of 1s and 0s. 
	/// The algorithm ignores invalid non-contiguous series of 1s and treats val 
	/// as if all bits between MSb and last non-zero bit are set to 1.
	UInt8 MaskBits(UInt32 val, UInt8 size) {
		UInt8 count = 0;
		if (val) {
			val = (val ^ (val - 1)) >> 1;
			for (count = 0; val; ++count)
				val >>= 1;
	} else
			count = size;
		return size - count;
	}

	bool IsIPv4Sock(const sockaddr& addr) {
		if (addr.sa_family == AF_INET)
			return true;
		const UInt16* words = reinterpret_cast<const UInt16*>(&reinterpret_cast<const sockaddr_in6&>(addr).sin6_addr);
		return !words[0] && !words[1] && !words[2] && !words[3] && !words[4] && Byte::From16Network(words[5]) == 0xFFFF;
	}

	struct IPAddress::IPImpl {
		virtual IPAddress::Family family() const = 0;

		virtual bool  isWildcard() const = 0;
		virtual bool  isBroadcast() const = 0;
		virtual bool  isAnyBroadcast() const = 0;
		virtual bool  isLoopback() const = 0;
		virtual bool  isMulticast() const = 0;
		virtual bool  isLinkLocal() const = 0;
		virtual bool  isSiteLocal() const = 0;
		virtual bool  isIPv4Mapped() const = 0;
		virtual bool  isIPv4Compatible() const = 0;
		virtual bool  isWellKnownMC() const = 0;
		virtual bool  isNodeLocalMC() const = 0;
		virtual bool  isLinkLocalMC() const = 0;
		virtual bool  isSiteLocalMC() const = 0;
		virtual bool  isOrgLocalMC() const = 0;
		virtual bool  isGlobalMC() const = 0;
		virtual UInt8 prefixLength() const = 0;

		virtual const void*		data() const = 0;
		virtual UInt8			size() const = 0;

		const in_addr&	ipv4() const { return reinterpret_cast<const in_addr&>(_addr.sin6_flowinfo); }
		const in6_addr&	ipv6() const { return _addr.sin6_addr; }

		UInt16  port() const { if (!_portSet) return 0; return Byte::From16Network(_addr.sin6_port); }
		UInt32	scope() const { return Byte::From32Network(_addr.sin6_scope_id); }

		const string& host() const { stringize(); return _host; }
		const string& address() const { stringize(); return _address; }

		const sockaddr_in6&  addr() const {
			if (!_portSet) {
				lock_guard<mutex> lock(_mutexPort);
				_addr.sin6_port = 0;
				_portSet = true;
			}
			return _addr;
		}

		bool portSet() const { return _portSet; }

		bool setPort(UInt16 port) {
			if (_portSet)
				return port == this->port();
			lock_guard<mutex> lock(_mutexPort);
			if (_portSet)
				return port == this->port();
			_addr.sin6_port = Byte::To16Network(port);
			_portSet = true;
			return true;
		}
	protected:
		IPImpl(IPAddress::Family family) : _stringized(false), _portSet(true) {
			// Wildcard => fixed port to 0!
			memset(&_addr, 0, sizeof(_addr));
			_addr.sin6_family = IPAddress::IPv6;
			if (family != AF_INET6)
				reinterpret_cast<UInt16*>(&_addr.sin6_addr)[5] = 0xFFFF; // 11 and 12 byte to FF
		}

		IPImpl(const sockaddr& addr) : _stringized(false), _portSet(true) {
			if (addr.sa_family == AF_INET6) {
				memcpy(&_addr, &addr, sizeof(sockaddr_in6));
				const UInt8* bytes(reinterpret_cast<UInt8*>(&_addr.sin6_addr));
				memcpy(&_addr.sin6_flowinfo, bytes + 12, 4);
		} else {
				memcpy(&_addr, &addr, sizeof(sockaddr_in));
				_addr.sin6_family = IPAddress::IPv6;
				_addr.sin6_scope_id = 0;
				computeIPv6(reinterpret_cast<in_addr&>(_addr.sin6_flowinfo));
			}
		}

		IPImpl(const sockaddr_in6& addr, const in_addr& addrv4) : _stringized(false), _portSet(true) {
			memcpy(&_addr, &addr, sizeof(addr));
			_addr.sin6_flowinfo = addrv4.s_addr;
		}
		IPImpl(const in6_addr& addr, UInt32 scope) : _stringized(false), _portSet(false) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_scope_id = Byte::To32Network(scope);
			_addr.sin6_flowinfo = 0;
			memcpy(&_addr.sin6_addr, &addr, sizeof(addr));
		}
		IPImpl(const in6_addr& addr, UInt32 scope, const in_addr& addrv4) : _stringized(false), _portSet(false) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_scope_id = Byte::To32Network(scope);
			_addr.sin6_flowinfo = addrv4.s_addr;
			memcpy(&_addr.sin6_addr, &addr, sizeof(addr));
		}
		IPImpl(const in6_addr& addr, UInt32 scope, UInt16 port) : _stringized(false), _portSet(true) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_port = Byte::To16Network(port);
			_addr.sin6_scope_id = Byte::To32Network(scope);
			_addr.sin6_flowinfo = 0;
			memcpy(&_addr.sin6_addr, &addr, sizeof(addr));
		}
		IPImpl(const in6_addr& addr, UInt32 scope, UInt16 port, const in_addr& addrv4) : _stringized(false), _portSet(true) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_port = Byte::To16Network(port);
			_addr.sin6_scope_id = Byte::To32Network(scope);
			_addr.sin6_flowinfo = addrv4.s_addr;
			memcpy(&_addr.sin6_addr, &addr, sizeof(addr));
		}
		IPImpl(const in_addr& addr) : _stringized(false), _portSet(false) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_scope_id = 0;
			_addr.sin6_flowinfo = addr.s_addr; // copy IPv4 in flowinfo!
			computeIPv6(addr);
		}
		IPImpl(const in_addr& addr, UInt16 port) : _stringized(false), _portSet(true) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_port = Byte::To16Network(port);
			_addr.sin6_scope_id = 0;
			_addr.sin6_flowinfo = addr.s_addr; // copy IPv4 in flowinfo!
			computeIPv6(addr);
		}
		IPImpl(const in_addr& addr, const in_addr& mask, const in_addr& set) : _stringized(false), _portSet(false) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_scope_id = 0;
			_addr.sin6_flowinfo = (addr.s_addr & mask.s_addr) | (set.s_addr & ~mask.s_addr);
			computeIPv6(addr);
		}
		IPImpl(const in_addr& addr, const in_addr& mask, const in_addr& set, UInt16 port) : _stringized(false), _portSet(true) {
			_addr.sin6_family = IPAddress::IPv6;
			_addr.sin6_port = Byte::To16Network(port);
			_addr.sin6_scope_id = 0;
			_addr.sin6_flowinfo = (addr.s_addr & mask.s_addr) | (set.s_addr & ~mask.s_addr);
			computeIPv6(addr);
		}

	private:
		virtual void stringize(string& host, string& address) const = 0;

		void computeIPv6(const in_addr& addr) {
			UInt8* bytes(reinterpret_cast<UInt8*>(&_addr.sin6_addr));

			memset(bytes, 0, 10);
			memset(bytes + 10, 0xFF, 2); // 11 and 12 byte to FF
			memcpy(bytes + 12, &addr, 4);
		}

		void stringize() const {
			if (_stringized)
				return;
			lock_guard<mutex> lock(_mutex);
			if (_stringized)
				return;
			stringize(_host, _address);
			_stringized = true;
		}


		mutable sockaddr_in6	_addr;

		mutable mutex			_mutexPort;
		mutable volatile bool	_portSet;

		mutable mutex			_mutex;
		mutable string			_host;
		mutable string			_address;
		mutable volatile bool	_stringized;
	};




	struct IPAddress::IPv4Impl : IPAddress::IPImpl, virtual Object {
		IPv4Impl() : IPImpl(IPAddress::IPv4) {} // Willcard
		IPv4Impl(const sockaddr& addr) : IPImpl(addr) {}
		IPv4Impl(const in_addr& addr) : IPImpl(addr) {}
		IPv4Impl(const in_addr& addr, UInt16 port) : IPImpl(addr, port) {}
		IPv4Impl(const in_addr& addr, const in_addr& mask, const in_addr& set) : IPImpl(addr, mask, set) {}
		IPv4Impl(const in_addr& addr, const in_addr& mask, const in_addr& set, UInt16 port) : IPImpl(addr, mask, set, port) {}

		const void*		data() const { return &ipv4(); }
		UInt8			size() const { return sizeof(ipv4()); }


		IPAddress::Family	family() const { return IPAddress::IPv4; }

		bool	isWildcard() const { return ipv4().s_addr == INADDR_ANY; }
		bool	isBroadcast() const { return ipv4().s_addr == INADDR_NONE; }
		bool	isAnyBroadcast() const { return ipv4().s_addr == INADDR_NONE || (Byte::From32Network(ipv4().s_addr) & 0x000000FF) == 0x000000FF; }
		bool	isLoopback() const { return (Byte::From32Network(ipv4().s_addr) & 0xFF000000) == 0x7F000000; } // 127.0.0.1 to 127.255.255.255 
		bool	isMulticast() const { return (Byte::From32Network(ipv4().s_addr) & 0xF0000000) == 0xE0000000; } // 224.0.0.0/24 to 239.0.0.0/24 
		bool	isLinkLocal() const { return (Byte::From32Network(ipv4().s_addr) & 0xFFFF0000) == 0xA9FE0000; } // 169.254.0.0/16

		bool	isSiteLocal() const {
			UInt32 addr = Byte::From32Network(ipv4().s_addr);
			return (addr & 0xFF000000) == 0x0A000000 ||        // 10.0.0.0/24
				(addr & 0xFFFF0000) == 0xC0A80000 ||        // 192.68.0.0/16
				(addr >= 0xAC100000 && addr <= 0xAC1FFFFF); // 172.16.0.0 to 172.31.255.255
		}

		bool isIPv4Compatible() const { return true; }
		bool isIPv4Mapped() const { return true; }

		bool isWellKnownMC() const { return (Byte::From32Network(ipv4().s_addr) & 0xFFFFFF00) == 0xE0000000; } // 224.0.0.0/8
		bool isNodeLocalMC() const { return false; }
		bool isLinkLocalMC() const { return (Byte::From32Network(ipv4().s_addr) & 0xFF000000) == 0xE0000000; } // 244.0.0.0/24
		bool isSiteLocalMC() const { return (Byte::From32Network(ipv4().s_addr) & 0xFFFF0000) == 0xEFFF0000; } // 239.255.0.0/16
		bool isOrgLocalMC() const { return (Byte::From32Network(ipv4().s_addr) & 0xFFFF0000) == 0xEFC00000; } // 239.192.0.0/16

		bool isGlobalMC() const {
			UInt32 addr = Byte::From32Network(ipv4().s_addr);
			return addr >= 0xE0000100 && addr <= 0xEE000000; // 224.0.1.0 to 238.255.255.255
		}

		UInt8 prefixLength() const {
			return MaskBits(Byte::From32Network(ipv4().s_addr), 32);
		}

		template<typename ...Args>
		static bool Parse(const char* address, shared<IPImpl>& pAddress) {
			if (String::ICompare(address, "localhost") == 0)
				address = "127.0.0.1";
			in_addr ia;
#if defined(_WIN32) 
			ia.s_addr = inet_addr(address);
			if (ia.s_addr == INADDR_NONE && strcmp(address, "255.255.255.255") != 0)
				return false;
#else
			if (!inet_aton(address, &ia))
				return false;
#endif
			pAddress.reset(new IPv4Impl(ia));
			return true;
		}


	private:

		void stringize(string& host, string& address) const {
			const UInt8* bytes = reinterpret_cast<const UInt8*>(&ipv4());
			String::Assign(host, bytes[0], '.', bytes[1], '.', bytes[2], '.', bytes[3]);
			String::Assign(address, host, ":", port());
		}

	};


	struct IPAddress::IPv6Impl : IPAddress::IPImpl, virtual Object {
		IPv6Impl() : IPImpl(IPAddress::IPv6) {} // Willcard
		IPv6Impl(const sockaddr& addr) : IPImpl(addr) {}
		IPv6Impl(const in6_addr& addr, UInt32 scope) : IPImpl(addr, scope) {}
		IPv6Impl(const in6_addr& addr, UInt32 scope, UInt16 port) : IPImpl(addr, scope, port) {}

		const void*		data() const { return &ipv6(); }
		UInt8			size() const { return sizeof(ipv6()); }


		IPAddress::Family	family() const { return IPAddress::IPv6; }

		bool			isBroadcast() const { return false; }
		bool			isAnyBroadcast() const { return false; }

		bool isWildcard() const {
			const UInt64* words = reinterpret_cast<const UInt64*>(&ipv6());
			return !words[0] && !words[1];
		}
		bool isLoopback() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return !words[0] && !words[1] && !words[2] && !words[3] && !words[4] && !words[5] && !words[6] && Byte::From16Network(words[7]) == 1;
		}
		bool isMulticast() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFE0) == 0xFF00;
		}
		bool isLinkLocal() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFE0) == 0xFE80;
		}
		bool isSiteLocal() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFE0) == 0xFEC0;
		}
		bool isIPv4Compatible() const {
			const UInt32* words = reinterpret_cast<const UInt32*>(&ipv6());
			return !words[0] && !words[1] && !words[2];
		}
		bool isIPv4Mapped() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return !words[0] && !words[1] && !words[2] && !words[3] && !words[4] && Byte::From16Network(words[5]) == 0xFFFF;
		}
		bool isWellKnownMC() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFF0) == 0xFF00;
		}
		bool isNodeLocalMC() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFEF) == 0xFF01;
		}
		bool isLinkLocalMC() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFEF) == 0xFF02;
		}
		bool isSiteLocalMC() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFEF) == 0xFF05;
		}
		bool isOrgLocalMC() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFEF) == 0xFF08;
		}
		bool isGlobalMC() const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			return (Byte::From16Network(words[0]) & 0xFFEF) == 0xFF0F;
		}

		UInt8 prefixLength() const {
			UInt8 bits = 0;
			UInt8 bitPos = 128;
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			for (int i = 7; i >= 0; --i) {
				if ((bits = MaskBits(Byte::From16Network(words[i]), 16)))
					return (bitPos - (16 - bits));
				bitPos -= 16;
			}
			return 0;
		}

		static bool Parse(const char* address, shared<IPImpl>& pAddress) {
			if (!*address)
				return false;
#if defined(_WIN32)
			if (String::ICompare(address, "localhost") == 0)
				address = "::1";
			addrinfo* pAI;
			addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = AI_NUMERICHOST;
			if (getaddrinfo(address, NULL, &hints, &pAI))
				return false;
			pAddress.reset(new IPv6Impl(reinterpret_cast<struct sockaddr_in6*>(pAI->ai_addr)->sin6_addr, reinterpret_cast<struct sockaddr_in6*>(pAI->ai_addr)->sin6_scope_id));
			freeaddrinfo(pAI);
#else
			in6_addr ia;
			const char* scoped(strchr(address, '%'));
			UInt32	 scope(0);
			if (scoped) {
				if (!(scope = if_nametoindex(scoped + 1)))
					return false;
				if (*address == '[')
					++address;
				if (inet_pton(AF_INET6, string(address, scoped - address).c_str(), &ia) != 1)
					return false;
		} else {
				if (String::ICompare(address, "localhost") == 0)
					address = "::1";
				if (*address == '[') {
					if (inet_pton(AF_INET6, string(address + 1, strlen(address) - 2).c_str(), &ia) != 1)
						return false;
			} else if (inet_pton(AF_INET6,address, &ia) != 1)
					return false;
			}
			pAddress.reset(new IPv6Impl(ia, scope));
#endif
			return true;
		}

	private:

		void stringize(string& host, string& address) const {
			const UInt16* words = reinterpret_cast<const UInt16*>(&ipv6());
			if ((isIPv4Compatible() && !isWildcard() && !isLoopback()) || isIPv4Mapped()) {
				const UInt8* bytes = reinterpret_cast<const UInt8*>(words);
				String::Assign(host, words[5] == 0 ? "::" : "::FFFF:", bytes[12], '.', bytes[13], '.', bytes[14], '.', bytes[15]);
		} else {
				bool zeroSequence = false;
				int i = 0;
				while (i < 8) {
					if (!zeroSequence && words[i] == 0) {
						int zi = i;
						while (zi < 8 && words[zi] == 0)
							++zi;
						if (zi > i + 1) {
							i = zi;
							host.append(":");
							zeroSequence = true;
						}
					}
					if (i > 0)
						host.append(":");
					if (i < 8)
						String::Append(host, String::Hex(BIN &words[i++], 2, HEX_TRIM_LEFT));
				}
				UInt32 scope(this->scope());
				if (scope) {
					host.append("%");
#if defined(_WIN32)
					String::Append(host, scope);
#else
					char buffer[IFNAMSIZ];
					if (if_indextoname(scope, buffer))
						host.append(buffer);
					else
						String::Append(host, scope);
#endif
				}
			}
			String::Assign(address, '[', host, ']', ':', port());
		}


	};


	//
	// IPAddress
	//


	IPAddress::LocalAddresses::LocalAddresses() {
#if defined (_WIN32)
		Buffer buffer(sizeof(IP_ADAPTER_ADDRESSES));
		ULONG size = sizeof(IP_ADAPTER_ADDRESSES);
		PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES *)buffer.data(), adapt = NULL;
		PIP_ADAPTER_UNICAST_ADDRESS aip = NULL;

		// we start with 1 address only to get the size of the buffer to allocate
		ULONG res = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &size);
		if (res == ERROR_BUFFER_OVERFLOW) {
			buffer.resize(size);
			res = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses = (IP_ADAPTER_ADDRESSES *)buffer.data(), &size);
		}
		if (res != NO_ERROR) {
			emplace_back(Loopback(IPAddress::IPv4));
			return;
		}

		for (adapt = pAddresses; adapt; adapt = adapt->Next) {
			for (aip = adapt->FirstUnicastAddress; aip; aip = aip->Next) {
				if (aip->Address.lpSockaddr->sa_family == AF_INET6)
					emplace_back(reinterpret_cast<struct sockaddr_in6*>(aip->Address.lpSockaddr)->sin6_addr, reinterpret_cast<struct sockaddr_in6*>(aip->Address.lpSockaddr)->sin6_scope_id);
				else if (aip->Address.lpSockaddr->sa_family == AF_INET)
					emplace_back(reinterpret_cast<struct sockaddr_in*>(aip->Address.lpSockaddr)->sin_addr);
			}
		}
#else
		struct ifaddrs * ifAddrStruct = NULL;
		if (getifaddrs(&ifAddrStruct) == -1) {
			emplace_back(Loopback(IPAddress::IPv4));
			return;
		}

		for (struct ifaddrs * ifa = ifAddrStruct; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr) // address can be empty
				continue;

			if (ifa->ifa_addr->sa_family == AF_INET6)
				emplace_back(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr, reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_scope_id);
			else if (ifa->ifa_addr->sa_family == AF_INET)
				emplace_back(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr);
		}
		freeifaddrs(ifaddrs);
#endif
	}

	struct IPBroadcaster : public IPAddress {
	public:
		IPBroadcaster() : IPAddress(IPv4) {
			struct in_addr ia;
			ia.s_addr = INADDR_BROADCAST;
			set(ia);
		}
	};

	class IPLoopback : public IPAddress {
	public:
		IPLoopback(Family family) : IPAddress(family) {
			if (family == IPv6) {
				struct in6_addr	addr;
				memset(&addr, 0, sizeof(addr));
				reinterpret_cast<UInt16*>(&addr)[7] = Byte::To16Network(1);
				set(addr);
		} else {
				struct in_addr	addr;
				addr.s_addr = Byte::To32Network(INADDR_LOOPBACK);
				set(addr);
			}
		}
	};

	IPAddress::IPAddress(IPImpl* pAddress) : _pIPAddress(pAddress) {}

	IPAddress::IPAddress(Family family) : _pIPAddress(Wildcard(family)._pIPAddress) {}

	IPAddress::IPAddress(const IPAddress& other) : _pIPAddress(other._pIPAddress) {}
	IPAddress::IPAddress(const IPAddress& other, UInt16 port) : _pIPAddress(other._pIPAddress) { setPort(port); }

	IPAddress::IPAddress(const in_addr& addr) : _pIPAddress(new IPv4Impl(addr)) {}
	IPAddress::IPAddress(const in6_addr& addr, UInt32 scope) : _pIPAddress(new IPv6Impl(addr, scope)) {}

	IPAddress::IPAddress(const sockaddr& addr) : _pIPAddress(IsIPv4Sock(addr) ? (IPImpl*)new IPv4Impl(addr) : (IPImpl*)new IPv6Impl(addr)) {
	}

	void IPAddress::setPort(UInt16 port) {
		if (_pIPAddress->setPort(port))
			return;
		if (family() == IPv6)
			_pIPAddress.reset(new IPv6Impl(_pIPAddress->ipv6(), _pIPAddress->scope(), port));
		else
			_pIPAddress.reset(new IPv4Impl(_pIPAddress->ipv4(), port));
	}

IPAddress& IPAddress::reset() {
		_pIPAddress = Wildcard(_pIPAddress->family())._pIPAddress;
	return *this;
	}

	IPAddress& IPAddress::set(const sockaddr& addr) {
		_pIPAddress.reset(IsIPv4Sock(addr) ? (IPImpl*)new IPv4Impl(addr) : (IPImpl*)new IPv6Impl(addr));
		return *this;
	}

	IPAddress& IPAddress::set(const IPAddress& other) {
		if (_pIPAddress->portSet())
			return set(other, _pIPAddress->port());
		_pIPAddress = other._pIPAddress;
		return *this;
	}

	IPAddress& IPAddress::set(const IPAddress& other, UInt16 port) {
		_pIPAddress = other._pIPAddress;
		setPort(port);
		return *this;
	}

	IPAddress& IPAddress::set(const in_addr& addr) {
		if (_pIPAddress->portSet())
			return set(addr, _pIPAddress->port());
		_pIPAddress.reset(new IPv4Impl(addr));
		return *this;
	}
	IPAddress& IPAddress::set(const in_addr& addr, UInt16 port) {
		_pIPAddress.reset(new IPv4Impl(addr, port));
		return *this;
	}

	IPAddress& IPAddress::set(const in6_addr& addr, UInt32 scope) {
		if (_pIPAddress->portSet())
			return set(addr, scope, _pIPAddress->port());
		_pIPAddress.reset(new IPv6Impl(addr, scope));
		return *this;
	}
	IPAddress& IPAddress::set(const in6_addr& addr, UInt32 scope, UInt16 port) {
		_pIPAddress.reset(new IPv6Impl(addr, scope, port));
		return *this;
	}

	bool IPAddress::set(Exception& ex, const char* address) {
		if (_pIPAddress->portSet())
			return set(ex, address, _pIPAddress->port());
		if (IPv4Impl::Parse(address, _pIPAddress) || IPv6Impl::Parse(address, _pIPAddress))
			return true;
		ex.set<Ex::Net::Address::Ip>("Invalid ip ", address);
		return false;
	}
	bool IPAddress::set(Exception& ex, const char* address, UInt16 port) {
		if (!IPv4Impl::Parse(address, _pIPAddress) && !IPv6Impl::Parse(address, _pIPAddress)) {
			ex.set<Ex::Net::Address::Ip>("Invalid IP ", address);
			return false;
		}
		_pIPAddress->setPort(port);
		return true;
	}

	bool IPAddress::set(Exception& ex, const char* address, Family family) {
		if (_pIPAddress->portSet())
			return set(ex, address, family, _pIPAddress->port());
		if (family == IPv6) {
			if (IPv6Impl::Parse(address, _pIPAddress))
				return true;
	} else if(IPv4Impl::Parse(address, _pIPAddress))
			return true;
		ex.set<Ex::Net::Address::Ip>("Invalid", family == IPv6 ? " IPv6 " : " IPv4 ", address);
		return false;
	}
	bool IPAddress::set(Exception& ex, const char* address, Family family, UInt16 port) {
		if (family == IPv6) {
			if (!IPv6Impl::Parse(address, _pIPAddress)) {
				ex.set<Ex::Net::Address::Ip>("Invalid IPv6 ", address);
				return false;
			}
	} else if (!IPv4Impl::Parse(address, _pIPAddress)) {
			ex.set<Ex::Net::Address::Ip>("Invalid IPv4 ", address);
			return false;
		}
		_pIPAddress->setPort(port);
		return true;
	}

	bool IPAddress::Resolve(Exception& ex, const char* address, IPAddress& host) {
		HostEntry entry;
		if (!DNS::HostByName(ex, address, entry))
			return false;
		auto& addresses(entry.addresses());
		if (addresses.empty()) {
			ex.set<Ex::Net::Address::Ip>("No ip found for address ", address);
			return false;
		}
		// if we get both IPv4 and IPv6 addresses, prefer IPv4
		for (const IPAddress& address : addresses) {
			if (address.family() == IPAddress::IPv4) {
				host = address;
				return true;
			}
		}
		host.set(*addresses.begin());
		return true;
	}

	bool IPAddress::mask(Exception& ex, const IPAddress& mask, const IPAddress& set) {
		if (family() != IPAddress::IPv4 || mask.family() != IPAddress::IPv4 || set.family() != IPAddress::IPv4) {
			ex.set<Ex::Net::Address::Ip>("IPAddress mask operation is available just between IPv4 addresses (address=", *this, ", mask=", mask, ", set=", set, ")");
			return false;
		}
		if (_pIPAddress->portSet())
			_pIPAddress.reset(new IPv4Impl(_pIPAddress->ipv4(), mask._pIPAddress->ipv4(), set._pIPAddress->ipv4(), _pIPAddress->port()));
		else
			_pIPAddress.reset(new IPv4Impl(_pIPAddress->ipv4(), mask._pIPAddress->ipv4(), set._pIPAddress->ipv4()));
		return true;
	}

	IPAddress::Family IPAddress::family() const {
		return _pIPAddress->family();
	}
	UInt16 IPAddress::port() const {
		return _pIPAddress->port();
	}
	const sockaddr_in6& IPAddress::addr() const {
		return _pIPAddress->addr();
	}
	const string& IPAddress::addrToString() const {
		return _pIPAddress->address();
	}
	UInt32 IPAddress::scope() const {
		return _pIPAddress->scope();
	}
	const std::string& IPAddress::toString() const {
		return _pIPAddress->host();
	}
	bool IPAddress::isWildcard() const {
		return _pIPAddress->isWildcard();
	}
	bool IPAddress::isBroadcast() const {
		return _pIPAddress->isBroadcast();
	}
	bool IPAddress::isAnyBroadcast() const {
		return _pIPAddress->isAnyBroadcast();
	}
	bool IPAddress::isLoopback() const {
		return _pIPAddress->isLoopback();
	}
	bool IPAddress::isMulticast() const {
		return _pIPAddress->isMulticast();
	}
	bool IPAddress::isLinkLocal() const {
		return _pIPAddress->isLinkLocal();
	}
	bool IPAddress::isSiteLocal() const {
		return _pIPAddress->isSiteLocal();
	}
	bool IPAddress::isIPv4Compatible() const {
		return _pIPAddress->isIPv4Compatible();
	}
	bool IPAddress::isIPv4Mapped() const {
		return _pIPAddress->isIPv4Mapped();
	}
	bool IPAddress::isWellKnownMC() const {
		return _pIPAddress->isWellKnownMC();
	}
	bool IPAddress::isNodeLocalMC() const {
		return _pIPAddress->isNodeLocalMC();
	}
	bool IPAddress::isLinkLocalMC() const {
		return _pIPAddress->isLinkLocalMC();
	}
	bool IPAddress::isSiteLocalMC() const {
		return _pIPAddress->isSiteLocalMC();
	}
	bool IPAddress::isOrgLocalMC() const {
		return _pIPAddress->isOrgLocalMC();
	}
	bool IPAddress::isGlobalMC() const {
		return _pIPAddress->isGlobalMC();
	}
	UInt8 IPAddress::prefixLength() const {
		return _pIPAddress->prefixLength();
	}

	bool IPAddress::operator == (const IPAddress& a) const {
		if (_pIPAddress->size() != a._pIPAddress->size())
			return false;
		if (_pIPAddress->scope() != a._pIPAddress->scope())
			return false;
		return memcmp(_pIPAddress->data(), a._pIPAddress->data(), _pIPAddress->size()) == 0;
	}

	bool IPAddress::operator < (const IPAddress& a) const {
		if (_pIPAddress->size() != a._pIPAddress->size())
			return _pIPAddress->size() < a._pIPAddress->size();
		if (_pIPAddress->scope() != a._pIPAddress->scope())
			return _pIPAddress->scope() < a._pIPAddress->scope();
		return memcmp(_pIPAddress->data(), a._pIPAddress->data(), _pIPAddress->size()) < 0;
	}

	const void* IPAddress::data() const {
		return _pIPAddress->data();
	}

	UInt8 IPAddress::size() const {
		return _pIPAddress->size();
	}

	const IPAddress& IPAddress::Broadcast() {
		static IPBroadcaster IPBroadcast;
		return IPBroadcast;
	}

	const IPAddress& IPAddress::Wildcard(Family family) {
		if (family == IPv6) {
			static IPAddress IPv6Wildcard(new IPv6Impl());
			return IPv6Wildcard;
		}
		static IPAddress IPv4Wildcard(new IPv4Impl());
		return IPv4Wildcard;
	}

	const IPAddress& IPAddress::Loopback(Family family) {
		if (family == IPv6) {
			static IPLoopback IPv6Loopback(IPAddress::IPv6);
			return IPv6Loopback;
		}
		static IPLoopback IPv4Loopback(IPAddress::IPv4);
		return IPv4Loopback;
	}


} // namespace Mona
