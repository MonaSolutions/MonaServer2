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

namespace Mona {

struct HTTPDecoder : Socket::Decoder, private StreamData<const shared<Socket>&>, protected Media::Source, virtual Object {
	typedef Event<void(HTTP::Request&)>  ON(Request);
	typedef Event<void(HTTP::Response&)> ON(Response);

	HTTPDecoder(const Handler& handler, const Path& path, std::string&& name = "");
	HTTPDecoder(const Handler& handler, const Path& path, const shared<HTTP::RendezVous>& pRendezVous, std::string&& name = "");

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

	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { receive(new Media::Audio(tag, packet, track)); }
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { receive(new Media::Video(tag, packet, track)); }
	void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) { receive(new Media::Data(type, packet, track)); }
	void setProperties(Media::Data::Type type, const Packet& packet, UInt8 track = 1) { receive(new Media::Data(type, packet, track, true)); }
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { if (lost) receive(type, lost, track); }
	void reset() { receive(!_length, !_length); if (!_length) _pReader.reset(); } // reset + end infos + reset _pReader to avoid double publication reset on flush!
	void flush() { if(_pReader) receive(false, true); }

	template <typename ...Args>
	void receive(Args&&... args) {
		if (_code) {
			if (onResponse)
				return _handler.queue(onResponse, _code, _pHeader, std::forward<Args>(args)...);
			_pHeader.reset();
			WARN(_name, " response ignored");
			return;
		}
		_lastRequest.update();
		if (onRequest)
			return _handler.queue(onRequest, _path, _pHeader, std::forward<Args>(args)...);
		_pHeader.reset();
		WARN(_name, " request ignored");
	}

	Exception				_ex;
	Stage					_stage;
	shared<HTTP::Header>	_pHeader;
	Time					_lastRequest;
	Path					_path;
	UInt16					_code;
	std::string				_www;
	const Handler&			_handler;
	unique<MediaReader>		_pReader;
	Int64					_length;
	std::string				_name;

	shared<HTTP::RendezVous> _pRendezVous;

	shared<Socket::Decoder>	_pUpgradeDecoder;
};


} // namespace Mona
