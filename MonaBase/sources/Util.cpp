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

#include "Mona/Util.h"
#include "Mona/Timezone.h"
#include <fstream>
#if !defined(_WIN32)
	#include <unistd.h>
	#include <sys/syscall.h>
	extern "C" char **environ;
#endif


using namespace std;

namespace Mona {

const char* Util::_URICharReserved("%<>{}|\\\"^`#?\x7F");

const char Util::_B64Table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char Util::_ReverseB64Table[128] = {
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
	64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
	64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

const Parameters& Util::Environment() {
	static struct Environment : Parameters, virtual Object {
		Environment() {
			const char* line(*environ);
			for (UInt32 i = 0; (line = *(environ + i)); ++i) {
				const char* value = strchr(line, '=');
				if (value) {
					string name(line, (value++) - line);
					setString(name, value);
				} else
					setString(line, NULL);
			}
		}
	} Environment;
	return Environment;
}

size_t Util::UnpackUrl(const char* url, string& address, string& path, string& query) {
	
	path.clear();
	query.clear();


	const char* dot(strpbrk(url, ":/\\"));
	if (!dot) {
		path.assign("/").append(url);
		return 1; // is file!
	}

	
	if (*dot == ':') {
		// protocol://address
		++dot;
		while (*dot && (*dot == '/' || *dot == '\\'))
			++dot;
		if (!*dot) // no address, no path, just "scheme://"
			return string::npos;
		const char* itEnd(dot);
		while (*itEnd && *itEnd != '/' && *itEnd != '\\')
			++itEnd;
		address.assign(dot, itEnd);
		url = itEnd; // on slash after address
	}

	// Decode PATH

	vector<size_t> slashs;

	// level = 0 => path => next char
	// level = 1 => path => /
	// level > 1 => path => . after /
	UInt8 level(1); 

	ForEachDecodedChar forEach([&level,&slashs,&path,&query](char c,bool wasEncoded){

		if (c == '?')
			return false;

		// path

		if (c == '/' || c == '\\') {

			// level + 1 = . level
			if (level > 2) {
				// /../
				if (!slashs.empty()) {
					path.resize(slashs.back());
					slashs.pop_back();
				} else
					path.clear();
			}
			level = 1;

			return true;
		}
		
		if (level) {
			// level = 1 or 2
			if (level<3 && c == '.') {
				++level;
				return true;
			}
			slashs.emplace_back(path.size());
			path.append("/..",level);
			level = 0;
		}

		// Add current character
		path += c;

		return true;
	});

	url += DecodeURI(url,forEach);

	// get QUERY
	if (*url)
		query.assign(url);

	if (level) {
		if (level > 2) // /..
			path.resize(slashs.empty() ? 0 : slashs.back());
		return string::npos;
	}
	return slashs.back()+1; // can't be empty here!
}

Parameters& Util::UnpackQuery(const char* query, size_t count, Parameters& parameters) {
	ForEachParameter forEach([&parameters](const string& key, const char* value) {
		parameters.setString(key, value);
		return true;
	});
	UnpackQuery(query, count, forEach);
	return parameters;
}

UInt32 Util::UnpackQuery(const char* query, size_t count, const ForEachParameter& forEach) {
	Int32 countPairs(0);
	string name,value;
	bool isName(true);

	ForEachDecodedChar forEachDecoded([&isName, &countPairs, &name, &value, &forEach](char c, bool wasEncoded) {

		if (!wasEncoded) {
			if (c == '&') {
				++countPairs;
				if (!forEach(name, isName ? NULL : value.c_str())) {
					countPairs *= -1;
					return false;
				}
				isName = true;
				value.clear();
				name.clear();
				return true;
			}
			if (c == '=') {
				if (isName) {
					isName = false;
					return true;
				}
			} else if (c == '+')
				c = ' ';
		}
		if (isName)
			name += c;
		else
			value += c; 
		return true;
	});

	if (DecodeURI(query, count, forEachDecoded) && countPairs>=0) {
		// for the last pairs just if there was some decoded bytes
		++countPairs;
		forEach(name, isName ? NULL : value.c_str());
	}

	return abs(countPairs);
}

UInt32 Util::DecodeURI(const char* value, std::size_t count, const ForEachDecodedChar& forEach) {

	const char* begin(value);

	while (count && (count!=string::npos || *value)) {

		char c(*value);
		bool encoded(false);

		if (c == '%') {
			// %
			++value;
			if(count!=string::npos)
				--count;
			if (!count || (count==string::npos && !*value)) {
				 // syntax error
				forEach('%',encoded);
				return value-begin;
			}
			
			char hi = toupper(*value);
			++value;
			if(count!=string::npos)
				--count;
			if (!count || (count==string::npos && !*value)) {
				// syntax error
				if (forEach('%',encoded))
					forEach(hi,encoded);
				else
					--value;
				return value-begin;
			}
			char lo = toupper(*value++);
if (count != string::npos)
--count;
if (!isxdigit(lo) || !isxdigit(hi)) {
	// syntax error
	if (forEach('%', encoded)) {
		if (forEach(hi, encoded)) {
			if (forEach(lo, encoded))
				continue;
		} else
			--value;
	} else
		value -= 2;
	return value - begin;
}
encoded = true;
c = ((hi - (hi <= '9' ? '0' : '7')) << 4) | ((lo - (lo <= '9' ? '0' : '7')) & 0x0F);
		} else {
		++value;
		if (count != string::npos)
			--count;
		}
		if (!forEach(c, encoded))
			break;
	}

	return value - begin;
}

void Util::Dump(const UInt8* data, UInt32 size, Buffer& buffer) {
	UInt8 b;
	UInt32 c(0);
	buffer.resize((UInt32)ceil((double)size / 16) * 67, false);

	const UInt8* end(data + size);
	UInt8*		 out(buffer.data());

	while (data < end) {
		c = 0;
		*out++ = '\t';
		while ((c < 16) && (data < end)) {
			b = *data++;
			snprintf((char*)out, 4, "%X%X ", b >> 4, b & 0x0f);
			out += 3;
			++c;
		}
		data -= c;
		while (c++ < 16) {
			memcpy((char*)out, "   \0", 4);
			out += 3;
		}
		*out++ = ' ';
		c = 0;
		while ((c < 16) && (data < end)) {
			b = *data++;
			if (b > 31)
				*out++ = b;
			else
				*out++ = '.';
			++c;
		}
		while (c++ < 16)
			*out++ = ' ';
		*out++ = '\n';
	}
}



bool Util::ReadIniFile(const string& path, Parameters& parameters) {
	ifstream ifile(path, ios::in | ios::binary | ios::ate);
	if (!ifile.good())
		return false;
	UInt32 size = (UInt32)ifile.tellg();
	if (size == 0)
		return true;
	vector<char> buffer(size);
	ifile.seekg(0);
	ifile.read(buffer.data(), size);
	char* cur = buffer.data();
	const char* end = cur+size;
	const char* key, *value;
	string section;
	while (cur < end) {
		// skip space
		while (isspace(*cur) && ++cur < end);
		if (cur == end)
			break;

		// skip comments
		if (*cur == ';') {
			while (++cur < end && *cur != '\n');
			++cur;
			continue;
		}
		
		// line
		key = cur;
		value = NULL;
		size_t vSize(0), kSize(0);
		do {
			if (*cur == '\n')
				break;
			if (value)
				++vSize;
			else if (*cur == '=')
				value = cur+1;
			else
				++kSize;
		} while (++cur < end);

		if (vSize)
			vSize = String::Trim(value, vSize);
		if(kSize)
			kSize = String::TrimRight(key, kSize);

		if (*key=='[' && ((value && value[vSize-1] == ']') || (!value && key[kSize-1]==']'))) {
			// section
			// remove ]
			if (value)
				--vSize; 
			else
				--kSize;
			parameters.setString(section.assign(++key, --kSize), value, vSize);
			section += '.';
		} else
			parameters.setString(string(section).append(key, kSize), value, vSize);
	}

	return true;
}



} // namespace Mona
