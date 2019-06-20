/*
This code is in part based on code from the POCO C++ Libraries,
licensed under the Boost software license :
https://www.boost.org/LICENSE_1_0.txt

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
#include "Mona/IPAddress.h"
#include <set>
#include <vector>


namespace Mona {

	/// This class stores information about a host
	/// such as host name, alias names and a list
	/// of IP addresses.
class HostEntry : public virtual Object {
public:

	// Creates an empty HostEntry.
	HostEntry() {}

	// Creates the HostEntry from the data in a hostent structure.
	void set(Exception& ex, const hostent& entry);

	// Creates the HostEntry from the data in an addrinfo structure.
	void set(Exception& ex, const addrinfo* ainfo);


	// Returns the canonical host name.
	const std::string& name() const {return _name;}
	// Returns a vector containing alias names for the host name
	const std::vector<std::string>& aliases() const {return _aliases;}
	// Returns a vector containing the IPAddresses for the host
	const std::set<IPAddress>&	addresses() const { return _addresses;}

private:
	std::string					_name;
	std::vector<std::string>	_aliases;
	std::set<IPAddress>			_addresses;
};


} // namespace Mona
