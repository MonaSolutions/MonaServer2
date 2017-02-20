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
	{ BinaryWriter& __writer(WRITER); __writer.write(EXPAND("<!DOCTYPE html><html><head><title>")).write(TITLE).write(EXPAND("</title></head><body><h1>")).write(TITLE).write(EXPAND("</h1><p>"));

#define HTML_END_COMMON_RESPONSE(SERVER_ADDRESS) \
	__writer.write(EXPAND("</p><hr><address>Mona Server at ")).write(SERVER_ADDRESS).write(EXPAND("</address></body></html>")); }

#define HTTP_BEGIN_HEADER(WRITER)  { BinaryWriter& __writer(WRITER); __writer.buffer().resize(__writer.buffer().size()-2);
#define HTTP_ADD_HEADER(NAME, ...) { String::Append(__writer, ": ", __VA_ARGS__, "\r\n"); }
#define HTTP_END_HEADER  __writer.write("\r\n");  }

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

	enum Type {
		TYPE_UNKNOWN = 0,
		TYPE_HEAD=1,
		TYPE_GET = 2,
		TYPE_PUT = 4,
		TYPE_OPTIONS = 8,
		TYPE_POST = 16,
		TYPE_DELETE = 32
	};

	enum Connection {
		CONNECTION_ABSENT = 0,
		CONNECTION_CLOSE = 1,
		CONNECTION_UPGRADE = 2,
		CONNECTION_KEEPALIVE = 4,
		CONNECTION_UPDATE = 8
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

	static bool Send(Socket& socket, const Packet& packet);

	static Type			 ParseType(const char* value, std::size_t size);
	static UInt8		 ParseConnection(const char* value);

	static bool			 WriteDirectoryEntries(Exception& ex, BinaryWriter& writer, const std::string& fullPath, const std::string& path, SortBy sortBy = SORTBY_NAME, Sort sort = SORT_ASC);

	typedef std::function<void(const char* key, const char* data, UInt32 size)> OnCookie;
	static bool			 WriteSetCookie(DataReader& reader, Buffer& buffer, const OnCookie& onCookie=nullptr);

	struct Header : Parameters, virtual Object {
		Header(const char* protocol, const SocketAddress& serverAddress);

		const char* protocol;

		MIME::Type		mime;
		const char*		subMime;

		HTTP::Type		type;
		std::string		path;
		std::string		query;
		std::string		serverAddress;
		float			version;
		const char*		origin;

		UInt8			connection;
		const char*		upgrade;
		UInt8			cacheControl;

		Date			ifModifiedSince;

		const char*		secWebsocketKey;
		const char*		secWebsocketAccept;

		UInt8			accessControlRequestMethod;
		const char*		accessControlRequestHeaders;

		shared<WSDecoder>	pWSDecoder;

		void			set(const char* key, const char* value);
	};


	struct Request : Packet, Parameters, virtual Object {
		Request(shared<Header>& pHeader, const Exception& ex) : lost(0), pMedia(NULL), ex(ex), flush(true), _pHeader(pHeader) { init(pHeader); }
		Request(shared<Header>& pHeader, const Path& file, const Packet& packet, bool flush) : file(std::move(file)), lost(0), pMedia(NULL), flush(flush), Packet(std::move(packet)), _pHeader(pHeader) { init(pHeader); }
		/*!
		Post media packet */
		Request(shared<Header>& pHeader, UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) : lost(0), pMedia(new Media::Audio(track, tag, packet)), _pHeader(pHeader), flush(false) { init(pHeader); }
		Request(shared<Header>& pHeader, UInt16 track, const Media::Video::Tag& tag, const Packet& packet) : lost(0), pMedia(new Media::Video(track, tag, packet)), _pHeader(pHeader), flush(false) { init(pHeader); }
		Request(shared<Header>& pHeader, UInt16 track, Media::Data::Type type, const Packet& packet) : lost(0), pMedia(new Media::Data(track, type, packet)), _pHeader(pHeader), flush(false) { init(pHeader); }
		Request(shared<Header>& pHeader, UInt16 track, DataReader& reader) : lost(0), pMedia(new Media::Data(track, reader)), _pHeader(pHeader), flush(false) { init(pHeader); }
		/*!
		Post media lost infos */
		Request(shared<Header>& pHeader, Media::Type type, UInt32 lost) : lost(-Int32(lost)), pMedia(new Media::Base(type, 0)), _pHeader(pHeader), flush(false) { init(pHeader); }
		Request(shared<Header>& pHeader, Media::Type type, UInt16 track, UInt32 lost) : lost(lost), pMedia(new Media::Base(type, track)), _pHeader(pHeader), flush(false) { init(pHeader); }
		/*!
		Post reset */
		Request(shared<Header>& pHeader) : lost(1), pMedia(NULL), _pHeader(pHeader), flush(false) { init(pHeader); }
		/*!
		Post flush or publish end */
		Request(shared<Header>& pHeader, bool end) : lost(end ? 1 : 0), pMedia(NULL), _pHeader(pHeader), flush(true) { init(pHeader); }
		/*!
		Put */
		Request(shared<Header>& pHeader, const Packet& packet) : lost(0), pMedia(NULL), _pHeader(pHeader), flush(true), Packet(std::move(packet)) { init(pHeader); }

		~Request() { if (pMedia) delete pMedia; }

		const Exception			ex;
		Media::Base*			pMedia;
		Int32					lost;
		const bool				flush;
		Path					file;
		SocketAddress			serverAddress;

		const Header* operator->() const { return _pHeader.get(); }
		const Header& operator*() const { return *_pHeader; }
		operator const shared<const Header>&() const { return _pHeader; }
		operator bool() const { return _pHeader.operator bool(); }
	private:
		void init(shared<Header>& pHeader);
		shared<const Header> _pHeader;
	};
};




} // namespace Mona
