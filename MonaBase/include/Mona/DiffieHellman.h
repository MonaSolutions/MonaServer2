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
#include "Mona/Exceptions.h"
#include OpenSSL(dh.h)


namespace Mona {

struct DiffieHellman : virtual Object {
	NULLABLE(!_pDH)

	enum { SIZE = 0x80 };

	DiffieHellman() : _pDH(NULL), _publicKeySize(0), _privateKeySize(0) {}
	~DiffieHellman() { if(_pDH) DH_free(_pDH);}

	bool	computeKeys(Exception& ex);
	UInt8	computeSecret(Exception& ex, const UInt8* farPubKey, UInt32 farPubKeySize, UInt8* sharedSecret);

	UInt8	publicKeySize() const { return _publicKeySize; }
	UInt8	privateKeySize() const { return _privateKeySize; }

	UInt8*	readPublicKey(UInt8* key) const;
	UInt8*	readPrivateKey(UInt8* key) const;

private:
	UInt8*	readKey(const BIGNUM *pKey, UInt8* key) const { BN_bn2bin(pKey, key); return key; }

	UInt8	_publicKeySize;
	UInt8	_privateKeySize;

	DH*		_pDH;
};


} // namespace Mona
