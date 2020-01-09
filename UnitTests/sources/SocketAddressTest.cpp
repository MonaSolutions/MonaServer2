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

#include "Mona/UnitTest.h"
#include "Mona/SocketAddress.h"
#include "Mona/Logs.h"
#include "Mona/Util.h"

using namespace std;
using namespace Mona;

namespace SocketAddressTest {

static string _Buffer;

ADD_TEST(Behavior) {
	
	SocketAddress sa;
	Exception ex;
	

	CHECK(!sa);
	CHECK(sa.host().isWildcard());
	CHECK(sa.port() == 0);

	CHECK(sa.set(ex,"192.168.1.100", 100) && !ex);
	CHECK(sa.host() == "192.168.1.100");
	CHECK(sa.port() == 100);

	CHECK(sa.set(ex, "192.168.1.100", "100") && !ex);
	CHECK(sa.host() == "192.168.1.100");
	CHECK(sa.port() == 100);


	CHECK(sa.set(ex, "192.168.1.100", Net::ResolvePort(ex, "ftp")) && !ex)
	CHECK(sa.host() == "192.168.1.100");
	CHECK(sa.port() == 21);
	CHECK(!Net::ResolvePort(ex, "f00bar") && ex);
	ex = NULL;


	DEBUG_CHECK(sa.setWithDNS(ex,"localhost", 80) && !ex);
	DEBUG_CHECK(sa.host() == "127.0.0.1");
	DEBUG_CHECK(sa.port() == 80);

	CHECK(!sa.set(ex, "192.168.2.260", 80) && ex);
	ex = NULL;

	CHECK(!sa.set(ex, "192.168.2.120", "80000") && ex);
	ex = NULL;

	CHECK(sa.set(ex,_Buffer.assign("192.168.2.120:88")) && !ex);
	CHECK(sa.host() == "192.168.2.120");
	CHECK(sa.port() == 88);

	CHECK(!sa.set(ex, _Buffer.assign("[192.168.2.120]:88")) && ex);
	ex = NULL;
	
	CHECK(!sa.set(ex, "[192.168.2.260]") && ex);
	ex = NULL;

	CHECK(!sa.set(ex, _Buffer.assign("[192.168.2.260:88")) && ex);
	ex = NULL;
	
	CHECK(sa.set(ex, "192.168.1.100", 100) && !ex);
	SocketAddress sa2;
	CHECK(sa2.set(ex, _Buffer.assign("192.168.1.100:100")) && !ex);
	CHECK(sa == sa2);

	CHECK(sa.set(ex, "192.168.1.101", "99") && !ex);
	CHECK(sa2 < sa);

	CHECK(sa2.set(ex, "192.168.1.101", "102") && !ex);
	CHECK(sa < sa2);
}

ADD_TEST(ToString) {
	// toString performance (for loop test)
	SocketAddress sa;
	CHECK(sa.length());
}

ADD_TEST(ParsePerformance) {
	// Parse performance (for loop test)
	SocketAddress sa;
	Exception ex;
	CHECK(sa.set(ex,"192.168.1.100",100) && !ex)
}

ADD_TEST(ComparaisonPerformance) {
	// Comparaison performance (for loop test)
	SocketAddress sa;
	SocketAddress sa2(sa);
	if (sa < sa2)
		CHECK(false)
}

ADD_TEST(Behavior6) {
	
	SocketAddress sa;
	Exception ex;

	CHECK(!sa);
	CHECK(sa.host().isWildcard());
	CHECK(sa.port() == 0);

	CHECK(sa.set(ex,"1080::8:600:200A:425C", 100) && !ex);
	CHECK(sa.host() == "1080::8:600:200a:425c");
	CHECK(sa.port() == 100);

	CHECK(sa.set(ex, "1080::8:600:200A:425C", "100") && !ex);
	CHECK(sa.host() == "1080::8:600:200a:425c");
	CHECK(sa.port() == 100);


	CHECK(sa.set(ex, "1080::8:600:200A:425C", Net::ResolvePort(ex, "ftp")) && !ex);
	CHECK(sa.host() == "1080::8:600:200a:425c");
	CHECK(sa.port() == 21);

	CHECK(sa.set(ex, "1080::0001", "65535") && !ex);
	CHECK(sa.host() == "1080::1");
	CHECK(sa.port() == 65535);

	CHECK(!sa.set(ex, "1080::8:600:200A:FFFFF", 80) && ex);
	ex = NULL;
	
	CHECK(!sa.set(ex, "1080::8:600:200A:425C", "80000") && ex);
	ex = NULL;

	CHECK(sa.set(ex,_Buffer.assign("[1080::8:600:200A:425C]:88")) && !ex);
	CHECK(sa.host() == "1080::8:600:200a:425c");
	CHECK(sa.port() == 88);

	CHECK(!sa.set(ex, _Buffer.assign("[1080::8:600:200A:425C]")) && ex);
	ex = NULL;

	CHECK(!sa.set(ex,_Buffer.assign("[1080::8:600:200A:425C:88]")) && ex);
	ex = NULL;
	
	CHECK(sa.set(ex,"1080::8:600:200A:425C", 100) && !ex);
	SocketAddress sa2;
	CHECK(sa2.set(ex,_Buffer.assign("[1080::8:600:200A:425C]:100")) && !ex);
	CHECK(sa == sa2);

	CHECK(sa.set(ex,"1080::8:600:200A:425D", "99") && !ex);
	CHECK(sa2 < sa);

	CHECK(sa2.set(ex, "1080::8:600:200A:425D", "102") && !ex);
	CHECK(sa < sa2);

	CHECK(sa.set(ex, "1080::0001", "100") && !ex);
	CHECK(sa2.set(ex, "1080::1", "100") && !ex);
	CHECK(sa == sa2);
}

ADD_TEST(Comparisons6) {
	string address, addressAndPort;
	Exception ex;
	SocketAddress sAddress1, sAddress2;

	// Test of 0:0:0:0:0:0:0:X formats
	String::Assign(address, "0:0:0:0:0:0:0:", String::Format<UInt32>("%.X", Util::Random<UInt16>()));
	String::Assign(addressAndPort, "[", address, "]:1234");

	CHECK(sAddress1.set(ex, addressAndPort) && !ex);
	CHECK(sAddress2.set(ex, address, 1234) && !ex);
	CHECK(sAddress1==sAddress2);
}

}
