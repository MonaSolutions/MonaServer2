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
#include "Mona/Parameters.h"
#include "Mona/Process.h"
#include "Mona/Path.h"

namespace Mona {

struct Util : virtual Static {
	template<typename Type>
	struct Scoped {
		Scoped(Type& value, const Type& tempValue) : _value(value), _oldValue(value) { value = tempValue; }
		~Scoped() { _value = _oldValue; }
		operator const Type&() const { return _oldValue; }
	private:
		const Type _oldValue;
		Type& _value;
	};

	static void Dump(const UInt8* data, UInt32 size, Buffer& buffer);

	static const Parameters& Environment();
	 
	static UInt64	Random();
	template<typename Type>
	static Type		Random() { return Type(Random()); } // cast gives the modulo!
	static void		Random(UInt8* data, UInt32 size) { for (UInt32 i = 0; i < size; ++i) data[i] = UInt8(Random()); }

	static const UInt8 UInt8Generators[];

	template<typename Type, typename ResultType = typename std::make_signed<Type>::type>
	static ResultType Distance(Type pt1, Type pt2) {
		ResultType result(pt2 - pt1);
		return Mona::abs(result) > std::ceil(std::numeric_limits<typename std::make_unsigned<ResultType>::type>::max() / 2.0) ? (pt1 - pt2) : result;
	}

	template<typename Type, typename ResultType = typename std::make_signed<Type>::type>
	static ResultType Distance(Type pt1, Type pt2, Type max, Type min = 0) {
		DEBUG_ASSERT(min <= pt1 && pt1 <= max);
		DEBUG_ASSERT(min <= pt2 && pt2 <= max);
		ResultType result(pt2 - pt1);
		max -= min;
		if (Mona::abs(result) <= std::ceil(max / 2.0))
			return result;
		return result>0 ? (result - max - 1) : (result + max + 1);
	}
	
	template<typename Type, typename TypeD>
	static Type AddDistance(Type pt, TypeD distance, Type max, Type min = 0) {
		DEBUG_ASSERT(min <= pt && pt <= max);
		typename std::make_unsigned<Type>::type delta;
		if (distance >= 0) { // move on the right
			while (typename std::make_unsigned<TypeD>::type(distance) > (delta = (max - pt))) {
				pt = min;
				distance -= delta + 1;
			}
			return pt + distance;
		}
		// move on the left (distance<0), TypeD is necessary signed!
		distance = -typename std::make_signed<TypeD>::type(distance);
		while (typename std::make_unsigned<TypeD>::type(distance) > (delta = (pt - min))) {
			pt = max;
			distance -= delta + 1;
		}
		return pt - distance;
	}

	static bool ReadIniFile(const std::string& path, Parameters& parameters);

	template <typename BufferType>
	static BufferType& ToBase64(const UInt8* data, UInt32 size, BufferType& buffer, bool append=false) {
		UInt32 accumulator(buffer.size()),bits(0);

		if (!append)
			accumulator = 0;
		buffer.resize(accumulator+UInt32(ceil(size/3.0)*4));

		char* current((char*)buffer.data());
		if (!current) // to expect null writer 
			return buffer;
		const char*	end(current+buffer.size());
		current += accumulator;

		const UInt8* endData = data + size;
		
		accumulator = 0;
		while(data<endData) {
			accumulator = (accumulator << 8) | (*data++ & 0xFFu);
			bits += 8;
			while (bits >= 6) {
				bits -= 6;
				*current++ = _B64Table[(accumulator >> bits) & 0x3Fu];
			}
		}
		if (bits > 0) { // Any trailing bits that are missing.
			accumulator <<= 6 - bits;
			*current++ = _B64Table[accumulator & 0x3Fu];
		}
		while (current<end) // padding with '='
			*current++ = '=';
		return buffer;
	}


	template <typename BufferType>
	static bool FromBase64(BufferType& buffer) { return FromBase64(BIN buffer.data(), buffer.size(), buffer); }

	template <typename BufferType>
	static bool FromBase64(const UInt8* data, UInt32 size,BufferType& buffer, bool append=false) {
		if (!buffer.data())
			return false; // to expect null writer 

		UInt32 bits(0), oldSize(append ? buffer.size() : 0);
		UInt32 accumulator(oldSize + UInt32(ceil(size / 4.0) * 3));
		const UInt8* end(data+size);

		if (buffer.size()<accumulator)
			buffer.resize(accumulator); // maximum size!
		UInt8* out(BIN buffer.data() + oldSize);

		accumulator = size = 0;

		while(data<end) {
			const int c = *data++;
			if (isspace(c) || c == '=')
				continue;

			if ((c > 127) || (c < 0) || (_ReverseB64Table[c] > 63)) {
				// reset the oldSize
				buffer.resize(oldSize);
				return false;
			}
		
			accumulator = (accumulator << 6) | _ReverseB64Table[c];
			bits += 6;
			if (bits >= 8) {
				bits -= 8;
				size++;
				*out++ = ((accumulator >> bits) & 0xFFu);
			}
		}
		buffer.resize(oldSize+size);
		return true;
	}


private:
	static const char						_B64Table[65];
	static const char						_ReverseB64Table[128];
};

} // namespace Mona
