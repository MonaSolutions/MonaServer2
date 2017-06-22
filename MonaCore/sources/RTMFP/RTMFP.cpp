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

#include "Mona/RTMFP/RTMFP.h"
#include "Mona/RTMFP/RTMFPWriter.h"
#include "Mona/Crypto.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

void RTMFP::Group::join(RTMFP::Member& member) {
	// Give the 6 peers closest (see https://drive.google.com/open?id=0B21tGxCEiSXGcHpYTkNOMHVvXzg),
	// to allow new member to estimate the more precisely possible N, and because its neighborns are desired
	// by this member (relating RTMFP spec), so give it immediatly rather wait that it find it itself (save time and resource).
	UInt8 count = 6;
	auto it = lower_bound(member);
	if(size()) {
		auto itLeft = it;
		auto itRight = it;
		for(;;) {
			if (itLeft != begin()) {
				if ((--itLeft)->second->writer()) { // if is opened (not closed)
					itLeft->second->writer().sendMember(member);
					if (!--count)
						break;
				}
			} else if (itRight == end())
				break;
			if (itRight != end()) {
				if (itRight->second->writer()) { // if is opened (not closed)
					itRight->second->writer().sendMember(member);
					if (!--count)
						break;
				}
				++itRight;
			}
		};
	}

	if (it != end() && member == *it->second)
		return;
	emplace_hint(it, member); // insert now, not before!
	DEBUG(member->address, " join group ", String::Hex(id, Entity::SIZE));
}

void RTMFP::Group::unjoin(RTMFP::Member& member) {
	if (!erase(member)) {
		ERROR(String::Hex(member, Entity::SIZE)," was not member of group ", String::Hex(id, Entity::SIZE));
		return;
	}
	DEBUG(member->address, " unjoin group ", String::Hex(id, Entity::SIZE));
	if (!empty())
		return;
	_groups.erase(id);
	delete this;
}

Buffer& RTMFP::InitBuffer(shared<Buffer>& pBuffer, UInt8 marker) {
	pBuffer.reset(new Buffer(6));
	return BinaryWriter(*pBuffer).write8(marker).write16(RTMFP::TimeNow()).buffer();
}

Buffer& RTMFP::InitBuffer(shared<Buffer>& pBuffer, std::atomic<Int64>& initiatorTime, UInt8 marker) {
	Int64 time = initiatorTime.exchange(0);
	if (!time)
		return InitBuffer(pBuffer, marker);
	time = Mona::Time::Now() - time;
	if(time>262140) // because is not convertible in RTMFP timestamp on 2 bytes, 0xFFFF*RTMFP::TIMESTAMP_SCALE = 262140
		return InitBuffer(pBuffer, marker);
	pBuffer.reset(new Buffer(6));
	return BinaryWriter(*pBuffer).write8(marker + 4).write16(RTMFP::TimeNow()).write16(RTMFP::Time(time)).buffer();
}

bool RTMFP::Send(Socket& socket, const Packet& packet, const SocketAddress& address) {
	Exception ex;
	int sent = socket.write(ex, packet, address);
	if (sent < 0) {
		DEBUG(ex);
		return false;
	}
	if (ex)
		DEBUG(ex);
	return true;
}

bool RTMFP::Engine::decode(Exception& ex, Buffer& buffer, const SocketAddress& address) {
	static UInt8 IV[KEY_SIZE];
	EVP_CipherInit_ex(_context, EVP_aes_128_cbc(), NULL, _key, IV, 0);
	int temp;
	EVP_CipherUpdate(_context, buffer.data(), &temp, buffer.data(), buffer.size());
	// Check CRC
	BinaryReader reader(buffer.data(), buffer.size());
	UInt16 crc(reader.read16());
	if (Crypto::ComputeChecksum(reader) != crc) {
		ex.set<Ex::Protocol>("Bad RTMFP CRC sum computing");
		return false;
	}
	buffer.clip(2);
	if (address)
		DUMP_REQUEST("RTMFP", buffer.data(), buffer.size(), address);
	return true;
}

shared<Buffer>& RTMFP::Engine::encode(shared<Buffer>& pBuffer, UInt32 farId, const SocketAddress& address) {
	if(address)
		DUMP_RESPONSE("RTMFP", pBuffer->data() + 6, pBuffer->size() - 6, address);

	int size = pBuffer->size();
	if (size > RTMFP::SIZE_PACKET)
		CRITIC("Packet exceeds 1192 RTMFP maximum size, risks to be ignored by client");
	// paddingBytesLength=(0xffffffff-plainRequestLength+5)&0x0F
	int temp = (0xFFFFFFFF - size + 5) & 0x0F;
	// Padd the plain request with paddingBytesLength of value 0xff at the end
	pBuffer->resize(size + temp);
	memset(pBuffer->data() + size, 0xFF, temp);
	size += temp;

	UInt8* data = pBuffer->data();

	// Write CRC (at the beginning of the request)
	BinaryReader reader(data, size);
	reader.next(6);
	BinaryWriter(data + 4, 2).write16(Crypto::ComputeChecksum(reader));
	// Encrypt the resulted request
	static UInt8 IV[KEY_SIZE];
	EVP_CipherInit_ex(_context, EVP_aes_128_cbc(), NULL, _key, IV, 1);
	EVP_CipherUpdate(_context, data+4, &temp, data + 4, size-4);

	reader.reset(4);
	BinaryWriter(data, 4).write32(reader.read32() ^ reader.read32() ^ farId);
	return pBuffer;
}

void RTMFP::ComputeAsymetricKeys(const UInt8* secret, UInt16 secretSize, const UInt8* initiatorNonce, UInt16 initNonceSize, const UInt8* responderNonce, UInt16 respNonceSize, UInt8* requestKey, UInt8* responseKey) {
	Crypto::HMAC::SHA256(responderNonce, respNonceSize, initiatorNonce, initNonceSize, requestKey);
	Crypto::HMAC::SHA256(initiatorNonce, initNonceSize, responderNonce, respNonceSize, responseKey);
	// now doing HMAC-sha256 of both result with the shared secret DH key
	Crypto::HMAC::SHA256(secret, secretSize, requestKey, Crypto::SHA256_SIZE, requestKey);
	Crypto::HMAC::SHA256(secret, secretSize, responseKey, Crypto::SHA256_SIZE, responseKey);
}

UInt32 RTMFP::ReadID(Buffer& buffer) {
	BinaryReader reader(buffer.data(), buffer.size());
	UInt32 id(0);
	for(UInt8 i=0;i<3;++i)
		id ^= reader.read32();
	buffer.clip(4);
	return id;
}

BinaryWriter& RTMFP::WriteAddress(BinaryWriter& writer, const SocketAddress& address, Location location) {
	const IPAddress& host = address.host();
	if (host.family() == IPAddress::IPv6)
		writer.write8(location | 0x80);
	else
		writer.write8(location);
	NET_SOCKLEN size(host.size());
	const UInt8* bytes = BIN host.data();
	for (NET_SOCKLEN i = 0; i<size; ++i)
		writer.write8(bytes[i]);
	return writer.write16(address.port());
}


}  // namespace Mona
