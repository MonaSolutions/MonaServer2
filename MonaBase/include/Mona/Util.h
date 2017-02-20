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
#include "Mona/Time.h"
#include "Mona/Exceptions.h"
#include "Mona/Buffer.h"
#include "math.h"
#include <limits>
#include <mutex>

namespace Mona {

struct Util : virtual Static {
	static void Dump(const UInt8* data, UInt32 size, Buffer& buffer);

	static const Parameters& Environment();
	 

	static bool ReadIniFile(const std::string& path, Parameters& parameters);

	/// \brief Unpack url in path and query
	/// \param url Url to unpack
	/// \param path Part of the url between host address and '?'
	/// \param query Part of the url after '?' (if present)
	/// \return The position of the file in path (std::npos for a directory)
	static std::size_t UnpackUrl(const std::string& url, std::string& path, std::string& query) {std::string address; return UnpackUrl(url, address, path, query);}
	static std::size_t UnpackUrl(const char* url, std::string& path, std::string& query) {std::string address; return UnpackUrl(url, address, path, query);}
	static std::size_t UnpackUrl(const std::string& url, std::string& address, std::string& path, std::string& query) { return UnpackUrl(url.c_str(), address, path, query); }
	static std::size_t UnpackUrl(const char* url, std::string& address, std::string& path, std::string& query);
	
	typedef std::function<bool(const std::string&, const char*)> ForEachParameter;

	static Parameters& UnpackQuery(const std::string& query, Parameters& parameters) { return UnpackQuery(query.data(), query.size(), parameters); }
	static Parameters& UnpackQuery(const char* query, std::size_t count, Parameters& parameters);
	static Parameters& UnpackQuery(const char* query, Parameters& parameters) { return UnpackQuery(query, std::string::npos, parameters); }

	/// \return the number of key/value found
	static UInt32 UnpackQuery(const std::string& query, const ForEachParameter& forEach) { return UnpackQuery(query.data(), query.size(), forEach); }
	static UInt32 UnpackQuery(const char* query, std::size_t count, const ForEachParameter& forEach);
	static UInt32 UnpackQuery(const char* query, const ForEachParameter& forEach) { return UnpackQuery(query, std::string::npos, forEach); }


	typedef std::function<bool(char c,bool wasEncoded)> ForEachDecodedChar;

	static UInt32 DecodeURI(const std::string& value, const ForEachDecodedChar& forEach) { return DecodeURI(value.data(),value.size(),forEach); }
	static UInt32 DecodeURI(const char* value, const ForEachDecodedChar& forEach)  { return DecodeURI(value,std::string::npos,forEach); }
	static UInt32 DecodeURI(const char* value, std::size_t count, const ForEachDecodedChar& forEach);
	

	template <typename BufferType>
	static BufferType&	EncodeURI(const char* in, BufferType& buffer) { return EncodeURI(in, std::string::npos, buffer); }

	template <typename BufferType>
	static BufferType&	EncodeURI(const char* in, std::size_t count, BufferType& buffer) {
		while (count && (count!=std::string::npos || *in)) {
			char c = *in++;
			if (isxml(c))
				buffer.append(&c, 1);
			else if (c <= 0x20 || c > 0x7E || strchr(_URICharReserved,c))
				String::Append(buffer, '%', String::Format<UInt8>("%2X", (UInt8)c));
			else
				buffer.append(&c, 1);
			if(count!=std::string::npos)
				--count;
		}
		return buffer;
	}

	template<typename Type=bool>
	static Type Random() {
		static UInt32 x = (UInt32)Time::Now();
		static UInt32 y = 362436069, z = 521288629, w = 88675123;
		UInt32 t = x ^ (x << 11);
		x = y; y = z; z = w;
		return (w = w ^ (w >> 19) ^ (t ^ (t >> 8))) % std::numeric_limits<Type>::max();
	}
	static void Random(UInt8* data, UInt32 size) {for (UInt32 i = 0; i < size; ++i) data[i] = Random<UInt8>();}

	
	template <typename BufferType>
	static BufferType& ToBase64(const UInt8* data, UInt32 size, BufferType& buffer,bool append=false) {
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
	static bool FromBase64(const UInt8* data, UInt32 size,BufferType& buffer,bool append=false) {
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
	static const char*						_URICharReserved;
	static const char						_B64Table[65];
	static const char						_ReverseB64Table[128];
};


template<> inline bool Util::Random() { return Random<UInt8>() % 2 ? true : false; }

} // namespace Mona
