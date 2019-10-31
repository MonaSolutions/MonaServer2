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
#include "Mona/TCPClient.h"
#include "Mona/TCPServer.h"
#include "Mona/UDPSocket.h"
#include "Mona/Proxy.h"
#include "Mona/Util.h"
#include <set>

using namespace std;
using namespace Mona;

namespace ProxyTest {

struct MainHandler : Handler {
	MainHandler() : Handler(_signal) {}
	bool join(const function<bool()>& joined) {
		while (!joined()) {
			if (!_signal.wait(14000))
				return false;
			flush();
		};
		return true;
	}

private:
	Signal _signal;
};



static string		_Short0Data(1024, '\0');
static string		_Long0Data(0xFFFF, '\0');
static ThreadPool	_ThreadPool;


struct UDPEchoClient : UDPSocket {
	UDPEchoClient(IOSocket& io) : UDPSocket(io) {
		onError = [](const Exception& ex) { FATAL_ERROR("UDPEchoClient, ", ex); };
		onPacket = [this](shared<Buffer>& pBuffer, const SocketAddress& address) {
			CHECK(address == self->peerAddress());
			CHECK(_packets.front().size() == pBuffer->size() && memcmp(_packets.front().data(), pBuffer->data(), pBuffer->size()) == 0);
			_packets.pop_front();
		};
	}

	~UDPEchoClient() {
		onPacket = nullptr;
		onError = nullptr;
	}

	bool echoing() { return !_packets.empty(); }

	void send(const void* data, UInt32 size) {
		_packets.emplace_back(Packet(data, size));
		Exception ex;
		CHECK(UDPSocket::send(ex, _packets.back()) && !ex);
	}

private:
	deque<Packet>  _packets;
};

struct UDPProxy : UDPSocket {
	UDPProxy(IOSocket& io, const SocketAddress& address) : UDPSocket(io), _proxy(io), _address(address) {
		onError = [](const Exception& ex) { FATAL_ERROR("UDPProxy, ", ex); };
		onPacket = [this](shared<Buffer>& pBuffer, const SocketAddress& address) {
			Exception ex;
			CHECK(_proxy.relay(ex, socket(), Packet(pBuffer), _address, address) && !ex);
		};
		_proxy.onError = [](const Exception& ex) { FATAL_ERROR("UDPProxy, ", ex); };
	}

	~UDPProxy() {
		_proxy.onError = nullptr;
		onPacket = nullptr;
		onError = nullptr;
	}

	void close() { _proxy.close(); UDPSocket::close(); }

private:
	Proxy			_proxy;
	SocketAddress	_address;
};


ADD_TEST(UDP_Proxy) {
	MainHandler	handler;
	IOSocket	io(handler, _ThreadPool);
	Exception ex;

	UDPSocket		server(io);
	server.onError = [&](const Exception& ex) { FATAL_ERROR("UDPEchoServer, ", ex); };
	server.onPacket = [&server, &ex](shared<Buffer>& pBuffer, const SocketAddress& address) {
		CHECK(server.send(ex, Packet(pBuffer), address) && !ex);
	};
	SocketAddress	 address;
	CHECK(server.bind(ex, address) && !ex);

	UDPProxy proxy(io, SocketAddress(IPAddress::Loopback(), server->address().port()));
	CHECK(proxy.bind(ex, address) && !ex);

	UDPEchoClient   client(io);
	CHECK(client.connect(ex, SocketAddress(IPAddress::Loopback(), proxy->address().port())) && !ex);
	client.send(EXPAND("hi mathieu and thomas"));
	client.send(_Short0Data.c_str(), _Short0Data.size());
	CHECK(handler.join([&client]()->bool { return !client.echoing(); }));

	client.close();
	server.close();
	proxy.close();

	server.onError = nullptr;
	server.onPacket = nullptr;

	_ThreadPool.join();
	handler.flush();
	CHECK(!io.subscribers());
}


struct TCPEchoClient : TCPClient {
	TCPEchoClient(IOSocket& io) : TCPClient(io) {
		onError = [this](const Exception& ex) { this->ex = ex; };;
		onDisconnection = [this](const SocketAddress& peerAddress) { CHECK(!connected()) };
		onFlush = [this]() { CHECK(connected()) };;
		onData = [this](Packet& buffer)->UInt32 {
			do {
				CHECK(!_packets.empty());
				UInt32 size(_packets.front().size());
				if (size > buffer.size())
					return buffer.size(); // wait next
				CHECK(memcmp(_packets.front().data(), buffer.data(), size) == 0);
				_packets.pop_front();
				buffer += size;
			} while (buffer);
			return 0;
		};
	}

	~TCPEchoClient() {
		onData = nullptr;
		onFlush = nullptr;
		onDisconnection = nullptr;
		onError = nullptr;
	}

	Exception	ex;

	bool echoing() { return !_packets.empty(); }

	void echo(const void* data, UInt32 size) {
		_packets.emplace_back(Packet(data, size));
		CHECK(send(ex, _packets.back()) && !ex);
	}

private:
	deque<Packet>	_packets;
};

struct TCPProxy : TCPServer {
	TCPProxy(IOSocket& io, const SocketAddress& address) : TCPServer(io), _address(address) {
		onError = [](const Exception& ex) { FATAL_ERROR("TCPProxy, ", ex); };
		onConnection = [this](const shared<Socket>& pSocket) {
			Connection* pConnection(new Connection(this->io, _address));
			pConnection->onDisconnection = [&, pConnection](const SocketAddress& address) {
				_connections.erase(pConnection);
				delete pConnection; // here no unsubscription required because event dies before the function
			};
			Exception ex;
			CHECK(pConnection->connect(ex, pSocket) && !ex);
			_connections.emplace(pConnection);
		};
	}

	~TCPProxy() {
		onConnection = nullptr;
		onError = nullptr;
	}

private:
	struct Connection : TCPClient {
		Connection(IOSocket& io, const SocketAddress& address) : TCPClient(io), _proxy(io), _address(address) {
			onError = [](const Exception& ex) { FATAL_ERROR("TCPProxy::Connection, ", ex); };
			onData = [this](Packet& buffer) {
				Exception ex;
				CHECK(_proxy.relay(ex, socket(), buffer, _address) && !ex);
				return 0;
			};
			_proxy.onError = [](const Exception& ex) { FATAL_ERROR("TCPProxy::Relay, ", ex); };
		}

		~Connection() {
			_proxy.onError = nullptr;
			onData = nullptr;
			onError = nullptr;
		}

	private:
		Proxy			_proxy;
		SocketAddress	_address;
	};
	SocketAddress		_address;
	set<Connection*>  _connections;
};


ADD_TEST(TCP_Proxy) {
	Exception ex;
	MainHandler	 handler;
	IOSocket io(handler, _ThreadPool);

	TCPServer   server(io);
	set<TCPClient*> pConnections;

	TCPServer::OnError onError([](const Exception& ex) { FATAL_ERROR("TCPEchoServer, ", ex); });
	server.onError = onError;

	server.onConnection = [&](const shared<Socket>& pSocket) {
		TCPClient* pConnection(new TCPClient(io));
		pConnection->onError = onError;
		pConnection->onDisconnection = [&, pConnection](const SocketAddress& address) {
			pConnections.erase(pConnection);
			delete pConnection; // here no unsubscription required because event dies before the function
		};
		pConnection->onData = [&, pConnection](Packet& buffer) {
			CHECK(pConnection->send(ex, buffer) && !ex); // echo
			return 0;
		};
		Exception ex;
		CHECK(pConnection->connect(ex, pSocket) && !ex);
		pConnections.emplace(pConnection);
	};


	SocketAddress address;
	CHECK(server.start(ex, address) && !ex);

	TCPProxy proxy(io, SocketAddress(IPAddress::Loopback(), server->address().port()));
	CHECK(proxy.start(ex, address) && !ex);

	TCPEchoClient client(io);
	CHECK(client.connect(ex, SocketAddress(IPAddress::Loopback(), proxy->address().port())) && !ex);
	client.echo(EXPAND("hi mathieu and thomas"));
	client.echo(_Long0Data.c_str(), _Long0Data.size());
	CHECK(handler.join([&client]()->bool { return client.connected() && !client.echoing(); }));

	client.disconnect();
	server.stop();
	proxy.stop();
	CHECK(handler.join([&pConnections]()->bool { return pConnections.empty(); }));

	server.onConnection = nullptr;
	server.onError = nullptr;

	_ThreadPool.join();
	handler.flush();
	CHECK(!io.subscribers());
}

}