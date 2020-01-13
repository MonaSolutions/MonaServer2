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
#include "Mona/Socket.h"
#include "Mona/MediaReader.h"
#include "Mona/StreamData.h"
#include "Mona/HTTP/HTTP.h"
#include "Mona/WS/WSDecoder.h"

namespace Mona {

struct WSDecoder;
struct HTTPDecoder : Socket::Decoder, private StreamData<const shared<Socket>&>, protected Media::Source, virtual Object {
	typedef Event<void(HTTP::Request&)>  ON(Request);
	typedef Event<void(HTTP::Response&)> ON(Response);

	HTTPDecoder(const Handler& handler, const std::string& www, const char* name = NULL);
	HTTPDecoder(const Handler& handler, const std::string& www, const shared<HTTP::RendezVous>& pRendezVous, const char* name = NULL);

protected:
	void   decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);
	UInt32 onStreamData(Packet& buffer, const shared<Socket>& pSocket);

private:
	void  onRelease(Socket& socket);

	// for header reading, keep this order!!
	enum Stage {
		CMD,
		PATH,
		VERSION,
		LEFT,
		RIGHT,
		BODY,
		CHUNKED
	};

	const char* name(const Socket& socket) const { return _name ? _name : (socket.isSecure() ? "HTTPS" : "HTTP"); }
	const std::string& name() const { return Media::Source::name(); } // overrides Source::name, remove a clang warning!

	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { receive(new Media::Audio(tag, packet, track)); }
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { receive(new Media::Video(tag, packet, track)); }
	void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) { receive(new Media::Data(type, packet, track)); }
	void addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) { receive(new Media::Data(type, packet, track, true)); }
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { if (lost) receive(type, lost, track); }
	void reset() { receive(!_length, !_length); if (!_length) _pReader.reset(); } // reset + end infos + reset _pReader to avoid double publication reset on flush!
	void flush() { if(_pReader) receive(false, true); }

	template <typename ...Args>
	void receive(Args&&... args) {
		if (_code) { // response
			if (!onResponse) {
				_ex.set<Ex::Intern>();
				_pHeader.reset();
			} else
				_handler.queue(onResponse, _code, _pHeader, std::forward<Args>(args)...);
		} else { // request
			_lastRequest.update();
			if (!onRequest) {
				_ex.set<Ex::Intern>();
				_pHeader.reset();
			} else
				_handler.queue(onRequest, _file, _pHeader, std::forward<Args>(args)...);
		}
	}

	bool throwError(Socket& socket, bool onReceive=false);

	const char*				_name;
	Exception				_ex;
	Stage					_stage;
	shared<HTTP::Header>	_pHeader;
	Time					_lastRequest;
	Path					_file;
	UInt16					_code;
	std::string				_www;
	const Handler&			_handler;
	unique<MediaReader>		_pReader;
	Int64					_length;

	shared<HTTP::RendezVous> _pRendezVous;

	shared<WSDecoder>		_pWSDecoder;
};


} // namespace Mona
