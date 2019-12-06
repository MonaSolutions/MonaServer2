/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Protocol.h"

namespace Mona {


struct Protocols : virtual Object {
	Protocols(Parameters& params) : _params(params) {}
	~Protocols() { stop(); }

	typedef std::map<std::string, Protocol*, String::IComparator>::const_iterator const_iterator;

	UInt32	 count() const { return _protocols.size(); }

	const_iterator begin() const { return _protocols.begin(); }
	const_iterator end() const { return _protocols.end(); }

	template<typename ProtocolType=Protocol>
	ProtocolType* find(const char* name) const {
		const auto& it = _protocols.find(name);
		return it == _protocols.end() ? NULL : (ProtocolType*)it->second;
	}

	void start(ServerAPI& api, Sessions& sessions) { load(api, sessions); }
	void stop() {
		// unload
		for (auto& it : _protocols)
			delete it.second;
		_protocols.clear();
	}

	void manage() { for (auto& it : _protocols) it.second->manage(); }

private:

	void load(ServerAPI& api, Sessions& sessions);

	/*!
	Load protocol */
	template<typename ProtocolType, bool enabled = true, typename ...Args >
	ProtocolType* load(const char* protocol, ServerAPI& api, Sessions& sessions, Args&&... args) {
		return loadIntern<ProtocolType>(protocol, enabled, api, sessions, std::forward<Args>(args)...);
	}

	/*!
	Load sub protocol */
	template<typename ProtocolType, bool enabled = true, typename ...Args >
	ProtocolType* load(const char* protocol, Protocol* pGateway, Args&&... args) {
		if(pGateway)
			return loadIntern<ProtocolType>(protocol, enabled, *pGateway, std::forward<Args>(args)...);
		_params.setBoolean(protocol, false);
		return NULL;
	}

	template<typename ProtocolType, typename ...Args >
	ProtocolType* loadIntern(const char* protocol, bool enabled, Args&&... args) {
		if (!_params.getBoolean(protocol, enabled))
			_params.setBoolean(protocol, enabled);
		ProtocolType* pProtocol(NULL);
		if (enabled) {
			if ((pProtocol = loadProtocol<ProtocolType>(protocol, std::forward<Args>(args)...)))
				NOTE(protocol, " server started on ", pProtocol->address);
		}
		// search other protocol declaration, ex: [HTTP2=HTTP]
		for (auto& it : _params) {
			// [name=protocol]
			if (it.first.find('.') != std::string::npos || String::ICompare(it.second, protocol) != 0)
				continue;
			ProtocolType* pProtocol2 = loadProtocol<ProtocolType>(it.first.c_str(), std::forward<Args>(args)...);
			if (pProtocol2) {
				NOTE(it.first, '(', protocol, ") server started on ", pProtocol2->address);
				if (!pProtocol)
					pProtocol = pProtocol2;
			}
		}
		return pProtocol;
	}

	template<typename ProtocolType, typename ...Args >
	ProtocolType* loadProtocol(const char* name, Args&&... args) {
		// check name protocol is unique!
		const auto& it(_protocols.lower_bound(name));
		if (it != _protocols.end() && it->first == name) {
			ERROR(name, " protocol already exists, use a unique name for each protocol");
			return NULL;
		}

		ProtocolType* pProtocol = new ProtocolType(name, std::forward<Args>(args)...);
	
		std::string buffer;
		// Copy parameters from API to PROTOCOL (duplicated to get easy access with protocol object)
		// => override default protocol parameters
		for(auto& it : _params.range(String::Assign(buffer, name, ".")))
			pProtocol->setString(it.first.c_str() + buffer.size(), it.second);

		Exception ex;
		SocketAddress& address((SocketAddress&)pProtocol->address); // address is wildcard here!

		// Build address
		buffer.clear();
		pProtocol->getString("host", buffer);
		if (!buffer.empty()) {
			// Build host address
			bool success;
			AUTO_ERROR(success = address.host().setWithDNS(ex, buffer), "Impossible to resolve host ", buffer, " of ", name, " server")
			if (!success) {
				// 0.0.0.0 is a valid host!
				delete pProtocol;
				_params.setBoolean(name, false); // to warn on protocol absence
				return NULL; // Bad host parameter can't start!
			}
		}
		address.setPort(pProtocol->template getNumber<UInt16>("port"));

		// Load protocol and read its address
		
		AUTO_ERROR(address = pProtocol->load(ex), name, " server");
		if (!address) {
			delete pProtocol;
			_params.setBoolean(name, false); // to warn on protocol absence
			return NULL;
		}
		_protocols.emplace_hint(it, name, pProtocol);

		// Fix bind addres
		pProtocol->setString("host", address.host());
		pProtocol->setNumber("port", address.port());
		pProtocol->setString("address", address);


		// Build public address (protocol.address)
		SocketAddress& publicAddress((SocketAddress&)pProtocol->publicAddress);
		publicAddress.set(address);
		buffer.clear();
		pProtocol->getString("publicHost", buffer);
		if (!buffer.empty())
			AUTO_WARN(publicAddress.host().setWithDNS(ex, buffer), "Impossible to resolve publicHost ", buffer, " of ", name, " server");
		UInt16 port;
		if (pProtocol->getNumber("publicPort", port))
			publicAddress.setPort(port);
		pProtocol->setString("publicHost", publicAddress.host());
		pProtocol->setNumber("publicPort", publicAddress.port());
		pProtocol->setString("publicAddress", publicAddress);

		// Copy protocol params to api params! (to get defaults protocol params + configs params on api)
		for (auto& it : *pProtocol)
			_params.setString(String::Assign(buffer, name, ".", it.first), it.second);

		return pProtocol;
	}


	Parameters&												_params;
	std::map<std::string, Protocol*, String::IComparator>	_protocols;
};


} // namespace Mona
