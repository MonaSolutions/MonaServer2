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
#include "Mona/UDPSocket.h"
#include "Mona/FileReader.h"
#include "Mona/Util.h"

using namespace Mona;
using namespace std;

static ThreadPool _ThreadPool;
static Handler	  _Handler;

struct Decoded : virtual Object, Packet {
	Decoded(const Packet& packet, const SocketAddress& address) : address(address), Packet(move(packet)) {}
	const SocketAddress address;
};

struct Decoder : virtual Object, Socket::Decoder {
	typedef Event<void(Decoded&)> ON(Decoded);

	Decoder() : count(0) {}
	UInt8	count;
	bool	join() { return _completed.wait(7000); }

	UInt32 decode(Packet& packet, const SocketAddress& address) {
		CHECK(Thread::CurrentId() != Thread::MainId);
		CHECK(!count);
		do {
			if (count++)
				_Handler.queue(onDecoded, Packet(packet, packet.data() + 2, 3), address);  // second time
			else
				_Handler.queue(onDecoded, Packet(packet, packet.data(), 5), address);  // first time
			packet += 5;
		} while (packet);
		CHECK(count == 2);
		_completed.set();
		return 0;
	}
private:
	UInt32 decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) { return decode(Packet(pBuffer), address); }
	Signal _completed;
};


ADD_TEST(DecoderTest, Manual) {

	Exception ex;
	Decoder decoder;

	UInt8 count(0);
	SocketAddress address(IPAddress::Loopback(), Util::Random<UInt16>());
	decoder.onDecoded = [&](Decoded& decoded) {
		CHECK(Thread::CurrentId()==Thread::MainId);
		CHECK(count < 2);
		CHECK(memcmp(decoded.data(), (count++ ? "msg" : "hello"), decoded.size())==0);
		CHECK(decoded.address == address);
	};
	std::thread([&] { decoder.decode(Packet(EXPAND("hello10msg")), address); }).detach();
	decoder.join();
	CHECK(_Handler.flush() == 2 && count == 2 && decoder.count == count);
}


ADD_TEST(DecoderTest, Socket) {

	Exception ex;
	IOSocket io(_Handler,_ThreadPool);
	Socket sender(Socket::TYPE_DATAGRAM);

	struct UDPReceiver : UDPSocket {
		UDPReceiver(IOSocket& io) : UDPSocket(io), _count(0) {}
		bool	join() { return _pDecoder->join(); }
	private:
		shared<Socket::Decoder> newDecoder() {
			CHECK(!_pDecoder);
			_pDecoder.reset(new Decoder());
			_pDecoder->onDecoded = [this](Decoded& decoded) {
				CHECK(Thread::CurrentId() == Thread::MainId);
				CHECK(_count < 2);
				CHECK(memcmp(decoded.data(), (_count++ ? "msg" : "hello"), decoded.size()) == 0);
			};
			return _pDecoder;
		}
		UInt8			_count;
		shared<Decoder> _pDecoder;
	};

	UDPReceiver receiver(io);
	CHECK(receiver.bind(ex, SocketAddress(IPAddress::Wildcard(),62435)) && !ex);

	sender.bind(ex, IPAddress::Loopback());
	
	CHECK(sender.sendTo(ex, EXPAND("hello10msg"), SocketAddress(IPAddress::Loopback(), receiver->address().port())) == 10 && !ex)

	CHECK(receiver.join());

	CHECK(_Handler.flush() >= 2); // 2 or 3 with SocketSend (udp writable!)
}


ADD_TEST(DecoderTest, File) {

	struct Decoded : Packet {
		Decoded(const Packet& packet, bool end) : end(end), Packet(move(packet)) {}
		const bool			end;
	};
	struct Decoder : File::Decoder {
		typedef Event<void(Decoded&)> ON(Decoded);

		bool join() { return _completed.wait(7000); }

	private:
		UInt32 decode(shared<Buffer>& pBuffer, bool end) {
			CHECK(Thread::CurrentId() != Thread::MainId);
			Packet packet(pBuffer);
			_Handler.queue(onDecoded, packet, end);
			if (!end) {
				CHECK(packet.size() == 0xFFFF);
				return packet.size();
			}
			_completed.set();
			return 0;
		}
		Signal _completed;
	};
	struct Reader : FileReader {
		Reader(IOFile& io) :FileReader(io),count(0), end(false) {}
		UInt8	count;
		bool	end;
		bool	join() { return _pDecoder->join(); }
	private:
		shared<File::Decoder> newDecoder() {
			CHECK(!_pDecoder);
			_pDecoder.reset(new Decoder());
			_pDecoder->onDecoded = [this](Decoded& decoded) {
				CHECK(Thread::CurrentId() == Thread::MainId);
				if (decoded.end) {
					CHECK(count > 0);
					end = true;
				} else
					CHECK(decoded.size() == 0xFFFF);
				++count;
			};
			return _pDecoder;
		}
		shared<Decoder> _pDecoder;
	};
	
	Exception ex;
	IOFile io(_Handler, _ThreadPool);
	Reader reader(io);
	reader.onError = [](const Exception& ex) { FATAL_ERROR(ex); };

	CHECK(reader.open(Path::CurrentApp()));
	reader.read();

	CHECK(reader.join());

	CHECK(_Handler.flush() == reader.count && reader.count && reader.end);
}
