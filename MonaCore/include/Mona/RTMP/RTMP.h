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
#include "Mona/Packet.h"
#include "Mona/Time.h"
#include "Mona/AMF.h"
#include OpenSSL(rc4.h)

namespace Mona {


struct RTMP : virtual Static {
	enum {
		DEFAULT_CHUNKSIZE =	128,
		DEFAULT_WIN_ACKSIZE = 131072 // TODO default value?
	};

	static UInt32			GetDigestPos(const UInt8* data,UInt32 size,bool middle, UInt32& length);
	static UInt32			GetDHPos(const UInt8* data,UInt32 size,bool middle, UInt32& length);
	static const UInt8*		ValidateClient(const UInt8* data, UInt32 size,bool& middleKey,UInt32& keySize);
	static bool				WriteDigestAndKey(const UInt8* key,UInt32 keySize,bool middleKey,UInt8* data,UInt32 size);
	static void				ComputeRC4Keys(const UInt8* pubKey, UInt32 pubKeySize, const UInt8* farPubKey, UInt32 farPubKeySize, const UInt8* secret, UInt8 secretSize, RC4_KEY& decryptKey, RC4_KEY& encryptKey);


	struct Channel : virtual Object {
		Channel(UInt32	id) : id(id), absoluteTime(0), time(0), streamId(0xFFFFFFFF), bodySize(0), type(AMF::TYPE_EMPTY) {}
		const UInt32	id;
		AMF::Type		type;
		UInt32			time;
		UInt32			absoluteTime;
		UInt32			streamId;
		UInt32			bodySize;

		void reset() {
			bodySize = time = absoluteTime = 0;
			streamId = 0xFFFFFFFF;  // initialize to 0xFFFFFFFF to write a complete RTMP header on first sending
			type = AMF::TYPE_EMPTY;
		}
	};

	struct Request : virtual Object, Packet {
		Request(AMF::Type type, UInt32 time, UInt32 channelId, UInt32 streamId, UInt32 ackBytes, const Packet& packet, bool flush) :
			Packet(std::move(packet)), type(type), time(time), ackBytes(ackBytes), channelId(channelId), streamId(streamId), flush(flush) {}
		const AMF::Type	type;
		const UInt32	time;
		const UInt32	channelId;
		const UInt32	streamId;
		const UInt32	ackBytes;
		bool			flush;
	};

private:
	static const UInt8*		ValidateClientScheme(const UInt8* data, UInt32 size,bool middleKey,UInt32& keySize);
};


} // namespace Mona
