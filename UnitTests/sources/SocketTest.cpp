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

#include "Test.h"
#include "Mona/TCPClient.h"
#include "Mona/TCPServer.h"
#include "Mona/UDPSocket.h"
#include "Mona/TLS.h"
#include "Mona/Util.h"
#include <set>

using namespace std;
using namespace Mona;

namespace SocketTest {

struct MainHandler : Handler {
	MainHandler() : Handler(_signal) {}
	bool join(const function<bool()>& joined) {
		while(!joined()) {
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

ADD_TEST(UDP_Blocking) {

	Socket server(Socket::TYPE_DATAGRAM);

	SocketAddress address;
	Exception ex;
	CHECK(server.bind(ex, address) && !ex && server.address() && !server.address().host());

	Socket client(Socket::TYPE_DATAGRAM);
	address.set(IPAddress::Loopback(), server.address().port());
	CHECK(client.connect(ex,address) && !ex && client.peerAddress()==address && client.address())
	CHECK(client.send(ex, EXPAND("hi mathieu and thomas"))==21 && !ex);
	CHECK(UInt32(client.send(ex, _Short0Data.c_str(),_Short0Data.size()))==_Short0Data.size() && !ex);

	UInt8 buffer[8192];
	SocketAddress from;
	CHECK(server.receiveFrom(ex, buffer, sizeof(buffer), from) == 21 && !ex && from==client.address() && memcmp(buffer, EXPAND("hi mathieu and thomas")) == 0);
	CHECK(server.sendTo(ex,buffer,21, from)==21 && !ex)
	CHECK(UInt32(server.receiveFrom(ex, buffer, sizeof(buffer), from))==_Short0Data.size() && !ex && from==client.address() && memcmp(buffer,_Short0Data.data(),_Short0Data.size())==0)
	CHECK(UInt32(server.sendTo(ex,buffer,_Short0Data.size(), from))==_Short0Data.size() && !ex)

	CHECK(client.receive(ex, buffer, sizeof(buffer)) == 21 && !ex&& memcmp(buffer, EXPAND("hi mathieu and thomas")) == 0);
	CHECK(UInt32(client.receive(ex, buffer, sizeof(buffer)))==_Short0Data.size() && !ex && memcmp(buffer,_Short0Data.data(),_Short0Data.size())==0)

	CHECK(client.connect(ex, SocketAddress::Wildcard()) && !ex && !client.peerAddress() && client.address())
}


struct Connection : Thread {
	Connection() : Thread("Connection") {}
	Socket*	operator->() { if(!_pSocket) stop();  return _pSocket.get(); }
	Socket&	operator*() { if (!_pSocket) stop(); return *_pSocket; }

	bool start(Exception& ex, Socket& server) { _pSocket.reset();  _pServer = &server; return Thread::start(ex); }
private:
	bool run(Exception& ex, const volatile bool& stopping) { return _pServer->accept(ex, _pSocket) && _pSocket && !ex; }

	Socket*			_pServer;
	shared<Socket>	_pSocket;
};


void TestTCPBlocking(const shared<TLS>& pClientTLS = nullptr, const shared<TLS>& pServerTLS = nullptr) {
	Exception ex;

	unique_ptr<Socket> pServer(new TLS::Socket(Socket::TYPE_STREAM, pServerTLS));
	unique_ptr<Socket> pClient(new TLS::Socket(Socket::TYPE_STREAM, pClientTLS));
	Connection connection;
	
	// Test a connection refused
	SocketAddress unknown(IPAddress::Loopback(), 62434);
	CHECK(!pClient->connect(ex, unknown) && ex && !pClient->address() && !pClient->peerAddress());
	ex = NULL;

	// Test a IPv4 server
	SocketAddress address;
	CHECK(pServer->bind(ex, address) && !ex && pServer->address() && !pServer->address().host());
	CHECK(pServer->listen(ex) && !ex);
	UInt16 port = pServer->address().port();
	
	// Test IPv6 client refused by IPv4 server
	address.set(IPAddress::Loopback(IPAddress::IPv6), port);
	CHECK(!pClient->connect(ex, address, 1) && ex && !pClient->address() && !pClient->peerAddress());
	ex = NULL;
	pClient.reset(new TLS::Socket(Socket::TYPE_STREAM, pClientTLS));

	// Test IPv4 client accepted by IPv4 server
	address.set(IPAddress::Loopback(), port);
	CHECK(connection.start(ex, *pServer) && !ex);
	CHECK(pClient->connect(ex, address) && !ex && pClient->address() && pClient->peerAddress() == address);
	CHECK(connection->peerAddress() == pClient->address() && pClient->peerAddress() == connection->address());
	pClient.reset(new TLS::Socket(Socket::TYPE_STREAM, pClientTLS));
	pServer.reset(new TLS::Socket(Socket::TYPE_STREAM, pServerTLS));
	
	// Test a IPv6 server (and test bind overriding)
	address.set(IPAddress::Wildcard(IPAddress::IPv6), port);
	CHECK(pServer->bind(ex, address) && !ex && pServer->address() == address);
	CHECK(pServer->listen(ex) && !ex);

	// Test IPv4 client accepted by IPv6 server
	address.set(IPAddress::Loopback(), port);
	CHECK(connection.start(ex, *pServer) && !ex);
	CHECK(pClient->connect(ex, address) && !ex && pClient->address() && pClient->peerAddress() == address);
	CHECK(connection->peerAddress() == pClient->address() && pClient->peerAddress() == connection->address());
	pClient.reset(new TLS::Socket(Socket::TYPE_STREAM, pClientTLS));

	// Test IPv6 client accepted by IPv6 server
	address.set(IPAddress::Loopback(IPAddress::IPv6), port);
	CHECK(connection.start(ex, *pServer) && !ex);
	CHECK(pClient->connect(ex, address) && !ex && pClient->address() && pClient->peerAddress() == address);
	CHECK(connection->peerAddress() == pClient->address() && pClient->peerAddress() == connection->address());

	// Test server TCP communication
	CHECK(pClient->send(ex, EXPAND("hi mathieu and thomas")) == 21 && !ex);
	CHECK(UInt32(pClient->send(ex, _Long0Data.c_str(), _Long0Data.size())) == _Long0Data.size() && !ex);
	CHECK(pClient->shutdown(Socket::SHUTDOWN_SEND));

	UInt8 buffer[8192];
	int received(0);
	Buffer message;
	while ((received = connection->receive(ex, buffer, sizeof(buffer))) > 0) {
		CHECK(connection->send(ex, buffer, received) == received && !ex);
		CHECK((received = pClient->receive(ex, buffer, sizeof(buffer))) > 0 && !ex);
		message.append(buffer, received);
	}
	CHECK(connection->shutdown(Socket::SHUTDOWN_SEND));

	CHECK(!pClient->receive(ex, buffer, sizeof(buffer)) && !ex)

	CHECK(!ex && message.size() == (_Long0Data.size() + 21) && memcmp(message.data(), EXPAND("hi mathieu and thomas")) == 0 && memcmp(message.data() + 21, _Long0Data.data(), _Long0Data.size()) == 0)
}


ADD_TEST(TCP_Blocking) {
	TestTCPBlocking();
}

ADD_TEST(TCP_SSL_Blocking) {
	Exception ex;
	shared<TLS> pClientTLS, pServerTLS;
	CHECK(TLS::Create(ex, pClientTLS) && !ex);
	CHECK(TLS::Create(ex, "cert.pem", "key.pem", pServerTLS) && !ex);
	TestTCPBlocking(pClientTLS, pServerTLS);
}


struct UDPEchoClient :  UDPSocket {
	UDPEchoClient(IOSocket& io) : UDPSocket(io) {
		onError = [this](const Exception& ex) { FATAL_ERROR("UDPEchoClient, ", ex); };
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

	void send(const void* data,UInt32 size) {
		_packets.emplace_back(Packet(data, size));
		Exception ex;
		CHECK(UDPSocket::send(ex, _packets.back()) && !ex);
	}

private:
	deque<Packet>  _packets;
};


ADD_TEST(UDP_NonBlocking) {
	MainHandler	handler;
	IOSocket	io(handler,_ThreadPool);
	Exception ex;

	UDPSocket    server(io);
	server.onError = [&](const Exception& ex) { FATAL_ERROR("UDPEchoServer, ",ex); };
	server.onPacket = [&server,&ex](shared<Buffer>& pBuffer,const SocketAddress& address) {
		CHECK(server.send(ex, Packet(pBuffer), address) && !ex)
	};

	SocketAddress	 address;
	CHECK(!server.bound() && server.bind(ex, address) && server.bind(ex, address) && !ex && server.bound());

	UDPEchoClient    client(io);
	SocketAddress	 target(IPAddress::Loopback(), server->address().port());
	CHECK(client.connect(ex, target) && client.connect(ex, target) && !ex && client.connected() && client->address() && client->peerAddress());
	client.send(EXPAND("hi mathieu and thomas"));
	client.send(_Short0Data.c_str(), _Short0Data.size());
	client.send(NULL, 0);
	CHECK(handler.join([&client]()->bool { return !client.echoing();}));

	client.disconnect();
	CHECK(!client.connected() && !client->peerAddress());

	server.close();
	CHECK(!server.bound() && !server);

	server.onError = nullptr;
	server.onPacket = nullptr;

	client.close();
	CHECK(!client)

	_ThreadPool.join();
	handler.flush();
	CHECK(!io.subscribers());
}

struct TCPEchoClient : TCPClient {
	TCPEchoClient(IOSocket& io, const shared<TLS>& pTLS) : TCPClient(io, pTLS) {

		onError = [this](const Exception& ex) { this->ex = ex; };;
		onDisconnection = [this](const SocketAddress& peerAddress){ CHECK(!connected()) };
		onFlush = [this](){ CHECK(connected()) };;
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

	bool echoing() {
		return !_packets.empty();
	}

	void echo(const void* data,UInt32 size) {
		_packets.emplace_back(Packet(data, size));
		CHECK(send(ex, _packets.back()) && !ex);
	}

private:
	deque<Packet>	_packets;
};

void TestTCPNonBlocking(const shared<TLS>& pClientTLS = nullptr, const shared<TLS>& pServerTLS = nullptr) {
	Exception ex;
	MainHandler	 handler;
	IOSocket io(handler, _ThreadPool);

	TCPServer   server(io, pServerTLS);
	set<TCPClient*> pConnections;

	TCPServer::OnError onError([](const Exception& ex) {
		FATAL_ERROR("TCPEchoServer, ", ex);
	});
	server.onError = onError;

	server.onConnection = [&](const shared<Socket>& pSocket) {
		CHECK(pSocket && pSocket->peerAddress());

		TCPClient* pConnection(new TCPClient(io, pClientTLS));

		pConnection->onError = onError;
		pConnection->onDisconnection = [&, pConnection](const SocketAddress& address) {
			CHECK(!pConnection->connected() && address && !*pConnection);
			pConnections.erase(pConnection);
			delete pConnection; // here no unsubscription required because event dies before the function
		};
		pConnection->onFlush = [pConnection]() { CHECK(pConnection->connected()) };
		pConnection->onData = [&, pConnection](Packet& buffer) {
			CHECK(pConnection->send(ex, buffer) && !ex); // echo
			return 0;
		};

		Exception ex;
		CHECK(pConnection->connect(ex, pSocket) && !ex);

		pConnections.emplace(pConnection);
	};


	SocketAddress address;
	CHECK(!server.running() && server.start(ex, address) && !ex && server.running());
	// Restart on the same address (to test start/stop/start server on same address binding)
	address = server->address();
	server.stop();
	CHECK(!server.running() && server.start(ex, address) && !ex  && server.running());

	TCPEchoClient client(io, pClientTLS);
	SocketAddress target(IPAddress::Loopback(), address.port());
	CHECK(client.connect(ex, target) && !ex && client->peerAddress() == target);
	client.echo(EXPAND("hi mathieu and thomas"));
	client.echo(_Long0Data.c_str(), _Long0Data.size());
	CHECK(handler.join([&]()->bool { return client.connected() && !client.echoing(); } ));

	CHECK(pConnections.size() == 1 && (*pConnections.begin())->connected() && (**pConnections.begin())->peerAddress() == client->address() && client->peerAddress() == (**pConnections.begin())->address())

	client.disconnect();
	CHECK(!client.connected() && !client);

	CHECK(!client.ex);
	
	// Test a refused connection
	SocketAddress unknown(IPAddress::Loopback(), 62434);
	if (client.connect(ex, unknown)) {
		CHECK(!ex && !client.connected() && client.connecting() && client->address() && client->peerAddress() == unknown);
		if (Util::Random()) // Random to check that with or without sending, after SocketEngine join we get a client.connected()==false!
			CHECK((client.send(ex, Packet(EXPAND("salut"))) && !ex) || ex);
		CHECK(handler.join([&client]()->bool { return !client.connecting(); }));
		CHECK((ex.cast<Ex::Net::Socket>().code == NET_ECONNREFUSED && !client.ex) || client.ex.cast<Ex::Net::Socket>().code == NET_ECONNREFUSED); // if ex it has been set by random client.send
	}
	CHECK(!client.connected() && !client.connecting());
	
	server.stop();
	CHECK(!server.running() && !server);
	CHECK(handler.join([&pConnections]()->bool { return pConnections.empty(); }));

	server.onConnection = nullptr;
	server.onError = nullptr;
	
	_ThreadPool.join();
	handler.flush();
	CHECK(!io.subscribers());
}


ADD_TEST(TCP_NonBlocking) {
	TestTCPNonBlocking();
}

ADD_TEST(TCP_SSL_NonBlocking) {
	Exception ex;
	shared<TLS> pClientTLS, pServerTLS;
	CHECK(TLS::Create(ex, pClientTLS) && !ex);
	CHECK(TLS::Create(ex, "cert.pem", "key.pem", pServerTLS) && !ex);
	TestTCPNonBlocking(pClientTLS, pServerTLS);
}

}
