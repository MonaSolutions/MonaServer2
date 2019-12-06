/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "BinaryReader.h"
#include "Mona/Exceptions.h"
#include OpenSSL(hmac.h)
#include OpenSSL(err.h)

namespace Mona {


typedef UInt8 ROTATE_OPTIONS;
enum {
	ROTATE_INPUT = 1,
	ROTATE_OUTPUT = 2
};

struct Crypto : virtual Static {
	enum {
		MD5_SIZE = 16,
		SHA1_SIZE = 20,
		SHA256_SIZE = 32
	}; 

	static unsigned long LastError() { return ERR_get_error(); }
	static const char*   ErrorToMessage(unsigned long error) {
		if (!error)
			return "No error";
		const char* reason(ERR_reason_error_string(error));
		return reason ? reason : "Unknown error";
	}
	static const char*   LastErrorMessage() { return ErrorToMessage(LastError()); }

	static UInt8  Rotate8(UInt8 value);
	static UInt16 Rotate16(UInt16 value);
	static UInt32 Rotate24(UInt32 value);
	static UInt32 Rotate32(UInt32 value);
	static UInt64 Rotate64(UInt64 value);

	static UInt16 ComputeChecksum(BinaryReader& reader);

	static UInt32 ComputeCRC32(const UInt8* data, UInt32 size, ROTATE_OPTIONS options =0);


	struct Hash : virtual Static {
		static UInt8* MD5(UInt8* value, size_t size) { return Compute(EVP_md5(), value, size, value); }
		static UInt8* MD5(const void* data, size_t size, UInt8* value) { return Compute(EVP_md5(), data, size, value); }
		static UInt8* SHA1(UInt8* value, size_t size) { return Compute(EVP_sha1(), value, size, value); }
		static UInt8* SHA1(const void* data, size_t size, UInt8* value) { return Compute(EVP_sha1(), data, size, value); }
		static UInt8* SHA256(UInt8* value, size_t size) { return Compute(EVP_sha256(), value, size, value); }
		static UInt8* SHA256(const void* data, size_t size, UInt8* value) { return Compute(EVP_sha256(), data, size, value); }

		static UInt8* Compute(const EVP_MD* evp, const void* data, size_t size, UInt8* value);
	};

	struct HMAC : virtual Static {
		static UInt8* MD5(const void* key, int keySize, UInt8* value, size_t size) { return Compute(EVP_md5(), key, keySize, value, size, value); }
		static UInt8* MD5(const void* key, int keySize, const void* data, size_t size, UInt8* value) { return Compute(EVP_md5(), key, keySize, data, size, value); }
		static UInt8* SHA1(const void* key, int keySize, UInt8* value, size_t size) { return Compute(EVP_sha1(), key, keySize, value, size, value); }
		static UInt8* SHA1(const void* key, int keySize, const void* data, size_t size, UInt8* value) { return Compute(EVP_sha1(), key, keySize, data, size, value); }
		static UInt8* SHA256(const void* key, int keySize, UInt8* value, size_t size) { return Compute(EVP_sha256(), key, keySize, value, size, value); }
		static UInt8* SHA256(const void* key, int keySize, const void* data, size_t size, UInt8* value) { return Compute(EVP_sha256(), key, keySize, data, size, value); }

		static UInt8* Compute(const EVP_MD* evp, const void* key, int keySize, const void* data, size_t size, UInt8* value);
	};

};



} // namespace Mona
