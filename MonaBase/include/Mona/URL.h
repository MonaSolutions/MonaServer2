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
#include "Mona/Path.h"
#include "Mona/Parameters.h"

namespace Mona {


typedef UInt8 REQUEST_OPTIONS;
enum {
	REQUEST_MAKE_FOLDER = 1, /// ignore empty tokens
	REQUEST_FORCE_RELATIVE = 2,  /// remove leading and trailing whitespace from tokens
};

/*!
URL parser,
to write use directly String like that => 
String(protocol, "://", address, "/", String::URI(path), '?', String::URI(query)); */
struct URL : virtual Static {
	/*!
	Parse URL, assign protocol and address and return request ("/request" if aboluste) */
	static const char* Parse(const std::string& url, std::string& protocol, std::string& address) { std::size_t size(url.size()); return Parse(url.data(), size, protocol, address); }
	static const char* Parse(const char* url, std::string& protocol, std::string& address) { std::size_t size(std::string::npos); return Parse(url, size, protocol, address); }
	static const char* Parse(const char* url, std::size_t& size, std::string& protocol, std::string& address);
	/*!
	Parse URL, assign address and return request */
	static const char* Parse(const std::string& url, std::string& address) { std::size_t size(url.size()); return Parse(url.data(), size, address, address); }
	static const char* Parse(const char* url, std::string& address) { std::size_t size(std::string::npos); return Parse(url, size, address, address); }
	static const char* Parse(const char* url, std::size_t& size, std::string& address) { return Parse(url, size, address, address); }
	
	/*!
	Parse Request, assign path and return query */
	static const char* ParseRequest(const std::string& request, Path& path, REQUEST_OPTIONS options=0) { std::size_t size(request.size()); return ParseRequest(request.data(), size, path, options); }
	static const char* ParseRequest(const char* request, Path& path, REQUEST_OPTIONS options = 0) { std::size_t size(std::string::npos); return ParseRequest(request, size, path, options); }
	static const char* ParseRequest(const char* request, std::size_t& size, Path& path, REQUEST_OPTIONS options = 0);

	static const char* ParseRequest(const std::string& request, std::string& path, REQUEST_OPTIONS options = 0) { std::size_t size(request.size()); return ParseRequest(request.data(), size, path, options); }
	static const char* ParseRequest(const char* request, std::string& path, REQUEST_OPTIONS options = 0) { std::size_t size(std::string::npos); return ParseRequest(request, size, path, options); }
	static const char* ParseRequest(const char* request, std::size_t& size, std::string& path, REQUEST_OPTIONS options = 0);

	typedef std::function<bool(std::string&, const char*)> ForEachParameter;
	static UInt32 ParseQuery(const std::string& query, const ForEachParameter& forEach) { return ParseQuery(query.data(), query.size(), forEach); }
	static UInt32 ParseQuery(const char* query, const ForEachParameter& forEach) { return ParseQuery(query, std::string::npos, forEach); }
	static UInt32 ParseQuery(const char* query, std::size_t size, const ForEachParameter& forEach);

	static Parameters& ParseQuery(const std::string& query, Parameters& parameters) { return ParseQuery(query.data(), query.size(), parameters); }
	static Parameters& ParseQuery(const char* query, Parameters& parameters) { return ParseQuery(query, std::string::npos, parameters); }
	static Parameters& ParseQuery(const char* query, std::size_t size, Parameters& parameters);

};



} // namespace Mona
