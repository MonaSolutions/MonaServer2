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

#pragma once

#include "Mona/Mona.h"
#include "Mona/WS/WSDecoder.h"

namespace Mona {


#define HTML_BEGIN_COMMON_RESPONSE(WRITER,TITLE) \
	{ decltype(WRITER)& __writer(WRITER); String::Append(__writer, "<!DOCTYPE html><html><head><title>", TITLE, "</title></head><body><h1>", TITLE, "</h1><p>");

#define HTML_END_COMMON_RESPONSE(SERVER_ADDRESS) \
	String::Append(__writer, "</p><hr><address>Mona Server at ", SERVER_ADDRESS, "</address></body></html>"); }

#define HTTP_LIVE_HEADER	"Cache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache"

#define HTTP_BEGIN_HEADER(WRITER)  { decltype(WRITER)& __writer(WRITER); __writer.resize(__writer.size()-2);
#define HTTP_ADD_HEADER(NAME, ...) { String::Append(__writer, NAME, ": ", __VA_ARGS__, "\r\n"); }
#define HTTP_ADD_HEADER_LINE(...) { String::Append(__writer, __VA_ARGS__, "\r\n"); }
#define HTTP_END_HEADER  String::Append(__writer, "\r\n");  }

#define HTTP_CODE_100	"100 Continue"
#define HTTP_CODE_101	"101 Switching Protocols"
#define HTTP_CODE_102	"102 Processing"
#define HTTP_CODE_118	"118 Connection timed out"
#define HTTP_CODE_200	"200 OK"
#define HTTP_CODE_201	"201 Created"
#define HTTP_CODE_202	"202 Accepted"
#define HTTP_CODE_203	"203 Non-Authoritative Information"
#define HTTP_CODE_204	"204 No Content"
#define HTTP_CODE_205	"205 Reset Content"
#define HTTP_CODE_206	"206 Partial Content"
#define HTTP_CODE_207	"207 Multi-Status"
#define HTTP_CODE_210	"210 Content Different"
#define HTTP_CODE_226	"226 IM Used"

#define HTTP_CODE_300	"300 Multiple Choices"
#define HTTP_CODE_301	"301 Moved Permanently"
#define HTTP_CODE_302	"302 Moved Temporarily"
#define HTTP_CODE_303	"303 See Other"
#define HTTP_CODE_304	"304 Not Modified"
#define HTTP_CODE_305	"305 Use Proxy"
#define HTTP_CODE_307	"307 Temporary Redirect"
#define HTTP_CODE_310	"310 Too many Redirects"

#define HTTP_CODE_400	"400 Bad Request"
#define HTTP_CODE_401	"401 Unauthorized"
#define HTTP_CODE_402	"402 Payment Required"
#define HTTP_CODE_403	"403 Forbidden"
#define HTTP_CODE_404	"404 Not Found"
#define HTTP_CODE_405	"405 Method Not Allowed"
#define HTTP_CODE_406	"406 Not Acceptable"
#define HTTP_CODE_407	"407 Proxy Authentication Required"
#define HTTP_CODE_408	"408 Request Time-out"
#define HTTP_CODE_409	"409 Conflict"
#define HTTP_CODE_410	"410 Gone"
#define HTTP_CODE_411	"411 Length Required"
#define HTTP_CODE_412	"412 Precondition Failed"
#define HTTP_CODE_413	"413 Request Entity Too Large"
#define HTTP_CODE_414	"414 Request-URI Too Long"
#define HTTP_CODE_415	"415 Unsupported Media Type"
#define HTTP_CODE_416	"416 Requested range unsatisfiable"
#define HTTP_CODE_417	"417 Expectation failed"
#define HTTP_CODE_418	"418 I'm a teapot"
#define HTTP_CODE_422	"422 Unprocessable entity"
#define HTTP_CODE_423	"423 Locked"
#define HTTP_CODE_424	"424 Method failure"
#define HTTP_CODE_425	"425 Unordered Collection"
#define HTTP_CODE_426	"426 Upgrade Required"
#define HTTP_CODE_428	"428 Precondition Required"
#define HTTP_CODE_429	"429 Too Many Requests"
#define HTTP_CODE_431	"431 Request Header Fields Too Large"
#define HTTP_CODE_449	"449 Retry With"
#define HTTP_CODE_450	"450 Blocked by Windows Parental Controls"
#define HTTP_CODE_456	"456 Unrecoverable Error"
#define HTTP_CODE_499	"499 client has closed connection"

#define HTTP_CODE_500	"500 Internal Server Error"
#define HTTP_CODE_501	"501 Not Implemented"
#define HTTP_CODE_502	"502 Proxy Error"
#define HTTP_CODE_503	"503 Service Unavailable"
#define HTTP_CODE_504	"504 Gateway Time-out"
#define HTTP_CODE_505	"505 HTTP Version not supported"
#define HTTP_CODE_506	"506 Variant also negociate"
#define HTTP_CODE_507	"507 Insufficient storage"
#define HTTP_CODE_508	"508 Loop detected"
#define HTTP_CODE_509	"509 Bandwidth Limit Exceeded"
#define HTTP_CODE_510	"510 Not extended"
#define HTTP_CODE_520	"520 Web server is returning an unknown error"


struct HTTP : virtual Static {

	enum Type { // with values to allow to check types with accessControlRequestMethod
		TYPE_UNKNOWN = 0,
		TYPE_HEAD = 1,
		TYPE_GET = 2,
		TYPE_PUT = 4,
		TYPE_OPTIONS = 8,
		TYPE_POST = 16,
		TYPE_DELETE = 32,
		TYPE_RDV = 8192
	};
	static const char* TypeToString(Type type) {
		switch (type) {
			case TYPE_HEAD:		return "HEAD";
			case TYPE_GET:		return "GET";
			case TYPE_PUT:		return "PUT";
			case TYPE_OPTIONS:	return "OPTIONS";
			case TYPE_POST:		return "POST";
			case TYPE_DELETE:	return "DELETE";
			case TYPE_RDV:		return "RDV";
			default:;
		}
		return "UNKNOWN";
	}
	static const char* Types(bool withRDV=false) {
		if(withRDV)
			return "HEAD, GET, PUT, OPTIONS, POST, DELETE, RDV";
		return "HEAD, GET, PUT, OPTIONS, POST, DELETE";
	}
	

	enum Connection {
		CONNECTION_CLOSE = 0, // close by default, other values requires a connection staying opened (upgrade or keepalive)
		CONNECTION_UPGRADE = 1,
		CONNECTION_KEEPALIVE = 2,
	};

	enum Sort {
		SORT_ASC,
		SORT_DESC,
	};

	enum SortBy {
		SORTBY_NAME,
		SORTBY_MODIFIED,
		SORTBY_SIZE
	};

	static const char*	 ErrorToCode(Int32 error);

	static Type			 ParseType(const char* value, bool withRDV=false);
	static UInt8		 ParseConnection(const char* value);

	static bool			 WriteDirectoryEntries(Exception& ex, BinaryWriter& writer, const std::string& fullPath, const std::string& path, SortBy sortBy = SORTBY_NAME, Sort sort = SORT_ASC);

	struct Header : Parameters, virtual Object {
		Header(const Socket& socket, bool rendezVous=false);

		MIME::Type		mime;
		const char*		subMime;

		HTTP::Type		type;
		Path			path;
		std::string		query;
		std::string		host;
		float			version;
		const char*		origin;
		const char*		range; // TODO supports it?

		const char*		code;
		UInt8			connection;
		const char*		upgrade;
		UInt8			cacheControl;
		bool			chunked;
		bool			progressive;

		Date			ifModifiedSince;

		const char*		secWebsocketKey;
		const char*		secWebsocketAccept;

		UInt8			accessControlRequestMethod;
		const char*		accessControlRequestHeaders;

		bool			rendezVous;

		shared<WSDecoder>	pWSDecoder;

		void			set(const char* key, const char* value);
	};

	
	struct Message : Packet, Parameters, virtual Object {
		NULLABLE(!_pHeader)

		const Exception			ex;
		Media::Base*			pMedia;
		UInt32					lost;
		const bool				flush;
	
		const Header* operator->() const { return _pHeader.get(); }
		const Header& operator*() const { return *_pHeader; }
		operator const shared<const Header>&() const { return _pHeader; }
		bool unique() const { return _pHeader.unique(); }
	protected:
		Message(shared<Header>& pHeader, const Packet& packet, bool flush) : lost(0), pMedia(NULL), flush(flush), Packet(std::move(packet)), _pHeader(std::move(pHeader)) { if (_pHeader) setParams(*_pHeader); }
		/*!
		exception */
		Message(shared<Header>& pHeader, const Exception& ex) : lost(0), pMedia(NULL), ex(ex), flush(true) { pHeader.reset(); }
		/*!
		media packet */
		Message(shared<Header>& pHeader, Media::Base* pMedia) : lost(0), pMedia(pMedia), _pHeader(std::move(pHeader)), flush(false) { if (_pHeader) setParams(*_pHeader); }
		/*!
		media lost infos */
		Message(shared<Header>& pHeader, Media::Type type, UInt32 lost, UInt8 track = 0) : lost(lost), pMedia(new Media::Base(type, Packet::Null(), track)), _pHeader(std::move(pHeader)), flush(false) { if (_pHeader) setParams(*_pHeader); }
		/*!
		media reset, flush or publish end */
		Message(shared<Header>& pHeader, bool endMedia, bool flush) : lost(0), pMedia(endMedia ? NULL : new Media::Base()), _pHeader(std::move(pHeader)), flush(flush) { if (_pHeader) setParams(*_pHeader); }

		~Message() { if (pMedia) delete pMedia; }
	private:
		shared<const Header> _pHeader;
	};

	struct Request : Message, virtual Object {
		Request(const Path& file, shared<Header>& pHeader, const Packet& packet, bool flush) : file(file), Message(pHeader, packet, flush) {}
		/*!
		exception */
		Request(const Path& file, shared<Header>& pHeader, const Exception& ex) : file(file), Message(pHeader, ex) {}
		/*!
		media packet */
		Request(const Path& file, shared<Header>& pHeader, Media::Base* pMedia) : file(file), Message(pHeader, pMedia) {}
		/*!
		media lost infos */
		Request(const Path& file, shared<Header>& pHeader, Media::Type type, UInt32 lost, UInt8 track = 0) : file(file), Message(pHeader, type, lost, track) {}
		/*!
		media reset, flush or publish end */
		Request(const Path& file, shared<Header>& pHeader, bool endMedia, bool flush) : file(file), Message(pHeader, endMedia, flush) {}
		
		Path	file;
	};

	struct Response : Message, virtual Object {
		Response(UInt16 code, shared<Header>& pHeader, const Packet& packet, bool flush) : code(code), Message(pHeader, packet, flush) {}
		/*!
		exception */
		Response(UInt16 code, shared<Header>& pHeader, const Exception& ex) : code(code), Message(pHeader, ex) {}
		/*!
		media packet */
		Response(UInt16 code, shared<Header>& pHeader, Media::Base* pMedia) : code(code), Message(pHeader, pMedia) {}
		/*!
		media lost infos */
		Response(UInt16 code, shared<Header>& pHeader, Media::Type type, UInt32 lost, UInt8 track = 0) : code(code), Message(pHeader, type, lost, track) {}
		/*!
		media reset, flush or publish end */
		Response(UInt16 code, shared<Header>& pHeader, bool endMedia, bool flush) : code(code), Message(pHeader, endMedia, flush) {}

		UInt16 code;
	};

	struct RendezVous : virtual Object {
		RendezVous(const Timer&	timer);
		~RendezVous();

		bool meet(shared<Header>& pHeader, const Packet& packet, const shared<Socket>& pSocket);

	private:
		void send(const shared<Socket>& pSocket, const shared<const Header>& pHeader, const Request& message, const SocketAddress& from);
		struct Remote : Request, weak<Socket>, virtual Object {
			Remote(shared<Header>& pHeader, const Packet& packet, const shared<Socket>& pSocket) : from(self->upgrade),
				weak<Socket>(pSocket), Request(Path::Null(), pHeader, std::move(packet), true) {}
			const char* from;
		};
		struct Comparator {
			bool operator()(const char* path1, const char* path2) const { return String::ICompare(path1, path2)<0; }
		};
		std::mutex														_mutex;
		std::map<const char*, unique<const Remote>, Comparator>			_remotes;
		Timer::OnTimer												    _onTimer;
		const Timer&													_timer;
	};
};




} // namespace Mona
