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
#include "Mona/Writer.h"
#include "Mona/TCPSession.h"
#include "Mona/WS/WSSender.h"

namespace Mona {


struct WSWriter : Writer, Media::TrackTarget, virtual Object {
	WSWriter(TCPSession& session) : _session(session) {}
	
	UInt64			queueing() const { return _session->queueing(); }

	void			clear() { _senders.clear(); }

	DataWriter&		writeInvocation(const char* name);
	DataWriter&		writeMessage() { return writeJSON(); } // JSON
	DataWriter&		writeResponse(UInt8 type=0) { return write(WS::Type(type)); } // if type==0 will write a JSON message
	void			writeRaw(DataReader& arguments, const Packet& packet = Packet::Null());

	void			writePing() { write(WS::TYPE_PING)->write32(UInt32(_session.peer.connection.elapsed())); }
	void			writePong(const Packet& packet) { newSender(WS::TYPE_PONG, packet); }

	bool			beginMedia(const std::string& name);
	bool			writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
	bool			writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable);
	bool			writeData(Media::Data::Type type, const Packet& packet, bool reliable);
	bool			writeProperties(const Media::Properties& properties);
	void			endMedia();

	void			flush() { Writer::flush(); }
private:
	
	void			flushing();
	void			closing(Int32 error, const char* reason = NULL);

	DataWriter&		writeJSON(const Packet& packet = Packet::Null()) { return write(WS::TYPE_NIL, packet); }
	DataWriter&		writeJSON(Media::Data::Type packetType, const Packet& packet) { return closed() ? DataWriter::Null() : newSender<WSDataSender>(packetType, packet)->writer(); }
	DataWriter&		write(WS::Type type, const Packet& packet = Packet::Null()) { return closed() ? DataWriter::Null() : newSender(type, packet)->writer(); }
	template<typename SenderType=WSSender, typename ...Args>
	WSSender*		newSender(Args&&... args) {
		if (closed())
			return NULL;
		_senders.emplace_back(new SenderType(_session, std::forward<Args>(args)...));
		return _senders.back().get();
	}

	TCPSession&						_session;
	std::vector<shared<WSSender>>	_senders;
};



} // namespace Mona
