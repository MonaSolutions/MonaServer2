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


#include "Mona/URL.h"


using namespace std;

namespace Mona {

const char* URL::Parse(const char* url, size_t& size, string& protocol, string& address) {
	protocol.clear();
	address.clear();

	// Fill protocol and address!
	/// level = 0 => protocol
	/// level = 1-3 => ://
	/// level = 3 => address
	const char* cur = url;
	size_t rest = size;
	UInt8 level(0);
	bool trimLeft = true;
	while(rest) {
		switch (*cur) {
			case '?':
				// query now
				if (level) {
					level = 4;
					break;
				}
				// just query!
			case 0:
				rest = 0;
				continue;
			case '\\':
			case '/':
				if (!level) {// just path!
					rest = 0;
					continue;
				}
				++level;
				break;
			case ':':
				if (!level) {
					address.clear(); // usefull if methods is using with the same string reference for protocol + address
					level = 1;
					break;
				}
			default: /// other char!
				if (level) {
					level = 3;
					address += *cur;
				} else {
					if (trimLeft) {
						if(isspace(*cur))
							break;
						trimLeft = false;
					}
					protocol += *cur;
				}
					
		}
		if (level > 3)
			break;
		++cur;
		if(rest !=string::npos)
			--rest;
	};

	if (level) { // => has gotten a protocol!
		// rest is request
		// cur must be a relative path excepting if file protocol
		url = cur;
		while (rest && (*cur == '/' || *cur == '\\')) {
			++cur;
			if (rest != string::npos)
				--rest;
		}
		if (url != cur && protocol == "file") {
			// always absolute if file!
			--cur;
			if (rest != string::npos)
				++rest;
		}
		size = rest;
		url = cur;
	} else { // url is just the request
		protocol.clear();
		address.clear();
	}
	if (size == string::npos)
		size = strlen(url);
	return url; // return request
}


const char* URL::ParseRequest(const char* request, std::size_t& size, std::string& path, REQUEST_OPTIONS options) {
	// Decode PATH

	bool relative;
	while (!(relative = !size || (*request != '/' && *request != '\\')) && (options & REQUEST_FORCE_RELATIVE)) {
		++request;
		if (size != std::string::npos)
			--size;
	}
	// allow to fix /c:/ to c:/ (and don't impact linux, if was already absolute it removes just the first slash)
	if (!relative && FileSystem::IsAbsolute(request+1)) {
		++request;
		if (size != std::string::npos)
			--size;
	}

	/// level = 0 => path => next char
	/// level = 1 => path => /
	/// level > 1 => path => . after /
	UInt8 level(0);
	vector<size_t> slashs;
	path.clear();
	String::ForEachDecodedChar forEach([&](char c, bool wasEncoded) {
		if (!wasEncoded && c == '?')
			return false; // query

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
			if (level<3 && !wasEncoded && c == '.') {
				++level;
				return true;
			}
			slashs.emplace_back(path.size());
			path.append("/..", level);
			level = 0;
		}

		// Add current character
		path += c;

		return true;
	});

	UInt32 count = String::FromURI(request, size, forEach);
	// Get query!
	request += count;
	if (size != std::string::npos)
		size -= count;
	else
		size = strlen(request);

	if(level) { // is folder
		if (level > 2) // /..
			path.resize(slashs.empty() ? 0 : slashs.back());
		path += '/';
	} else if(options & REQUEST_MAKE_FOLDER)
		path += '/';
	if (relative)
		MAKE_RELATIVE(path);
	return request; // returns query!
}

const char* URL::ParseRequest(const char* request, std::size_t& size, Path& path, REQUEST_OPTIONS options) {
	string strPath;
	request = ParseRequest(request, size, strPath, options);
	path = move(strPath);
	return request; // returns query!
}


UInt32 URL::ParseQuery(const char* query, size_t size, const ForEachParameter& forEach) {
	Int32 countPairs(0);
	string name,value;
	bool isName(true);

	String::ForEachDecodedChar forEachDecoded([&isName, &countPairs, &name, &value, &forEach](char c, bool wasEncoded) {

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
			if (c == '=' && isName) {
				isName = false;
				return true;
			}
			// not considerate '+' char to replace by ' ', because it affects Base64 value in query which includes '+',
			// a space must be in a legal %20 format!
		}
		if (isName) {
			if(countPairs || c != '?') // ignore first '?'!
				name += c;
		} else
			value += c; 
		return true;
	});

	if (String::FromURI(query, size, forEachDecoded) && countPairs>=0) {
		// for the last pairs just if there was some decoded bytes
		++countPairs;
		forEach(name, isName ? NULL : value.c_str());
	}

	return abs(countPairs);
}


Parameters& URL::ParseQuery(const char* query, size_t size, Parameters& parameters) {
	ForEachParameter forEach([&parameters](const string& key, const char* value) {
		parameters.setParameter(key, value ? value : "");
		return true;
	});
	ParseQuery(query, size, forEach);
	return parameters;
}

} // namespace Mona
