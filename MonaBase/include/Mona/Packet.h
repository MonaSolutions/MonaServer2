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
#include "Mona/Buffer.h"
#include "Mona/Exceptions.h"
#include <memory>

namespace Mona {
/*!
Packet is a ingenious buffer tool which can be mainly used in two complement ways:
- As a signature method requirement to allow an area of data to be distributed/shared without copying data.
		void method(const Packet& packet)
	To allow a sharing buffer referenced (without data copy) Packet constructor will make data referenced immutable.
- As a way to hold a sharing buffer (in buffering it if not already done)
		
Example:
	Packet unbuffered("data",4);
	Packet buffered(pBuffer);
	...
	std::deque<Packet> packets;
	// Here Packet wraps a unbuffered area of data
	packets.push(std::move(unbuffered));
	// Here Packet wraps a already buffered area of data, after this call pBuffer is empty (became immutable), and no copying data will occur anymore
	packets.push(std::move(buffered)); */
struct Packet: Binary, virtual Object {
	/*!
	Build an empty Packet */
	Packet() : _ppBuffer(&Null().buffer()), _data(NULL), _size(0), _reference(true) {}
	/*!
	Reference an immutable area of unbuffered data */
	Packet(const void* data, UInt32 size) : _data(BIN data), _size(size), _ppBuffer(&Null().buffer()), _reference(true) {}
	/*!
	Reference an immutable area of unbuffered data */
	Packet(const Binary& binary) : _data(BIN binary.data()), _size(binary.size()), _ppBuffer(&Null().buffer()), _reference(true) {}
	/*!
	Create a copy from packet */
	Packet(const Packet& packet) : _reference(true) { set(packet); }
	/*!
	Create a copy from packet and move area of data referenced, area have to be include inside */
	Packet(const Packet& packet, const UInt8* data, UInt32 size) : _reference(true) { set(packet, data, size); }
	/*!
	Bufferize packet */
	explicit Packet(const Packet&& packet) : _reference(true) { set(std::move(packet)); }
	/*!
	Bufferize packet and move area of data referenced, area have to be include inside */
	explicit Packet(const Packet&& packet, const UInt8* data, UInt32 size) : _reference(true) { set(std::move(packet)).setArea(data, size); }
	/*!
	Reference an immutable area of buffered data */
	Packet(const shared<const Binary>& pBuffer) : _reference(true) { set(pBuffer); }
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	Packet(const shared<const Binary>& pBuffer, const UInt8* data, UInt32 size) : _reference(true) { set(pBuffer, data, size); }
	/*!
	Reference an immutable area of buffered data */
	template<typename BinaryType>
	Packet(const shared<const BinaryType>& pBuffer) : _reference(true) { set<BinaryType>(pBuffer); }
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	template<typename BinaryType>
	Packet(const shared<const BinaryType>& pBuffer, const UInt8* data, UInt32 size) : _reference(true) { set<BinaryType>(pBuffer, data, size); }
	/*!
	Capture the buffer passed in parameter, buffer become immutable and allows safe distribution */
	template<typename BinaryType>
	explicit Packet(shared<BinaryType>& pBuffer) : _reference(true) { set<BinaryType>(pBuffer); }
	/*!
	Capture the buffer passed in parameter and move area of data referenced, buffer become immutable and allows safe distribution, area have to be include inside */
	template<typename BinaryType>
	explicit Packet(shared<BinaryType>& pBuffer, const UInt8* data, UInt32 size) : _reference(true) { set<BinaryType>(pBuffer, data, size); }
	/*!
	Release the referenced area of data */
	virtual ~Packet() { if (!_reference) delete _ppBuffer; }
	/*!
	Return buffer */
	const shared<const Binary>&	buffer() const { return *_ppBuffer; }
	/*!
	Return data */
	const UInt8*				data() const { return _data; }
	/*!
	Return size */
	UInt32						size() const { return _size; }
	/*!
	Move the area of data referenced */
	Packet& operator+=(UInt32 offset);
	/*!
	Move the area of data referenced */
	Packet& operator++() { return operator+=(1); }
	/*!
	Get a new area of data referenced (move) in a new packet */
	Packet  operator+(UInt32 offset) const;
	/*!
	[1] Get a new area of data referenced (resize) in a new packet */
	Packet  operator-(UInt32 count) const;
	/*!
	Resize the area of data referenced */
	Packet& operator-=(UInt32 count);
	/*!
	Resize the area of data referenced */
	Packet& operator--() { return operator-=(1); }
	/*!
	Reference an immutable area of unbuffered data */
	Packet& operator=(const Binary& binary) { return set(binary); }
	/*!
	Assign a copy from packet */
	Packet& operator=(const Packet& packet) { return set(packet); }
	/*!
	Bufferize packet */
	Packet& operator=(const Packet&& packet) { return set(std::move(packet)); }
	/*!
	Reference an immutable area of buffered data */
	Packet& operator=(const shared<const Binary>& pBuffer) { return set(pBuffer); }
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	template<typename BinaryType>
	Packet& operator=(const shared<const BinaryType>& pBuffer) { return set<BinaryType>(pBuffer); }
	/*!
	Capture the buffer passed in parameter, buffer become immutable and allows safe distribution */
	template<typename BinaryType>
	Packet& operator=(shared<BinaryType>& pBuffer) { return set<BinaryType>(pBuffer); }
	/*!
	Release packet */
	Packet& operator=(std::nullptr_t) { return set(NULL, 0); }
	/*!
	Release packet */
	Packet& reset() { return set(NULL, 0); }
	/*!
	Raccourcir packet */
	Packet& shrink(UInt32 available) { return setArea(_data, available); }
	/*!
	Reference an immutable area of unbuffered data */
	Packet& set(const void* data, UInt32 size);
	/*!
	Reference an immutable area of unbuffered data */
	Packet& set(const Binary& binary) { return set(binary.data(), binary.size()); }
	/*!
	Create a copy from packet */
	Packet& set(const Packet& packet);
	/*!
	Create a copy from packet and move area of data referenced, area have to be include inside */
	Packet& set(const Packet& packet, const UInt8* data, UInt32 size) { return set(packet).setArea(data, size); }
	/*!
	Bufferize packet */
	Packet& set(const Packet&& packet);
	/*!
	Bufferize packet and move area of data referenced, area have to be include inside */
	Packet& set(const Packet&& packet, const UInt8* data, UInt32 size) { return set(std::move(packet)).setArea(data, size); }
	/*!
	Reference an immutable area of buffered data */
	Packet& set(const shared<const Binary>& pBuffer);
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	Packet& set(const shared<const Binary>& pBuffer, const UInt8* data, UInt32 size) { return set(pBuffer).setArea(data, size); }
	/*!
	Reference an immutable area of buffered data */
	template<typename BinaryType>
	Packet& set(const shared<const BinaryType>& pBuffer) { return setIntern<const BinaryType>(pBuffer); }
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	template<typename BinaryType>
	Packet& set(const shared<const BinaryType>& pBuffer, const UInt8* data, UInt32 size) { return setIntern<const BinaryType>(pBuffer).setArea(data, size); }
	/*!
	Capture the buffer passed in parameter, buffer become immutable and allows safe distribution */
	template<typename BinaryType>
	Packet& set(shared<BinaryType>& pBuffer) {
		setIntern<BinaryType>(pBuffer);
		pBuffer.reset(); // forbid now all changes by caller!
		return *this;
	}
	/*!
	Capture the buffer passed in parameter and move area of data referenced, buffer become immutable and allows safe distribution, area have to be include inside */
	template<typename BinaryType>
	Packet& set(shared<BinaryType>& pBuffer, const UInt8* data, UInt32 size) { return set<BinaryType>(pBuffer).setArea(data, size); }

	static const Packet& Null() { static Packet Null(nullptr); return Null; }

private:
	Packet(std::nullptr_t) : _ppBuffer(new shared<const Binary>()), _data(NULL), _size(0), _reference(false) {}

	Packet& setArea(const UInt8* data, UInt32 size);

	const shared<const Binary>&	bufferize() const;

	template<typename BinaryType>
	Packet& setIntern(const shared<BinaryType>& pBuffer) {
		if (!pBuffer || !pBuffer->data()) // if size==0 the normal behavior is required to get the same data address
			return set(NULL, 0);
		if (!_reference)
			delete _ppBuffer;
		else
			_reference = false;
		_ppBuffer = new shared<const Binary>(pBuffer);
		_data = pBuffer->data();
		_size = pBuffer->size();
		return *this;
	}

	mutable const shared<const Binary>*	_ppBuffer;
	mutable const UInt8*				_data;
	mutable bool						_reference;
	UInt32								_size;
};


} // namespace Mona
