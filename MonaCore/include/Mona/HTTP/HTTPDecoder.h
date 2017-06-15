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

struct HTTPDecoder : Socket::Decoder, private StreamData<Socket&>, private Media::Source, virtual Object {
	typedef Event<void(HTTP::Request&)> ON(Request);

	HTTPDecoder(ServerAPI& api) : _pReader(NULL), _www(api.www), _decoded(0), _stage(CMD), _handler(api.handler) {
		FileSystem::MakeFile(_www);
	}

private:
	// for header reading, keep this order!!
	enum Stage {
		CMD,
		PATH,
		VERSION,
		LEFT,
		RIGHT,
		BODY,
		PROGRESSIVE
	};

	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) { receive(_pHeader, track, tag, packet); }
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) { receive(_pHeader, track, tag, packet); }
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet) { receive(_pHeader, track, type, packet); }
	void setProperties(UInt8 track, DataReader& reader) { receive(_pHeader, track, reader); }
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { if (lost) receive(_pHeader, type, lost, track); }
	void reset() { receive(_pHeader); }
	void flush() { receive(_pHeader, !_length); /* flush + end infos*/ }

	UInt32 decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);
	UInt32 onStreamData(Packet& buffer, Socket& socket);

	template <typename ...Args>
	void receive(Args&&... args) { _handler.queue(onRequest, std::forward<Args>(args)...); }

	Exception				_ex;
	Stage					_stage;
	shared<HTTP::Header>	_pHeader;
	Path					_file;
	UInt32					_decoded;
	std::string				_www;
	const Handler&			_handler;
	MediaReader*			_pReader;
	Int64					_length;

	shared<StreamData<Socket&>>	_pUpgradeDecoder;
};


} // namespace Mona
