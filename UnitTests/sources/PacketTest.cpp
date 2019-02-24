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
#include "Mona/Packet.h"
#include "Mona/String.h"
#include <deque>

using namespace Mona;
using namespace std;

namespace PacketTest {

void PacketATA(const Packet& packet, bool buffered) {
	CHECK(memcmp(packet.data(), "ata", 3) == 0 && packet.size() == 3 && packet.buffer().operator bool()==buffered);
}

void PacketDAT(const Packet& packet, bool buffered) {
	CHECK(memcmp(packet.data(), "dat", 3) == 0 && packet.size() == 3 && packet.buffer().operator bool()==buffered);
}

void PacketT(const Packet& packet, bool buffered) {
	CHECK(memcmp(packet.data(), "t", 1) == 0 && packet.size() == 1 && packet.buffer().operator bool()==buffered);
}

ADD_TEST(Unbuffered) {

	Packet packet;

	CHECK(!packet && !packet.buffer() && !packet.data() && !packet.size());
	CHECK(memcmp(packet.set(EXPAND("data")).data(), "data", 4) == 0 && packet.size() == 4 && !packet.buffer());

	PacketATA(Packet(packet,packet.data()+1,packet.size()-1),false);
	
	PacketDAT(Packet(packet,packet.data(),3),false);

	packet += 1;
	CHECK(memcmp(packet.data(), "ata", 3) == 0 && packet.size() == 3 && !packet.buffer());

	packet -= 1;
	CHECK(memcmp(packet.data(), "at", 2) == 0 && packet.size() == 2 && !packet.buffer());

	PacketT(Packet(packet, packet.data() + 1, 1 ), false);

	packet.reset();
	CHECK(!packet.data() && packet.size() == 0 && !packet.buffer());
}

ADD_TEST(Buffered) {

	shared<Buffer> pBuffer(SET, 4,"data");

	Packet packet;
	CHECK(!packet && !packet.buffer() && !packet.data() && !packet.size() && pBuffer && pBuffer->data() && pBuffer->size());

	CHECK(memcmp(packet.set(pBuffer).data(), "data", 4) == 0 && packet.size() == 4 && packet && packet.buffer());

	PacketATA(Packet(packet,packet.data()+1,packet.size()-1),true);
	
	PacketDAT(Packet(packet,packet.data(),3),true);
	
	packet += 1;
	CHECK(memcmp(packet.data(), "ata", 3) == 0 && packet.size() == 3 && packet.buffer());

	packet -= 1;
	CHECK(memcmp(packet.data(), "at", 2) == 0 && packet.size() == 2 && packet.buffer());

	PacketT(Packet(packet, packet.data() + 1, 1 ), true);
	
	packet.reset();
	CHECK(!packet.data() && packet.size() == 0 && !packet.buffer());
}

ADD_TEST(Bufferize) {
	deque<Packet> packets;
	Packet packet(EXPAND("data"));
	
	// test unbuffered data
	CHECK(!packet.buffer());
	const UInt8* data(packet.data());
	packets.emplace_back(move(packet));
	CHECK(packet.buffer() && &packet.buffer() != &packets.back().buffer() && data != packet.data());

	// test buffered data
	shared<Buffer> pBuffer(SET, packet.size(), packet.data());
	CHECK(packet.set(pBuffer).buffer());
	data = packet.data();
	packets.emplace_back(move(packet));
	CHECK(packet.buffer() && &packet.buffer() != &packets.back().buffer() && data == packet.data());

	packet = nullptr;
	CHECK(!packet.buffer());
}

}
