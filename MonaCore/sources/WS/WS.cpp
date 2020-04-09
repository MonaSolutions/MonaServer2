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

#include "Mona/WS/WS.h"
#include "Mona/Crypto.h"

using namespace std;



namespace Mona {

UInt16 WS::ErrorToCode(Int32 error) {
	if (!error)
		return WS::CODE_NORMAL_CLOSE;
	switch (error) {
		case Session::ERROR_UPDATE:
		case Session::ERROR_SERVER:
			return WS::CODE_ENDPOINT_GOING_AWAY;
		case Session::ERROR_APPLICATION:
			return WS::CODE_PAYLOAD_NOT_ACCEPTABLE;
		case Session::ERROR_REJECTED:
			return WS::CODE_POLICY_VIOLATION;
		case Session::ERROR_IDLE:
		case Session::ERROR_PROTOCOL:
			return WS::CODE_PROTOCOL_ERROR;
		case Session::ERROR_UNAVAILABLE:
		case Session::ERROR_UNFOUND:
		case Session::ERROR_UNSUPPORTED:
			return WS::CODE_EXTENSION_REQUIRED;
		case Session::ERROR_UNEXPECTED:
			return WS::CODE_UNEXPECTED_CONDITION; // Expectation failed
		default: // User close
			return range<UInt16>(error);
	}
}

BinaryWriter& WS::WriteKey(BinaryWriter& writer, const string& key) {
	string value(key);
	value.append(EXPAND("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));  // WEBSOCKET_GUID
	return Util::ToBase64(Crypto::Hash::SHA1(BIN value.data(), value.size()), Crypto::SHA1_SIZE, writer, true);
}

BinaryReader& WS::Unmask(BinaryReader& reader) {
	UInt32 i;
	const UInt8* mask(reader.current());
	reader.next(4);
	UInt8* bytes = BIN reader.current();
	for(i=0;i<reader.available();++i)
		bytes[i] ^= mask[i%4];
	return reader;
}




} // namespace Mona
