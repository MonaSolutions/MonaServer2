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
#include "Mona/HTTP/HTTPErrorSender.h"
#include "Mona/HTTP/HTTPDataSender.h"
#include "Mona/HTTP/HTTPMediaSender.h"
#include "Mona/HTTP/HTTPFileSender.h"
#include "Mona/HTTP/HTTPFolderSender.h"

namespace Mona {


struct HTTPWriter : Writer, Media::Target, virtual Object {
	HTTPWriter(TCPSession& session);

	void			beginRequest(const shared<const HTTP::Header>& pRequest);
	void			endRequest();

	UInt64			queueing() const { return _session->queueing(); }

	void			clear() { _pResponse.reset(); _senders.clear();  }

	DataWriter&		writeInvocation(const char* name) { DataWriter& writer(writeMessage()); writer.writeString(name,strlen(name)); return writer; }
	DataWriter&		writeMessage() { return writeMessage(false); }
	DataWriter&		writeResponse(UInt8 type=0) { return writeMessage(true); }
	void			writeRaw(DataReader& arguments, const Packet& packet = Packet::Null());

	/*!
	Set the cookie key to value, if reader is empty it's a cookie to remove, otherwise first argument must be a string and the rest are parameters settings */
	void			writeSetCookie(const std::string& key, DataReader& reader);

	void			writeFile(const Path& file, Parameters& properties);
	BinaryWriter&   writeRaw(const char* code);
	DataWriter&     writeResponse(const char* subMime);

	bool			beginMedia(const std::string& name);
	bool			writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return newSender<HTTPMediaSender>(_pMediaWriter, new Media::Audio(tag, packet, track)) ? true : false; }
	bool			writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return newSender<HTTPMediaSender>(_pMediaWriter, new Media::Video(tag, packet, track)) ? true : false; }
	bool			writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return newSender<HTTPMediaSender>(_pMediaWriter, new Media::Data(type, packet, track)) ? true : false; }
	// No writeProperties here because HTTP has no way to control a multiple channel global stream
	bool			endMedia();

	void			writeError(const Exception& ex) { newSender<HTTPErrorSender>(true, HTTP::ErrorToCode(Session::ToError(ex)), ex); }
	template <typename ...Args>
	void			writeError(const char* code, Args&&... args) { newSender<HTTPErrorSender>(true, code, std::forward<Args>(args)...); }

	bool			answering() const { return !_flushings.empty() || _session->queueing(); }
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
			return nullptr;
		shared<SenderType> pSender(SET, _pRequest, _session.socket(), std::forward<Args>(args)...);
		pSender->setCookies(_pSetCookie);
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

	File::OnError						_onFileError;
	File::OnReaden						_onFileReaden;

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
