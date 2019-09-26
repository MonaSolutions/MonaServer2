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
	Reference an immutable area of unbuffered data (explicit to not reference a temporary memory zone) */
	explicit Packet(const void* data, UInt32 size) : _data(BIN data), _size(size), _ppBuffer(&Null().buffer()), _reference(true) {}
	/*!
	Create a copy from packet (explicit to not reference a temporary packet) */
	explicit Packet(const Packet& packet) : _reference(true) { set(packet); }
	/*!
	Create a copy from packet and move area of data referenced, area have to be include inside (explicit to not reference a temporary packet) */
	explicit Packet(const Packet& packet, const UInt8* data) : _reference(true) { set(packet, data); }
	explicit Packet(const Packet& packet, const UInt8* data, UInt32 size) : _reference(true) { set(packet, data, size); }
	/*!
	Bufferize packet */
	Packet(const Packet&& packet) : _reference(true) { set(std::move(packet)); }
	/*!
	Bufferize packet and move area of data referenced, area have to be include inside */
	Packet(const Packet&& packet, const UInt8* data) : _reference(true) { set(std::move(packet), data); }
	Packet(const Packet&& packet, const UInt8* data, UInt32 size) : _reference(true) { set(std::move(packet), data, size); }
	/*!
	Reference an immutable area of buffered data (explicit to not reference a temporary pBuffer) */
	template<typename BinaryType>
	explicit Packet(const shared<const BinaryType>& pBuffer) : _reference(true) { set(pBuffer); }
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside
	(explicit to not reference a temporary pBuffer) */
	template<typename BinaryType>
	explicit Packet(const shared<const BinaryType>& pBuffer, const UInt8* data) : _reference(true) { set(pBuffer, data); }
	template<typename BinaryType>
	explicit Packet(const shared<const BinaryType>& pBuffer, const UInt8* data, UInt32 size) : _reference(true) { set(pBuffer, data, size); }
	/*!
	Capture the buffer passed in parameter, buffer become immutable and allows safe distribution
	(explicit to note that pBuffer is release) */
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	explicit Packet(shared<BinaryType>& pBuffer) : _reference(true) { set(pBuffer); }
	/*!
	Capture the buffer passed in parameter and move area of data referenced, buffer become immutable and allows safe distribution, area have to be include inside
	(explicit to note that pBuffer is release) */
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	explicit Packet(shared<BinaryType>& pBuffer, const UInt8* data) : _reference(true) { set(pBuffer, data); }
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	explicit Packet(shared<BinaryType>& pBuffer, const UInt8* data, UInt32 size) : _reference(true) { set(pBuffer, data, size); }
	/*!
	Release the referenced area of data */
	virtual ~Packet() { if (!_reference) delete _ppBuffer; }
	/*!
	Allow to compare data packet*/
	bool operator == (const Packet& packet) const { return _size == packet._size && ((_size && memcmp(_data, packet._data, _size)==0) || _data == packet._data); }
	bool operator != (const Packet& packet) const { return !operator==(packet); }
	bool operator <  (const Packet& packet) const { return _size != packet._size ? _size < packet._size : (_size ? memcmp(_data, packet._data, _size) < 0 : _data < packet._data); }
	bool operator <= (const Packet& packet) const { return operator==(packet) || operator<(packet); }
	bool operator >  (const Packet& packet) const { return !operator<=(packet); }
	bool operator >= (const Packet& packet) const { return operator==(packet) || operator>(packet); }
	/*!
	Return buffer */
	const shared<const Binary>&	buffer() const { return *_ppBuffer; }
	/*!
	Return data */
	const UInt8*				data() const { return _data; }
	/*!
	Return size */
	UInt32						size() const { return _size; }

#if defined(_DEBUG)
	UInt8						operator[](UInt32 i) const;
#else
	UInt8						operator[](UInt32 i) const { return _data[i]; }
#endif

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
	Reference a packet */
	Packet& operator=(const Packet& packet) { return set(packet); }
	/*!
	Bufferize packet */
	Packet& operator=(const Packet&& packet) { return set(std::move(packet)); }
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	template<typename BinaryType>
	Packet& operator=(const shared<const BinaryType>& pBuffer) { return set(pBuffer); }
	/*!
	Capture the buffer passed in parameter, buffer become immutable and allows safe distribution */
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	Packet& operator=(shared<BinaryType>& pBuffer) { return set(pBuffer); }
	/*!
	Release packet */
	Packet& operator=(std::nullptr_t) { return set(NULL, 0); }
	/*!
	Release packet */
	Packet& reset() { return set(NULL, 0); }
	/*!
	Shrink packet */
	Packet& shrink(UInt32 available) { return setArea(_data, available); }
	/*!
	Move the area of data referenced */
	Packet& clip(UInt32 offset) { return operator+=(offset); }
	/*!
	Reference an immutable area of unbuffered data */
	Packet& set(const void* data, UInt32 size);
	/*!
	Reference a packet */
	Packet& set(const Packet& packet);
	/*!
	Reference a packet and move area of data referenced, area have to be include inside */
	Packet& set(const Packet& packet, const UInt8* data) { return set(packet).setArea(data, packet.size() - (data-packet.data())); }
	Packet& set(const Packet& packet, const UInt8* data, UInt32 size) { return set(packet).setArea(data, size); }
	/*!
	Bufferize packet */
	Packet& set(const Packet&& packet);
	/*!
	Bufferize packet and move area of data referenced, area have to be include inside */
	Packet& set(const Packet&& packet, const UInt8* data) { return set(std::move(packet)).setArea(data, packet.size() - (data - packet.data())); }
	Packet& set(const Packet&& packet, const UInt8* data, UInt32 size) { return set(std::move(packet)).setArea(data, size); }
	/*!
	Reference an immutable area of buffered data */
	template<typename BinaryType>
	Packet& set(const shared<const BinaryType>& pBuffer) {
		if (!pBuffer || !pBuffer->data())  // if pBuffer->size==0 the normal behavior is required to get the same data address
			return set(NULL, 0);
		if (!_reference)
			delete _ppBuffer;
		_reference = typeid(Binary) == typeid(BinaryType);
		_data = pBuffer->data();
		_size = pBuffer->size();
		_ppBuffer = _reference ? (shared<const Binary>*)&pBuffer : new shared<const Binary>(pBuffer);
		return self;
	}
	/*!
	Reference an immutable area of buffered data and move area of data referenced, area have to be include inside */
	template<typename BinaryType>
	Packet& set(const shared<const BinaryType>& pBuffer, const UInt8* data) { return set(pBuffer).setArea(data, pBuffer->size() - (data - pBuffer->data())); }
	template<typename BinaryType>
	Packet& set(const shared<const BinaryType>& pBuffer, const UInt8* data, UInt32 size) { return set(pBuffer).setArea(data, size); }
	/*!
	Capture the buffer passed in parameter, buffer become immutable and allows safe distribution */
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	Packet& set(shared<BinaryType>& pBuffer) {
		if (!pBuffer || !pBuffer->data()) // if size==0 the normal behavior is required to get the same data address
			return set(NULL, 0); // no need here to capture pBuffer (no holder on)
		if (!_reference)
			delete _ppBuffer;
		else
			_reference = false;
		_data = pBuffer->data();
		_size = pBuffer->size();
		_ppBuffer = new shared<const Binary>(move(pBuffer)); // forbid now all changes by caller!
		return self;
	}
	/*!
	Capture the buffer passed in parameter and move area of data referenced, buffer become immutable and allows safe distribution, area have to be include inside */
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	Packet& set(shared<BinaryType>& pBuffer, const UInt8* data) { return set(pBuffer).setArea(data, pBuffer->size() - (data - pBuffer->data())); }
	template<typename BinaryType, typename = typename std::enable_if<!std::is_const<BinaryType>::value>::type>
	Packet& set(shared<BinaryType>& pBuffer, const UInt8* data, UInt32 size) { return set(pBuffer).setArea(data, size); }

	static const Packet& Null() { static Packet Null(nullptr); return Null; }

private:
	Packet(std::nullptr_t) : _ppBuffer(new shared<const Binary>()), _data(NULL), _size(0), _reference(false) {}

	Packet& setArea(const UInt8* data, UInt32 size);

	const shared<const Binary>&	bufferize() const;

	mutable const shared<const Binary>*	_ppBuffer;
	mutable const UInt8*				_data;
	mutable bool						_reference;
	UInt32								_size;
};


} // namespace Mona
