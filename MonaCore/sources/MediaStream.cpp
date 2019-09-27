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

#include "Mona/MediaStream.h"
#include "Mona/MediaSocket.h"
#include "Mona/MediaFile.h"
#include "Mona/MediaServer.h"

using namespace std;


namespace Mona {

MediaStream::MediaStream(Type type, const Path& path, Media::Source& source) : _firstStart(true),
	_starting(false), _running(false), targets(_targets), _startCount(0),
	type(type), path(path), source(source) {
}

MediaStream::~MediaStream() {
	onDelete();
	if (_running || _starting)
		CRITIC("MediaStream deleting without be stopped before by child class");
}

void MediaStream::start(const Parameters& parameters) {
	ex = nullptr; // reset lastEx on pullse start!
	if (_running && !_starting)
		return; // nothing todo, starting already done!
	if (_firstStart) {
		_firstStart = false;
		// This is a target, with beginMedia it can be start, so create a _pTarget and keep it alive all the life-time of this Stream
		// Indeed it can be stopped and restarted with beginMedia!
		Media::Target* pTarget = dynamic_cast<Media::Target*>(this);
		if (pTarget)
			onNewTarget(_pTarget = shared<Media::Target>(make_shared<Media::Target>(), pTarget)); // aliasing	
	}
	_starting = true;
	params.onUnfound = [this, &parameters](const string& key)->const string* {
		const string* pValue = parameters.getParameter(key);
		if (!pValue)
			return NULL;
		params.setParameter(key, *pValue); // set value in params!
		return pValue;
	};
	if (starting(params))
		finalizeStart();
	params.onUnfound = NULL;
	if (_starting) // _starting can switch to false if finalizeStart (then _running is already set to true) and on stop (then _running must stay on false)
		_running = true;
}

bool MediaStream::finalizeStart() {
	if (!_starting)
		return false;
	if (!_running) // called while starting!
		_running = true;
	if (onStart && !onStart()) {
		stop();
		return false;
	}
	_starting = false;
	++_startCount;
	INFO(description(), " starts");
	return true;
}
void MediaStream::stop() {
	if (!_running && !_starting)
		return;
	stopping();
	_running = false;
	for (const auto& it : _targets)
		(MediaStream::OnStop&)it->onStop = nullptr; // unsuscribe because children pTarget can continue to live alone!
	_targets.clear(); // to invalid targets!
	if (_starting) {
		_starting = false;
		return;
	}
	INFO(description(), " stops");
	onStop(); // in last to allow possibily a delete this (beware impossible with Subscription usage!)
}
shared<const Socket> MediaStream::socket() const {
	if (type>0)
		WARN(typeof(self), " should implement socket()");
	return nullptr;
}
shared<const File> MediaStream::file() const {
	if (type == TYPE_FILE)
		WARN(typeof(self), " should implement file()");
	return nullptr;
}

shared<Socket> MediaStream::newSocket(const Parameters& parameters, const shared<TLS>& pTLS) {
	if (type <= 0)
		return nullptr;
	shared<Socket> pSocket;
	switch (type) {
		case TYPE_SRT:
#if defined(SRT_API)
			pSocket.set<SRT::Socket>();
			break;
#else
			ERROR(description(), "SRT unsupported replacing by UDP (build MonaBase with SRT support before)");
#endif
		default:
			if (pTLS)
				pSocket.set<TLS::Socket>(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM, pTLS);
			else
				pSocket.set(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM);
	}
	Exception ex;
	AUTO_WARN(pSocket->processParams(ex, parameters, "stream."), description());
	return pSocket;
}

unique<MediaStream> MediaStream::New(Exception& ex, Media::Source& source, const string& description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS) {
	// Net => [address] [type/TLS][/MediaFormat] [parameter]
	// File = > file[.format][MediaFormat][parameter]
	
	const char* line = String::TrimLeft(description.c_str());

	// isolate first + capture path and query
	Path path;
	const char* first = line;
	const char* query = NULL;
	size_t size = 0;
	while (*line && !isblank(*line)) {
		switch (*line) {
			case '?': // query
				if(!query)
					query = line;
			case '/':
			case '\\': // path
				if(!size)
					size = line - first;
				break;
			default:;
		}
		++line;
	}
	// query => parameters
	Parameters params;
	if (query)
		Util::UnpackQuery(query, line - query, params);
	if (size) {
		if (first[size] != '?') // size stop on '/', else no path!
			path.set(String::Data(first + size, query - first - size));
	} else
		size = line - first;

	Type type(TYPE_FILE);
	bool isSecure(false);
	bool isFile(false);
	string format;
	String::ForEach forEach([&](UInt32 index, const char* value) {
		if (String::ICompare(value, "UDP") == 0) {
			isFile = false;
			type = TYPE_UDP;
			return true;
		}
		if (String::ICompare(value, "TCP") == 0) {
			isFile = false;
			if (type != TYPE_HTTP)
				type = TYPE_TCP;
			return true;
		}
		if (String::ICompare(value, "SRT") == 0) {
			isFile = false;
			type = TYPE_SRT;
			return true;
		}
		if (String::ICompare(value, "HTTP") == 0) {
			isFile = false;
			type = TYPE_HTTP;
			return true;
		}
		if (String::ICompare(value, "TLS") == 0) {
			isSecure = true;
			return true;
		}
		if (String::ICompare(value, "FILE") == 0) {
			// force file!
			isFile = true;
			type = TYPE_FILE;
			return true;
		}
		format = value;
		return false;
	});

	const char* args = strpbrk(line = String::TrimLeft(line), " \t\r\n\v\f");
	String::Split(line, args ? (line - args) : string::npos, "/", forEach, SPLIT_IGNORE_EMPTY);

#if !defined(SRT_API)
	if (type == TYPE_SRT) {
		ex.set<Ex::Unsupported>(TypeToString(type), " stream not supported, build MonaBase with SRT support first");
		return nullptr;
	}
#endif

	bool isTarget(&source == &Media::Source::Null());
	SocketAddress address;
	UInt16 port;
	Int8 isAddress = 0; // -1 if explicit address to bind, 1 if explicit address
	

	switch (isFile) {
		case false: {
			// if is not explicitly a file, test if it's a port
			isAddress = *first == '@' ? -1 : 0;
			if (isAddress) {
				--size;
				if (*++first == ':') { // is just ":port"
					++first;
					--size;
				}
			}
			if (String::ToNumber(first, size, port)) {
				address.setPort(port);
				break;
			}
			// Test if it's an address
			String::Scoped scoped(first + size);
			Exception exc;
			if (address.set(exc, first)) {
				isAddress = 1; // if was -1 no pb, 
				break;
			}
			if (type) { // explicitly network protocol (not file) however address invalid!
				ex = exc;
				return nullptr;
			}
			isFile = true;
			type = TYPE_FILE;
		}
		default: // is a file!
			path.set(String::Data(first, size));
			if (path.isFolder()) {
				ex.set<Ex::Format>("Stream file ", path, " can't be a folder");
				return nullptr;
			}
			if (!path) {
				ex.set<Ex::Format>("No file name in stream file description");
				return nullptr;
			}
	}

	// fix params!
	if (args) {
		while (isspace(*args) && *++args);
		if (*args) {
			// TODO add to parameters?!
		}
	}

	if (format.empty()) {
		switch (type) {
			case TYPE_SRT: // SRT and No Format => TS by default to catch Haivision usage
			case TYPE_UDP: // UDP and No Format => TS by default to catch with VLC => UDP = TS
				format = "mp2t";
				break;
			case TYPE_HTTP:
				if (path.isFolder()) {
					ex.set<Ex::Format>("A HTTP source or target stream can't be a folder");
					return nullptr;
				}
			default: {
				const char* subMime;
				if (MIME::Read(path, subMime)) {
					format = subMime;
					break;
				}
				if (!isTarget && type == TYPE_HTTP)
					break; // HTTP source allow empty format (will be determinate with content-type)
				if(path.extension().empty())
					ex.set<Ex::Format>(TypeToString(type), " stream description have to indicate a media format");
				else
					ex.set<Ex::Format>(TypeToString(type), " stream path has a format ", path.extension(), " unknown or not supported");
				return nullptr;
			}
		}
	}
	
	unique<MediaStream> pStream;
	if (isFile) {
		if (isTarget)
			pStream = MediaFile::Writer::New(path, format.c_str(), ioFile);
		else
			pStream = MediaFile::Reader::New(path, source, format.c_str(), timer, ioFile);
	} else {
		if (!type) // TCP by default excepting if format is RTP where rather UDP by default
			type = String::ICompare(format, "RTP") == 0 ? TYPE_UDP : TYPE_TCP;

		// KEEP this model of double creation to allow a day a new RTPWriter<...>(parameter)
		if (type != TYPE_UDP && (isAddress<0 || !address.host())) { // MediaServer, UDP is always a MediaSocket!
			if (isTarget)
				pStream = MediaServer::Writer::New(MediaServer::Type(type), path, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
			else
				pStream = MediaServer::Reader::New(MediaServer::Type(type), path, source, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
		} else if (isTarget) {
			if (!address.host() && isAddress) { // explicit 0.0.0.0 is an error here!
				// necessary UDP here (else give a MediaServer)
				ex.set<Ex::Net::Address::Ip>("Wildcard binding impossible for a stream ", (isTarget ? "target " : "source "), TypeToString(type));
				return nullptr;
			}
			pStream = MediaSocket::Writer::New(type, path, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
		} else
			pStream = MediaSocket::Reader::New(type, path, source, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
	}

	if (!pStream) {
		ex.set<Ex::Unsupported>(isTarget ? "Target stream " : "Source stream ", TypeToString(type), " format ", format, " not supported");
		return nullptr;
	}
	pStream->params.setParams(move(params));
	return pStream;
}

} // namespace Mona
