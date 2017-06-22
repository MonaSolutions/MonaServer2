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
#include "Mona/BinaryReader.h"
#include "Mona/BinaryWriter.h"
#include "Mona/ThreadPool.h"
#include "Mona/Socket.h"
#include "Mona/Crypto.h"
#include "Mona/Client.h"
#include "Mona/RendezVous.h"

namespace Mona {

struct RTMFPWriter;
struct RTMFPSender;
struct RTMFP : virtual Static {

	enum Location {
		LOCATION_UNSPECIFIED = 0,
		LOCATION_LOCAL = 1,
		LOCATION_PUBLIC = 2,
		LOCATION_REDIRECTION = 3
	};


	enum { TIMESTAMP_SCALE = 4 };
	enum { SENDABLE_MAX = 6 };

	enum {
		SIZE_HEADER = 11,
		SIZE_PACKET = 1192,
		SIZE_COOKIE = 0x40
	};

	enum {
		MESSAGE_OPTIONS			= 0x80,
		MESSAGE_WITH_BEFOREPART = 0x20,
		MESSAGE_WITH_AFTERPART  = 0x10,
		MESSAGE_RELIABLE		= 0x04, // not a RTMFP spec., just for a RTMFPPacket need
		MESSAGE_ABANDON			= 0x02,
		MESSAGE_END				= 0x01
	};

	struct Member {
		operator const UInt8*() { return client().id; }
		Client*					operator->() { return &client(); }
		virtual Client&			client() = 0;
		virtual RTMFPWriter&	writer() = 0;
	};
	struct Group : virtual Object, Entity, Entity::Map<Member> {
		Group(const UInt8* id, Entity::Map<Group>& groups) : Entity(id), _groups(groups) {
			INFO("Creation group ", String::Hex(id, Entity::SIZE));
		}
		~Group() {
			INFO("Deletion group ", String::Hex(id, Entity::SIZE));
		}
		void join(Member& member);
		void unjoin(Member& member);
	private:
		Entity::Map<RTMFP::Group>& _groups;
	};

	struct Engine : virtual Object {
		Engine(const UInt8* key) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
			_context = new EVP_CIPHER_CTX();
#endif
			memcpy(_key, key, KEY_SIZE); EVP_CIPHER_CTX_init(_context); 
		}
		Engine(const Engine& engine) { 
#if OPENSSL_VERSION_NUMBER < 0x10100000L
			_context = new EVP_CIPHER_CTX();
#endif
			memcpy(_key, engine._key, KEY_SIZE); EVP_CIPHER_CTX_init(_context); 
		}
		virtual ~Engine() { 
			EVP_CIPHER_CTX_cleanup(_context);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
			delete _context;
			_context = NULL;
#endif
		}

		bool			decode(Exception& ex, Buffer& buffer, const SocketAddress& address);
		shared<Buffer>&	encode(shared<Buffer>& pBuffer, UInt32 farId, const SocketAddress& address);

		static bool				Decode(Exception& ex, Buffer& buffer, const SocketAddress& address) { return Default().decode(ex, buffer, address); }
		static shared<Buffer>&	Encode(shared<Buffer>& pBuffer, UInt32 farId, const SocketAddress& address) { return Default().encode(pBuffer, farId, address); }

	private:
		static Engine& Default() { thread_local Engine Engine(BIN "Adobe Systems 02"); return Engine; }

		enum { KEY_SIZE = 0x10 };
		UInt8							_key[KEY_SIZE];
		EVP_CIPHER_CTX*					_context;
	};

	struct Handshake : Packet, virtual Object {
		Handshake(const Packet& packet, const SocketAddress& address, const shared<Packet>& pResponse) : pResponse(pResponse), address(address), Packet(std::move(packet)) {}
		const SocketAddress address;
		shared<Packet>		pResponse;
	};
	struct Message : virtual Object, Packet {
		Message(UInt64 flowId, UInt32 lost, const Packet& packet) : lost(lost), flowId(flowId), Packet(std::move(packet)) {}
		const UInt64 flowId;
		const UInt32 lost;
	};
	struct Flush : virtual Object {
		Flush(Int32 ping, bool keepalive, bool died, std::map<UInt64, Packet>& acks) :
			ping(ping), acks(std::move(acks)), keepalive(keepalive), died(died) {}
		const Int32						ping; // if died, ping takes error
		const bool						keepalive;
		const bool						died;
		const std::map<UInt64, Packet>	acks; // ack + fails
	};

	struct Session : virtual Object {
		typedef Event<void(SocketAddress&)>		ON(Address);
		typedef Event<void(RTMFP::Message&)>	ON(Message);
		typedef Event<void(RTMFP::Flush&)>		ON(Flush);

		Session(UInt32 id, UInt32 farId, const UInt8* farPubKey, UInt8 farPubKeySize, const UInt8* decryptKey, const UInt8* encryptKey, const shared<RendezVous>& pRendezVous) : 
				id(id), farId(farId), peerId(), pDecoder(new Engine(decryptKey)), pEncoder(new Engine(encryptKey)), pRendezVous(pRendezVous), initiatorTime(0) {
			Crypto::Hash::SHA256(farPubKey, farPubKeySize, BIN peerId);
		}
		const UInt32				id;
		const UInt32				farId;
		const UInt8					peerId[Entity::SIZE];
		const shared<Engine>		pDecoder;
		const shared<Engine>		pEncoder;
		const shared<RendezVous>	pRendezVous;
		std::atomic<Int64>			initiatorTime;
	};

	struct Output : virtual Object {
		virtual shared<RTMFPWriter>	newWriter(UInt64 flowId, const Packet& signature) = 0;
		virtual UInt64				resetWriter(UInt64 id) = 0;

		virtual UInt32		rto() const = 0;
		virtual void		send(const shared<RTMFPSender>& pSender) = 0;
		virtual UInt64		queueing() const = 0;
	};

	static bool				Send(Socket& socket, const Packet& packet, const SocketAddress& address);
	static Buffer&			InitBuffer(shared<Buffer>& pBuffer, UInt8 marker = 0x4a);
	static Buffer&			InitBuffer(shared<Buffer>& pBuffer, std::atomic<Int64>& initiatorTime, UInt8 marker = 0x4a);
	static void				ComputeAsymetricKeys(const UInt8* secret, UInt16 secretSize, const UInt8* initiatorNonce, UInt16 initNonceSize, const UInt8* responderNonce, UInt16 respNonceSize, UInt8* requestKey, UInt8* responseKey);

	static UInt32			ReadID(Buffer& buffer);

	static UInt16			TimeNow() { return Time(Mona::Time::Now()); }
	static UInt16			Time(Int64 time) { return UInt16(time / RTMFP::TIMESTAMP_SCALE); }


	static BinaryWriter&	WriteAddress(BinaryWriter& writer, const SocketAddress& address, Location location);

};

}  // namespace Mona
