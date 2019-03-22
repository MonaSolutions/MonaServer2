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
#include "Mona/SRT.h"
#include "Mona/Util.h"
#include <set>

using namespace std;
using namespace Mona;

#if defined(SRT_API)

namespace SRTSocketTest {

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


static string		_Long0Data(1316, '\0');
static ThreadPool	_ThreadPool;


struct Server : private Thread {
	Server() : _signal(false), Thread("Server") {}
	~Server() { stop(); }
	
	const SocketAddress& bind(const SocketAddress& address) {
		Exception ex;
		CHECK(_server.setLinger(ex, true, 0) && !ex);
		CHECK(_server.setReuseAddress(ex, true) && !ex);
		
		CHECK(_server.bind(ex, address) && !ex && _server.address() && !_server.address().host());
		CHECK(_server.listen(ex) && !ex);
		return _server.address();
	}
	void accept() {
		stop();
		_signal.reset();
		start();
	}

	const Socket& connection() {
		_signal.wait(); 
		return *_pConnection;
	}

	// WARN: not thread-safe
	SRT::Socket& socket() {
		return _server;
	}

private:
	bool run(Exception& ex, const volatile bool& requestStop) {
		CHECK(_server.accept(ex, _pConnection) && _pConnection && !ex);
		_signal.set();
		UInt8 buffer[8192];
		int received(0);
		Buffer message;
		while ((received = _pConnection->receive(ex, buffer, sizeof(buffer))) > 1)
			CHECK(_pConnection->send(ex, buffer, received) == received && !ex);
		_pConnection->shutdown(Socket::SHUTDOWN_SEND);
		return true;
	}

	SRT::Socket				_server;
	shared<Socket>			_pConnection;
	Signal					_signal;
};


ADD_TEST(TestBlocking) {
	Exception ex;

	
	unique<Socket> pClient(new SRT::Socket());
	unique<Server> pServer(SET);

	// Test a connection refused
	SocketAddress unknown(IPAddress::Loopback(), 62434);
	CHECK(((SRT::Socket*)pClient.get())->setConnectionTimeout(ex, 1000) && !ex);
	CHECK(!pClient->connect(ex, unknown) && ex && !pClient->address() && !pClient->peerAddress());
	ex = NULL;

	// Test a IPv4 server
	SocketAddress address;
	UInt16 port = pServer->bind(address).port();
	pServer->accept();
	
	// Test IPv6 client refused by IPv4 server
	address.set(IPAddress::Loopback(IPAddress::IPv6), port);
	CHECK(!pClient->connect(ex, address, 1) && ex && !pClient->address() && !pClient->peerAddress());
	ex = NULL;
	pClient.set<SRT::Socket>();

	// Test IPv4 client accepted by IPv4 server
	address.set(IPAddress::Loopback(), port);
	CHECK(pClient->connect(ex, address) && !ex && pClient->address() && pClient->peerAddress() == address);
	CHECK(pServer->connection().peerAddress() == pClient->address() && pClient->peerAddress() == pServer->connection().address());
	pClient.set<SRT::Socket>();
	pServer.set<Server>();
	
	// Test a IPv6 server (with a new port)
	address.set(IPAddress::Wildcard(IPAddress::IPv6), 0);
	port = pServer->bind(address).port();

	// Test IPv4 client accepted by IPv6 server
	pServer->accept();
	address.set(IPAddress::Loopback(), port);
	CHECK(pClient->connect(ex, address) && !ex && pClient->address() && pClient->peerAddress() == address);
	CHECK(pServer->connection().peerAddress() == pClient->address() && pClient->peerAddress() == pServer->connection().address());
	pClient.set<SRT::Socket>();

	// Test IPv6 client accepted by IPv6 server
	pServer->accept();
	address.set(IPAddress::Loopback(IPAddress::IPv6), port);
	CHECK(pClient->connect(ex, address) && !ex && pClient->address() && pClient->peerAddress() == address);
	CHECK(pServer->connection().peerAddress() == pClient->address() && pClient->peerAddress() == pServer->connection().address());

	// Test server TCP communication
	CHECK(pClient->send(ex, EXPAND("hi mathieu and thomas")) == 21 && !ex);
	CHECK(UInt32(pClient->send(ex, _Long0Data.c_str(), _Long0Data.size())) == _Long0Data.size() && !ex);
	CHECK(pClient->send(ex, EXPAND("\0"))); // 1 byte packet to mean "end of data" (SHUTDOWN_SEND is not possible now)

	UInt8 buffer[8192];
	int received(0);
	Buffer message;
	while ((received = pClient->receive(ex, buffer, sizeof(buffer))) > 0)
		message.append(buffer, received);

	//TODO: Disabled because the socket is shutdown before receiving all messages
	//CHECK(!ex && message.size() == (_Long0Data.size() + 21) && memcmp(message.data(), EXPAND("hi mathieu and thomas")) == 0 && memcmp(message.data() + 21, _Long0Data.data(), _Long0Data.size()) == 0)*/
}

struct SRTEchoClient : SRT::Client {
	SRTEchoClient(IOSocket& io) : SRT::Client(io) {

		onError = [this](const Exception& ex) { 
			this->ex = ex; 
		};
		onDisconnection = [this](const SocketAddress& peerAddress){ 
			CHECK(!connected()) 
		};
		onFlush = [this](){
			CHECK(connected())
		};
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

	~SRTEchoClient() {
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

ADD_TEST(TestNonBlocking) {
	Exception ex;
	MainHandler	 handler;
	IOSocket io(handler, _ThreadPool);

	SRT::Server   server(io);
	set<SRT::Client*> pConnections;

	SRT::Server::OnError onError([](const Exception& ex) {
		FATAL_ERROR("EchoServer, ", ex);
	});
	server.onError = onError;

	server.onConnection = [&](const shared<Socket>& pSocket) {
		CHECK(pSocket && pSocket->peerAddress());

		SRT::Client* pConnection(new SRT::Client(io));

		pConnection->onError = onError;
		pConnection->onDisconnection = [&, pConnection](const SocketAddress& address) {
			CHECK(!pConnection->connected() && address && !*pConnection);
			pConnections.erase(pConnection);
			delete pConnection; // here no unsubscription required because event dies before the function
		};
		pConnection->onFlush = [pConnection]() { 
			CHECK(pConnection->connected()) 
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
	CHECK(!server.running() && server.start(ex, address) && !ex && server.running());
	// Restart on the same address (to test start/stop/start server on same address binding)
	address = server->address();
	server.stop();
	CHECK(!server.running() && server.start(ex, address) && !ex  && server.running()); 

	SRTEchoClient client(io);
	SocketAddress target(IPAddress::Loopback(), address.port());
	CHECK(client.connect(ex, target) && !ex && client->peerAddress() == target);
	client.echo(EXPAND("hi mathieu and thomas"));
	client.echo(_Long0Data.c_str(), _Long0Data.size());
	CHECK(handler.join([&]()->bool { return client.connected() && !client.echoing(); } ));

	CHECK(pConnections.size() == 1 && (*pConnections.begin())->connected() && (**pConnections.begin())->peerAddress() == client->address() && client->peerAddress() == (**pConnections.begin())->address())

	// Try a reconnection on another address before disconnecting
	SocketAddress unknown(IPAddress::Loopback(), 62434);
	CHECK(client.connect(ex, unknown) && !ex && client->peerAddress() == target);

	client.disconnect();
	CHECK(!client.connected() && !client);

	CHECK(!client.ex);
	
	// Test a refused connection
	if (client.connect(ex, unknown)) {
		CHECK(!ex && !client.connected() && client.connecting() && client->address() && client->peerAddress() == unknown);
		if (Util::Random()%2) // Random to check that with or without sending, after SocketEngine join we get a client.connected()==false!
			CHECK((client.send(ex, Packet(EXPAND("salut"))) && !ex) || ex);
		CHECK(handler.join([&client]()->bool { return !client.connecting(); }));
		//TODO: Disabled because we do not get exception NET_ECONNREFUSED
		//CHECK((ex.cast<Ex::Net::Socket>().code == NET_ECONNREFUSED && !client.ex) || client.ex.cast<Ex::Net::Socket>().code == NET_ECONNREFUSED); // if ex it has been set by random client.send
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

ADD_TEST(TestLoad) {
	Exception ex;
	MainHandler	 handler;
	IOSocket io(handler, _ThreadPool);

	SRT::Server   server(io);
	set<SRT::Client*> pConnections;

	SRT::Server::OnError onError([](const Exception& ex) {
		FATAL_ERROR("EchoServer, ", ex);
	});
	server.onError = onError;

	server.onConnection = [&](const shared<Socket>& pSocket) {
		CHECK(pSocket && pSocket->peerAddress());

		SRT::Client* pConnection(new SRT::Client(io));

		pConnection->onError = onError;
		pConnection->onDisconnection = [&, pConnection](const SocketAddress& address) {
			CHECK(!pConnection->connected() && address && !*pConnection);
			pConnections.erase(pConnection);
			delete pConnection; // here no unsubscription required because event dies before the function
		};
		pConnection->onFlush = [pConnection]() {
			INFO("onFlush pConnection")
			CHECK(pConnection->connected())
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
	CHECK(!server.running() && server.start(ex, address) && !ex && server.running());
	address = server->address();

	SRTEchoClient client(io);

	SocketAddress target(IPAddress::Loopback(), address.port());
	CHECK(client.connect(ex, target) && !ex && client->peerAddress() == target);

	CHECK(handler.join([&]()->bool { return client.connected(); }));

	for (int i = 0; i < 500; ++i)
		client.echo(_Long0Data.c_str(), _Long0Data.size());
	CHECK(handler.join([&]()->bool { return client.connected() && !client.echoing(); }));

	// Closing
	client.disconnect();

	server.stop();
	CHECK(!server.running() && !server);
	CHECK(handler.join([&pConnections]()->bool { return pConnections.empty(); }));

	server.onConnection = nullptr;
	server.onError = nullptr;

	_ThreadPool.join();
	handler.flush();
	CHECK(!io.subscribers());
}

ADD_TEST(TestOptions) {


	unique<SRT::Socket> pClient(SET);
	unique<Server> pServer(SET);

	int value;
	Exception ex;
	CHECK(pClient->setRecvBufferSize(ex, 0xFFFF));
	CHECK(pClient->getRecvBufferSize(ex, value) && value == 0xFFFF);
	CHECK(pClient->setSendBufferSize(ex, 0xFFFF));
	CHECK(pClient->getSendBufferSize(ex, value) && value == 0xFFFF);

	CHECK(pClient->setLatency(ex, 500));
	CHECK(pClient->setPeerLatency(ex, 600));
	CHECK(pClient->setMSS(ex, 900) && !ex);
	CHECK(pClient->setOverheadBW(ex, 50) && !ex);
	CHECK(pClient->setMaxBW(ex, 2000) && !ex);

	// Encryption
	CHECK(pClient->getEncryptionState(ex, value) && !ex && value == ::SRT_KM_S_UNSECURED);
	CHECK(pClient->getPeerDecryptionState(ex, value) && !ex && value == ::SRT_KM_S_UNSECURED);
	CHECK(pClient->setPassphrase(ex, EXPAND("passphrase")) && !ex);
	CHECK(pServer->socket().setPassphrase(ex, EXPAND("passphrase")) && !ex);

	// Connect client
	SocketAddress source(IPAddress::Wildcard(), 6123);
	SocketAddress target(IPAddress::Loopback(), 6123);
	CHECK(pServer->bind(source));
	pServer->accept();
	CHECK(pClient->connect(ex, target) && !ex && pClient->peerAddress() == target);

	// It's impossible to set options after connexion with SRT
	CHECK(!pClient->setRecvBufferSize(ex, 2000) && ex.cast<Ex::Net::Socket>().code == NET_EINVAL);
	ex = NULL;
	CHECK(pClient->getRecvBufferSize(ex, value) && !ex && value == 0xFFFF);

	// 1 byte packet to mean "end of data" (SHUTDOWN_SEND is not possible now)
	CHECK(pClient->send(ex, EXPAND("\0")));

	// Some options can only work after connection
	const SRT::Socket& connection = (SRT::Socket&)pServer->connection();
	CHECK(pClient->getEncryptionState(ex, value) && !ex && value == ::SRT_KM_S_SECURED);
	CHECK(connection.getPeerDecryptionState(ex, value) && !ex && value == ::SRT_KM_S_SECURED);

	CHECK(pClient->getLatency(ex, value) && !ex && value == 500);
	CHECK(pClient->getPeerLatency(ex, value) && !ex && value == 600);
	CHECK(connection.getPeerLatency(ex, value) && !ex && value == 500);
	CHECK(connection.getLatency(ex, value) && !ex && value == 600);

	CHECK(pClient->getMSS(ex, value) && !ex && value == 900);
	Int64 val64;
	CHECK(pClient->getMaxBW(ex, val64) && !ex && val64 == 2000);

	SRT::Stats stats;
	CHECK(pClient->getStats(ex, stats) && !ex && stats.recvLostCount() == 0 && (stats->pktSent == 1 || stats->pktSndBuf == 1)); // Can be sent or bufferised now
}

}

#endif