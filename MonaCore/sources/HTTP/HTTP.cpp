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

#include "Mona/HTTP/HTTP.h"
#include <algorithm>

using namespace std;


namespace Mona {

HTTP::Header::Header(const shared<Socket>& pSocket) : pSocket(pSocket), accessControlRequestMethod(0),
	mime(MIME::TYPE_UNKNOWN),
	type(TYPE_UNKNOWN),
	version(0),
	connection(CONNECTION_CLOSE),
	ifModifiedSince(0),
	subMime(NULL),
	origin(NULL),
	upgrade(NULL),
	secWebsocketKey(NULL),
	secWebsocketAccept(NULL),
	accessControlRequestHeaders(NULL),
	host(pSocket->address()),
	range(NULL),
	encoding(ENCODING_IDENTITY),
	code(NULL) {
}

void HTTP::Header::set(const char* key, const char* value) {
	value = setString(key, value).c_str();

	if (String::ICompare(key, "content-type") == 0) {
		mime = MIME::Read(value, subMime);
		if (!mime)
			WARN("Unknown Content-Type ", value);
	} else if (String::ICompare(key, "connection") == 0) {
		connection = ParseConnection(value);
	} else if (String::ICompare(key, "range") == 0) {
		range = strchr(value, '=');
		if (range)
			String::TrimLeft(++range);
	} else if (String::ICompare(key, "host") == 0) {
		host = value;
	} else if (String::ICompare(key, "origin") == 0) {
		origin = value;
	} else if (String::ICompare(key, "upgrade") == 0) {
		upgrade = value;
	} else if (String::ICompare(key, "sec-websocket-key") == 0) {
		secWebsocketKey = value;
	} else if (String::ICompare(key, "sec-webSocket-accept") == 0) {
		secWebsocketAccept = value;
	} else if (String::ICompare(key, "transfer-encoding") == 0) {
		encoding = ParseEncoding(value);
	} else if (String::ICompare(key, "if-modified-since") == 0) {
		Exception ex;
		AUTO_ERROR(ifModifiedSince.update(ex, value, Date::FORMAT_HTTP), "HTTP header")
	} else if (String::ICompare(key, "access-control-request-headers") == 0) {
		accessControlRequestHeaders = value;
	} else if (String::ICompare(key, "access-control-request-method") == 0) {

		String::ForEach forEach([this](UInt32 index, const char* value) {
			Type type(ParseType(value));
			if (!type)
				WARN("Access-control-request-method, unknown ", value, " type")
			else
				accessControlRequestMethod |= type;
			return true;
		});
		String::Split(value, ",", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);

	} else if (String::ICompare(key, "cookie") == 0) {
		String::ForEach forEach([this](UInt32 index, const char* data) {
			const char* value = data;
			// trim right
			while (value && *value != '=' && !isblank(*value))
				++value;
			if (value) {
				const char *endKey = value;
				// trim left
				do {
					++value;
				} while (value && (isblank(*value) || *value == '='));
				String::Scoped scoped(endKey);
				setString(data, value); // cookies in Map!
			}
			return true;
		});
		String::Split(value, ";", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);
	}
}

const char* HTTP::ErrorToCode(Int32 error) {
	if (!error)
		return NULL;
	switch (error) {
		case Session::ERROR_UNAVAILABLE:
		case Session::ERROR_SERVER:
			return HTTP_CODE_503; // Service unavailable
		case Session::ERROR_REJECTED:
			return HTTP_CODE_401; // Unauthorized
		case Session::ERROR_IDLE:
			return HTTP_CODE_408; // Request Time-out
		case Session::ERROR_PROTOCOL:
			return HTTP_CODE_400; // Bad Request
		case Session::ERROR_UNEXPECTED:
			return HTTP_CODE_417; // Expectation failed
		case Session::ERROR_UNSUPPORTED:
			return HTTP_CODE_501; // Not Implemented
		case Session::ERROR_UNFOUND:
			return HTTP_CODE_404; // Not Found
		default: // User close!
			return NULL; // could be HTTP_CODE_406 = Not Acceptable
	}
}

HTTP::Type HTTP::ParseType(const char* value) {
	switch (strlen(value)) {
		case 3:
			if(String::ICompare(value, EXPAND("GET")) == 0)
				return TYPE_GET;
			break;
		case 4:
			if (String::ICompare(value, EXPAND("PUT")) == 0)
				return TYPE_PUT;
			if (String::ICompare(value, EXPAND("HEAD")) == 0)
				return TYPE_HEAD;
			if (String::ICompare(value, EXPAND("POST")) == 0)
				return TYPE_POST;
			break;
		case 7:
			if (String::ICompare(value, EXPAND("OPTIONS")) == 0)
				return TYPE_OPTIONS;
			break;
		default:break;
	}
	return TYPE_UNKNOWN;
}

HTTP::Encoding HTTP::ParseEncoding(const char* value) {
	if (String::ICompare(value, EXPAND("chunked")) == 0)
		return ENCODING_CHUNKED;
	if (String::ICompare(value, EXPAND("compress")) == 0)
		return ENCODING_COMPRESS;
	if (String::ICompare(value, EXPAND("deflate")) == 0)
		return ENCODING_DEFLATE;
	if (String::ICompare(value, EXPAND("gzip")) == 0)
		return ENCODING_GZIP;
	return ENCODING_IDENTITY;
}

UInt8 HTTP::ParseConnection(const char* value) {
	UInt8 type(CONNECTION_CLOSE);
	String::ForEach forEach([&type](UInt32 index,const char* field){
		if (String::ICompare(field, "close") != 0) {
			if (String::ICompare(field, "keep-alive") == 0)
				type |= CONNECTION_KEEPALIVE;
			else if (String::ICompare(field, "upgrade") == 0)
				type |= CONNECTION_UPGRADE;
			else
				WARN("Unknown HTTP type connection ", field);
		}
		return true;
	});
	String::Split(value, ",", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);
	return type;
}

struct EntriesComparator {
	EntriesComparator(HTTP::SortBy sortBy, HTTP::Sort sort) : _sortBy(sortBy), _sort(sort) {}

	bool operator() (const Path* pPath1,const Path* pPath2) {
		if (_sortBy == HTTP::SORTBY_SIZE) {
			if (pPath1->size() != pPath2->size())
				return _sort ==HTTP::SORT_ASC ? pPath1->size() < pPath2->size() : pPath2->size() < pPath1->size();
		} else if (_sortBy == HTTP::SORTBY_MODIFIED) {
			if (pPath1->lastChange() != pPath2->lastChange())
				return _sort ==HTTP::SORT_ASC ? pPath1->lastChange() < pPath2->lastChange() : pPath2->lastChange() < pPath1->lastChange();
		}
		
		// NAME case
		if (pPath1->isFolder() && !pPath2->isFolder())
			return _sort ==HTTP::SORT_ASC;
		if (pPath2->isFolder() && !pPath1->isFolder())
			return _sort ==HTTP::SORT_DESC;
		int result = String::ICompare(pPath1->name(), pPath2->name());
		return _sort ==HTTP::SORT_ASC ? result<0 : result>0;
	}
private:
	HTTP::SortBy _sortBy;
	HTTP::Sort   _sort;
};


static void WriteDirectoryEntry(BinaryWriter& writer, const Path& entry) {
	string size;
	if (entry.isFolder())
		size.assign("-");
	else if (entry.size()<1024)
		String::Assign(size, entry.size());
	else if (entry.size() <	1048576)
		String::Assign(size, String::Format<double>("%.1fk", entry.size() / 1024.0));
	else if (entry.size() <	1073741824)
		String::Assign(size, String::Format<double>("%.1fM", entry.size() / 1048576.0));
	else
		String::Assign(size, String::Format<double>("%.1fG", entry.size() / 1073741824.0));

	writer.write(EXPAND("<tr><td><a href=\""))
		.write(entry.name()).write(entry.isFolder() ? "/\">" : "\">")
		.write(entry.name()).write(entry.isFolder() ? "/" : "").write(EXPAND("</a></td><td>&nbsp;"));
	String::Append(writer, String::Date(Date(entry.lastChange()), "%d-%b-%Y %H:%M"))
		.write(EXPAND("</td><td align=right>&nbsp;&nbsp;"))
		.write(size).write(EXPAND("</td></tr>\n"));
}

bool HTTP::WriteDirectoryEntries(Exception& ex, BinaryWriter& writer, const string& fullPath, const string& path, SortBy sortBy, Sort sort) {

	vector<Path*> paths;
	FileSystem::ForEach forEach([&paths](const string& path, UInt16 level){
		paths.emplace_back(new Path(path));
	});
	FileSystem::ListFiles(ex, fullPath, forEach);
	if (ex)
		return false;

	char ord[] = "D";
	if (sort==SORT_DESC)
		ord[0] = 'A';

	// Write column names
	// Name		Modified	Size
	writer.write(EXPAND("<!DOCTYPE html><html><head><title>Index of "))
		.write(path).write(EXPAND("/</title><style>th {text-align: center;}</style></head><body><h1>Index of "))
		.write(path).write(EXPAND("/</h1><pre><table cellpadding=\"0\"><tr><th><a href=\"?N="))
		.write(ord).write(EXPAND("\">Name</a></th><th><a href=\"?M="))
		.write(ord).write(EXPAND("\">Modified</a></th><th><a href=\"?S="))
		.write(ord).write(EXPAND("\">Size</a></th></tr><tr><td colspan=\"3\"><hr></td></tr>"));

	// Write first entry - link to a parent directory
	if(!path.empty())
		writer.write(EXPAND("<tr><td><a href=\"")).write(EXPAND("..\">Parent directory</a></td><td>&nbsp;-</td><td>&nbsp;&nbsp;-</td></tr>\n"));

	// Sort entries
	EntriesComparator comparator(sortBy,sort);
	std::sort(paths.begin(), paths.end(), comparator);

	// Write entries
	for (const Path* pPath : paths) {
		WriteDirectoryEntry(writer, *pPath);
		delete pPath;
	}

	// Write footer
	writer.write(EXPAND("</table></body></html>"));
	return true;
}

/// Cookie Writer class for writing Set-Cookie HTTP headers when client.properties(key, value, {expires=...}) is called
/// This implementation is based on RFC 6265 (but does not use 'Max-Age')
struct SetCookieWriter : DataWriter, virtual Object {
	SetCookieWriter(Buffer& buffer, const HTTP::OnCookie& onCookie) : _onCookie(onCookie),_option(VALUE),_buffer(buffer) {
		_buffer.append(EXPAND("\r\nSet-Cookie: "));
	}

	UInt64	beginObject(const char* type=NULL) { return 0; }
	void	endObject() {}
	UInt64	beginArray(UInt32 size) { return 0; }
	void	endArray() {}

	void	writePropertyName(const char* value) {
		if (String::ICompare("domain", value) == 0)
			_option = DOM;
		else if (String::ICompare("path", value) == 0)
			_option = PATH;
		else if (String::ICompare("expires", value) == 0)
			_option = EXPIRES;
		else if (String::ICompare("secure", value) == 0)
			_option = SECURE;
		else if (String::ICompare("httponly", value) == 0)
			_option = HTTPONLY;
		else
			WARN("Unexpected option : ", value)
	}

	UInt64	writeDate(const Date& date) {
		if ((_option <= 2 || date) && writeHeader())
			writeContent(String::Date(date, Date::FORMAT_RFC1123));
		_option = NO;
		return 0;
	}
	void	writeBoolean(bool value) {
		if ((_option <= 2 || value) && writeHeader()) {
			if(value)
				writeContent(EXPAND("true"));
			else
				writeContent(EXPAND("false"));
		}
		_option = NO;
	}
	void	writeNull() {
		if (_option <= 2 && writeHeader())
			writeContent(EXPAND("null"));
		_option = NO;
	}
	UInt64	writeBytes(const UInt8* data, UInt32 size) { writeString((const char*)data, size); return 0; }
	void	writeString(const char* value, UInt32 size) {
		if (_key.empty() && size && _option==VALUE) {
			_key.assign(value, size);
			String::Append(_buffer, _key, "=");
		} else if ((_option <= 2 || !String::IsFalse(value)) && writeHeader()) {
			writeContent(value, size);
			_option = NO;
		}
	}
	void	writeNumber(double value) {
		if ((_option <= 2 || value) && writeHeader()) {
			if (_option == EXPIRES)
				writeContent(String::Date(Date(Time::Now() + (Int64)(value * 1000), Timezone::GMT), Date::FORMAT_RFC1123));
			else
				writeContent(value);
		}
		_option = NO;
	}

private:

	bool writeHeader() {
		static const vector<const char*> Patterns({ "Domain=", "Path=", "Expires=", "Secure", "HttpOnly" });
		if (_option >= 0)
			String::Append(_buffer, "; ", Patterns[_option]);
		return _option<=2;
	}

	template <typename ...Args>
	void writeContent(Args&&... args) {
		UInt32 size = _buffer.size();
		String::Append(_buffer, args ...);
		if (_option == VALUE && _onCookie)
			_onCookie(_key.c_str(), STR _buffer.data()+size, _buffer.size()-size);
	}
	
	enum Option {
		NO = -2,
		VALUE=-1,
		DOM = 0,
		PATH = 1,
		EXPIRES = 2,
		SECURE = 3,
		HTTPONLY = 4
	};

	Buffer&				   _buffer;
	string				   _key;
	const HTTP::OnCookie&  _onCookie;
	Option				   _option;
};


bool HTTP::WriteSetCookie(DataReader& reader, Buffer& buffer, const OnCookie& onCookie) {
	if (reader.nextType() != DataReader::STRING)
		return false;
	UInt32 before = buffer.size();
	SetCookieWriter writer(buffer, onCookie);
	bool res = reader.read(writer)>1;
	if (!res)
		buffer.resize(before, true);
	return res;
}


} // namespace Mona
