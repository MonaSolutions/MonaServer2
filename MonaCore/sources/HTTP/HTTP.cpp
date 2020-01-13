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
#include "Mona/HTTP/HTTPSender.h"
#include "Mona/URL.h"
#include <algorithm>


using namespace std;


namespace Mona {

HTTP::Header::Header(const Socket& socket, bool rendezVous) : accessControlRequestMethod(0),
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
	host(socket.address()),
	range(NULL),
	chunked(false),
	code(NULL),
	rendezVous(rendezVous) {
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
		String::ForEach forEach([this](UInt32 index, const char* value) {
			if (String::ICompare(value, "chunked") == 0)
				chunked = true;
			return true;
		});
		String::Split(value, ",", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);
	} else if (String::ICompare(key, "if-modified-since") == 0) {
		Exception ex;
		AUTO_ERROR(ifModifiedSince.update(ex, value, Date::FORMAT_HTTP), "HTTP header")
	} else if (String::ICompare(key, "access-control-request-headers") == 0) {
			accessControlRequestHeaders = value;
	} else if (String::ICompare(key, "access-control-request-method") == 0) {

		String::ForEach forEach([this](UInt32 index, const char* value) {
			Type type(ParseType(value, rendezVous));
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
			// trim key right
			while (*value && *value != '=' && !isblank(*value))
				++value;
			const char *endKey = value;
			// trim value left
			while (*value && isblank(*++value));
			String::Scoped scoped(endKey);
			setString(data, value); // cookies in Map!
			return true;
		});
		String::Split(value, ";", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);
	}
	
}

const char* HTTP::ErrorToCode(Int32 error) {
	if (!error)
		return NULL;
	switch (error) {
		case Session::ERROR_APPLICATION:
			return HTTP_CODE_424; // Method failure
		case Session::ERROR_UPDATE:
			return HTTP_CODE_205;
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
			return HTTP_CODE_500; // Internal server error
		case Session::ERROR_UNSUPPORTED:
			return HTTP_CODE_501; // Not Implemented
		case Session::ERROR_UNFOUND:
			return HTTP_CODE_404; // Not Found
		default: // User close!
			return NULL; // could be HTTP_CODE_406 = Not Acceptable
	}
}

HTTP::Type HTTP::ParseType(const char* value, bool withRDV) {
	switch (strlen(value)) {
		case 3:
			if (withRDV && String::ICompare(value, "RDV") == 0)
				return TYPE_RDV;
			if(String::ICompare(value, "GET") == 0)
				return TYPE_GET;
			if (String::ICompare(value, "PUT") == 0)
				return TYPE_PUT;
			break;
		case 4:
			if (String::ICompare(value, "HEAD") == 0)
				return TYPE_HEAD;
			if (String::ICompare(value, "POST") == 0)
				return TYPE_POST;
			break;
		case 6:
			if (String::ICompare(value, "DELETE") == 0)
				return TYPE_DELETE;
			break;
		case 7:
			if (String::ICompare(value, "OPTIONS") == 0)
				return TYPE_OPTIONS;
			break;
		default:break;
	}
	return TYPE_UNKNOWN;
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

	String::Append(writer, "<tr><td><a href=\"", entry.name(), entry.isFolder() ? "/\">" : "\">", entry.name(), entry.isFolder() ? "/" : "");
	String::Append(writer, "</a></td><td>&nbsp;", String::Date(entry.lastChange(), "%d-%b-%Y %H:%M"), "</td><td align=right>&nbsp;&nbsp;", size, "</td></tr>\n");
}

bool HTTP::WriteDirectoryEntries(Exception& ex, BinaryWriter& writer, const std::string& fullPath, const std::string& path, SortBy sortBy, Sort sort) {

	vector<Path*> paths;
	FileSystem::ForEach forEach([&paths](const string& path, UInt16 level){
		paths.emplace_back(new Path(path));
		return true;
	});
	if(FileSystem::ListFiles(ex, fullPath, forEach)<0)
		return false;

	char ord[] = "D";
	if (sort==SORT_DESC)
		ord[0] = 'A';

	// Write column names
	// Name		Modified	Size
	String::Append(writer, "<!DOCTYPE html><html><head><title>Index of ", path.empty() ? "/" : path, "</title><style>th {text-align: center;}</style></head><body><h1>Index of ", path.empty() ? "/" : path, "</h1><pre><table cellpadding=\"0\"><tr><th><a href=\"?N=");
	String::Append(writer, ord, "\">Name</a></th><th><a href=\"?M=", ord, "\">Modified</a></th><th><a href=\"?S=", ord, "\">Size</a></th></tr><tr><td colspan=\"3\"><hr></td></tr>");

	// Write first entry - link to a parent directory
	if (!path.empty() && path[0]!='/' && path[0] != '\\')
		String::Append(writer, "<tr><td><a href=\"..\">Parent directory</a></td><td>&nbsp;-</td><td>&nbsp;&nbsp;-</td></tr>\n");

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

//////// RENDEZ VOUS ////////////


HTTP::RendezVous::RendezVous(const Timer& timer) : _timer(timer),
	_onTimer([this](UInt32)->UInt32 {
		// remove obsolete handshakes
		lock_guard<mutex> lock(_mutex);
		auto it = _remotes.begin();
		while (it != _remotes.end()) {
			if (it->second->expired())
				it = _remotes.erase(it);
			else
				++it;
		}
		return 10000;
	}) {
	_timer.set(_onTimer, 10000);
}

HTTP::RendezVous::~RendezVous() {
	_timer.set(_onTimer, 0);
}

bool HTTP::RendezVous::meet(shared<Header>& pHeader, const Packet& packet, const shared<Socket>& pSocket) {
	// read join and resolve parameters
	bool join = false;
	const char* to = NULL;
	URL::ForEachParameter forEach([&join, &to, &pHeader](string& key, const char* value) {
		if (String::ICompare(key, "join") == 0)
			join = !value || !String::IsFalse(value);
		else if (String::ICompare(key, "from") == 0)
			pHeader->upgrade = pHeader->setString(key, value).c_str();
		else if (String::ICompare(key, "to") == 0)
			to = pHeader->setString(key, value).c_str();
		return true;
	});
	URL::ParseQuery(pHeader->query, forEach);

	// meet
	{ // lock
		unique_lock<mutex> lock(_mutex);
		auto it = _remotes.lower_bound(pHeader->path.c_str());
		if (it != _remotes.end() && String::ICompare(it->first, pHeader->path) == 0) {
			// found!
			shared<Socket> pRemoteSocket = it->second->lock();
			if (pRemoteSocket) {
				if (!to || String::ICompare(pHeader->code = to, it->second->from) == 0) {
					const Remote* pRemote = it->second.get();
					if (!join || packet) { // no release RDV if join without packet!
						it->second.release();
						_remotes.erase(it);
						join = false;
					} // else to == NULL!
					lock.unlock(); // unlock

					Request local(Path::Null(), pHeader, packet, true); // pHeader captured!
					// Send remote to local (request)
					send(pSocket, local, *pRemote, pRemoteSocket->peerAddress());
					if (!join) {
						// Send local to remote (response)
						send(pRemoteSocket, *pRemote, local, pSocket->peerAddress());
						delete pRemote;
					}
					return true;
				}
				// response match a previous request => 410
				join = false;
			} else
				it = _remotes.erase(it); // peer not found!
		}
		if (!join && !to) {
			unique<Remote> pRemote(SET, pHeader, packet, pSocket);
			_remotes.emplace_hint(it, SET, forward_as_tuple((*pRemote)->path.c_str()), forward_as_tuple(move(pRemote)));
			return true;
		} // else peer not found => 410
	} // unlock
	HTTPSender("RDVSender", pHeader, pSocket).sendError(HTTP_CODE_410);
	pHeader.reset();
	return false;
}

void HTTP::RendezVous::send(const shared<Socket>& pSocket, const shared<const Header>& pHeader, const Request& message, const SocketAddress& from) {
	// Send local to remote
	HTTPSender sender("RDVSender", pHeader, pSocket);
	HTTP_BEGIN_HEADER(sender.buffer())
		HTTP_ADD_HEADER("Access-Control-Expose-Headers", "from")
		const char* id = message->getString("from");
		if(id)
			HTTP_ADD_HEADER("from", from, ", ", id)
		else
			HTTP_ADD_HEADER("from", from)
	HTTP_END_HEADER
	if (!pHeader->code) {
		sender.send(HTTP_CODE_200, message->mime, message->subMime, message.size());
		sender.send(message);
	} else
		sender.send(HTTP_CODE_200);
}


} // namespace Mona
