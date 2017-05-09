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
#include "Mona/TCPSession.h"
#include "Mona/MediaWriter.h"
#include "Mona/HTTP/HTTPDataSender.h"
#include "Mona/HTTP/HTTPMediaSender.h"
#include "Mona/HTTP/HTTPFileSender.h"

namespace Mona {


struct HTTPWriter : Writer, virtual Object {
	HTTPWriter(TCPSession& session);
	~HTTPWriter();

	void			beginRequest(const shared<const HTTP::Header>& pRequest);
	void			endRequest();

	UInt64			queueing() const { return _session.socket()->queueing(); }

	void			clear() { _pResponse.reset(); _senders.clear();  }

	DataWriter&		writeInvocation(const char* name) { DataWriter& writer(writeMessage()); writer.writeString(name,strlen(name)); return writer; }
	DataWriter&		writeMessage() { return writeMessage(false); }
	DataWriter&		writeResponse(UInt8 type) { return writeMessage(true); }
	void			writeRaw(DataReader& reader);

	bool			writeSetCookie(DataReader& reader, const HTTP::OnCookie& onCookie = nullptr) { if (!_pSetCookie) _pSetCookie.reset(new Buffer()); return HTTP::WriteSetCookie(reader, *_pSetCookie, onCookie); }
	void			writeFile(const Path& file, Parameters& properties) { newSender<HTTPFileSender>(true, _session.api.ioFile, file, properties); }
	BinaryWriter&   writeRaw(const char* code);

	bool			beginMedia(const std::string& name, const Parameters& parameters);
	bool			writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return newSender<HTTPMediaSend<Media::Audio>>(_pMediaWriter, track, tag, packet) ? true : false; }
	bool			writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return newSender<HTTPMediaSend<Media::Video>>(_pMediaWriter, track, tag, packet) ? true : false; }
	bool			writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable) { return newSender<HTTPMediaSend<Media::Data>>(_pMediaWriter, track, type, packet) ? true : false; }
	// No writeProperties here because HTTP has no way to control a multiple channel global stream

	void			writeError(const Exception& ex) { newSender<HTTPDataSender>(true, HTTP::ErrorToCode(Session::ToError(ex)), ex); }
	template <typename ...Args>
	void			writeError(const char* code, Args&&... args) { newSender<HTTPDataSender>(true, code, std::forward<Args>(args)...); }

	void			flush() { Writer::flush(); }
private:
	void			flush(const shared<HTTPSender>& pSender);
	void			flushing();
	void			closing(Int32 error=0, const char* reason = NULL);

	DataWriter&		writeMessage(bool isResponse);

	template <typename SenderType, typename ...Args>
	shared<SenderType> newSender(Args&&... args) {
		return newSender<SenderType>(false, std::forward<Args>(args)...);
	}

	template <typename SenderType, typename ...Args>
	shared<SenderType> newSender(bool isResponse, Args&&... args) {
		if (!*this || !_pRequest) // if closed or has never gotten any request!
			return NULL;
		shared<SenderType> pSender(new SenderType(_session.socket(), _pRequest, _pSetCookie, std::forward<Args>(args)...));
		if(isResponse) {
			if (_pResponse) {
				if(_requesting)
					WARN("Response already written on ", _session.name()) // override the response, but should not happen, this warn can to check the code if happen
				else
					flush(); // force flush!
			}
			_pResponse = pSender;
		} else
			_senders.emplace_back(pSender);
		return pSender;
	}


	shared<MediaWriter>					_pMediaWriter;
	TCPSession&							_session;
	shared<Buffer>						_pSetCookie;

	shared<const HTTP::Header>			_pRequest; // Last request
	UInt32								_requestCount;
	bool								_requesting;
	std::deque<shared<HTTPSender>>		_flushings;

	// Variables to clear on HTTPWriter::clear()
	shared<HTTPSender>					_pResponse;
	std::deque<shared<HTTPSender>>		_senders;
};



} // namespace Mona
