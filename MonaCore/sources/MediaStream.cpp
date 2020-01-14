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
#include "Mona/URL.h"

using namespace std;


namespace Mona {

map<string, MediaStream::OnNewStream, String::IComparator> MediaStream::_OtherStreams;

MediaStream::~MediaStream() {
	if (_state)
		CRITIC("MediaStream deleting without be stopped before by child class");
	onDelete();
}

const Parameters& MediaStream::params() {
	if (_params.onUnfound) {
		// Keep assigned to something if was starting!
		_params.onUnfound = nullptr;
		_params.onUnfound = [](const string& key) { return nullptr; };
	}
	return _params;
}

bool MediaStream::start(const Parameters& parameters) {
	ex = nullptr; // reset lastEx on pullse start!
	if (_state==STATE_RUNNING)
		return true; // nothing todo
	if (_firstStart) {
		_firstStart = false;
		// This is a target, with beginMedia it can be start, so create a _pTarget and keep it alive all the life-time of this Stream
		// Indeed it can be stopped and restarted with beginMedia!
		Media::Target* pTarget = dynamic_cast<Media::Target*>(this);
		if (pTarget)
			onNewTarget(_pTarget = shared<Media::Target>(make_shared<Media::Target>(), pTarget)); // aliasing	
	}
	_params.onUnfound = nullptr; // If starting() has returned false, state is always STOPPED and we have not been able to reset onUnfound (this been maybe deleted)
	_params.onUnfound = [this, &parameters](const string& key)->const string* {
		const string* pValue = parameters.getParameter(key);
		if (!pValue)
			return NULL;
		_params.setParameter(key, *pValue); // set value in params!
		return pValue;
	};
	// first call _state is on STOPPED
	if (!starting(_params)) // if returns false stop has maybe been called and this can be deleted, state stays STOPPED!
		return false;
	if (!_state) // if run() has been called state is RUNNING
		_state = STATE_STARTING;
	return true;
}

bool MediaStream::run() {
	if (_state == STATE_RUNNING)
		return true;
	if (!_params.onUnfound)
		return false; // is stopped!
	_state = STATE_RUNNING;
	++_runCount;
	INFO(description, " started");
	onRunning();
	return true;
}
void MediaStream::stop() {
	if (!_params.onUnfound) // already stopped!
		return;
	_params.onUnfound = nullptr;
	stopping();
	for (const auto& it : _targets)
		(MediaStream::OnStop&)it->onStop = nullptr; // unsuscribe because children pTarget can continue to live alone!
	_targets.clear(); // to invalid targets!
	if (_state == STATE_RUNNING)
		INFO(description, " stopped");
	_state = STATE_STOPPED;
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

bool MediaStream::initSocket(shared<Socket>& pSocket, const Parameters& parameters, const shared<TLS>& pTLS) {
	if (type <= 0)
		return false;
	if(!pSocket) {
		switch (type) {
			case TYPE_SRT:
	#if defined(SRT_API)
				pSocket.set<SRT::Socket>();
				break;
	#else
				ERROR(description, "SRT unsupported replacing by UDP (build MonaBase with SRT support before)");
	#endif
			default:
				if (pTLS)
					pSocket.set<TLS::Socket>(type >= TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM, pTLS);
				else
					pSocket.set(type >= TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM);
		}
	}
	Exception ex;
	AUTO_WARN(pSocket->processParams(ex, parameters, "stream."), description);
	return true;
}

const char* MediaStream::Format(Exception& ex, MediaStream::Type type, const char* request, Path& path) {
	static thread_local Path FMT;
	FMT = move(path); // clear path, and set default format
	URL::ParseRequest(request, path);
	switch (type) {
		case TYPE_HTTP:
		case TYPE_FILE:
			if (path.isFolder()) {
				ex.set<Ex::Protocol>(TypeToString(type), " stream can't be a folder");
				return NULL;
			}
		default:;
	}
	const char* format = FMT.c_str();
	if (!*format && !MIME::Read(path, format)) {
		switch (type) {
			case MediaStream::TYPE_SRT: // SRT and No Format => TS by default to catch Haivision usage
			case MediaStream::TYPE_UDP: // UDP and No Format => TS by default to catch with VLC => UDP = TS
				return "mp2t";
			default: {
				if (path.extension().empty())
					ex.set<Ex::Format>(TypeToString(type), " stream description have to indicate a media format");
				else
					ex.set<Ex::Unsupported>(TypeToString(type), " stream path has a format ", path.extension(), " unknown or not supported");
				return NULL;
			}
		}
	}
	return format;
}


unique<MediaStream> MediaStream::New(Exception& ex, Media::Source& source, const string& description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS) {

	// split "main url" and format?query!
	size_t size = description.find_last_of(" \t\r\n\v\f");
	// parse explicit format and parameters
	Parameters params;
	string format;
	if (size != string::npos)
		URL::ParseQuery(URL::ParseRequest(description.c_str() + size + 1, format), params);
	
	// parse protocol and address
	string protocol, addr;
	const char* request = URL::Parse(description.data(), size, protocol, addr);

	Type type(TYPE_FILE);
	bool isSecure(false);
	map<string, OnNewStream, String::IComparator>::const_iterator itOtherStream;
	if (!protocol.empty()) {
		static map<const char*, pair<Type, bool>, String::IComparator> Types({
			{ "FILE",{ TYPE_FILE, false } },
			{ "TCP",{ TYPE_TCP, false } },
			{ "TLS",{ TYPE_TCP, true } },
			{ "UDP",{ TYPE_UDP, false } },
			{ "HTTP",{ TYPE_HTTP, false } },
			{ "HTTPS",{ TYPE_HTTP, true } }
#if defined(SRT_API)
			,{ "SRT",{ TYPE_SRT, false } }
#endif
		});
		const auto& it = Types.find(protocol.c_str());
		if (it != Types.end()) {
			type = it->second.first;
			isSecure = it->second.second;
		} else {
			itOtherStream = _OtherStreams.find(protocol);
			if (itOtherStream == _OtherStreams.end()) {
				ex.set<Ex::Format>(protocol, " stream not supported");
				return nullptr;
			}
			type = TYPE_OTHER;
		}
	}

	bool isTarget(&source == &Media::Source::Null());
	SocketAddress address;
	bool toBind;
	if (type>0) { // is socket!
		toBind = addr[0] == '@';
		if (toBind)
			addr.erase(0, 1);

		Exception exc;
		IPAddress host;

		if (!address.host().set(exc, addr)) { // just host? (ipv6 with : for example)
			// if no ':' => just an host => has failed!
			size_t portPos = addr.rfind(':');
			if (portPos==string::npos) {
				ex = exc;
				return nullptr;
			}
			// has port!
			addr[portPos] = 0;
			if (!address.set(ex, addr, addr.c_str() + portPos + 1))
				return nullptr;
		} else {
			UInt16 port = Net::ResolvePort(ex, protocol.c_str());
			if (!port) // no port, search with protocol maybe?
				return nullptr;
			address.setPort(port);
		}
		if (!address.host())
			toBind = true;
	}
	
	
	String::Scoped scoped(request+size);
	unique<MediaStream> pStream;
	if (type>0) {
		// KEEP this model of double creation to allow a day a new RTPWriter<...>(parameter)
		if (type < TYPE_UDP && toBind) { // MediaServer if not UDP (UDP is always a MediaSocket) or if addresss to bind!
			if (isTarget)
				pStream = MediaServer::Writer::New(ex, type, request, address, ioSocket, isSecure ? pTLS : nullptr, move(format));
			else
				pStream = MediaServer::Reader::New(ex, type, request, source, address, ioSocket, isSecure ? pTLS : nullptr, move(format));
		} else {
			if (isTarget) {
				if (!address.host()) { // explicit 0.0.0.0 is an error here!
					ex.set<Ex::Net::Address::Ip>("Wildcard binding impossible for a stream UDP target");
					return nullptr;
				}
				pStream = MediaSocket::Writer::New(ex, type, request, address, ioSocket, isSecure ? pTLS : nullptr, move(format));
			} else
				pStream = MediaSocket::Reader::New(ex, type, request, source, address, ioSocket, isSecure ? pTLS : nullptr, move(format));
		}
	} else if(type==TYPE_FILE) {
		if (isTarget)
			pStream = MediaFile::Writer::New(ex, request, ioFile, move(format));
		else
			pStream = MediaFile::Reader::New(ex, request, source, timer, ioFile, move(format));
	} else
			pStream = itOtherStream->second(request, move(format), isTarget ? &source : NULL);
	if(pStream)
		pStream->_params.setParams(move(params));
	return pStream;
}

} // namespace Mona
