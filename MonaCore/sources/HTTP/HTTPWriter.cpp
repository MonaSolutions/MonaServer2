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

#include "Mona/HTTP/HTTPWriter.h"

using namespace std;

namespace Mona {

struct HeaderWriter : DataWriter, virtual Object {
	HeaderWriter(BinaryWriter& writer) : _writer(writer), _level(0), _hasContentType(false) {}

	operator bool() const { return _level==1; }
	bool hasContentType() const { return _hasContentType; }

	UInt64 beginObject(const char* type = NULL) { if(_level<=1) ++_level;  return 0; }
	void   endObject() {}

	void writePropertyName(const char* value) {
		if (_level != 1)
			return;
		if(!_hasContentType)
			_hasContentType = String::ICompare(value, "Content-Type") == 0;
		_writer.write(value).write(": ");
	}

	UInt64 beginArray(UInt32 size) { _level = 2; return 0; }
	void   endArray() {}

	void writeNumber(double value) { if (_level == 1) String::Append(_writer, value); }
	void writeString(const char* value, UInt32 size) { if (_level == 1) String::Append(_writer, value, size); }
	void writeBoolean(bool value) { if (_level == 1) _writer.write(value ? "true" : "false"); }
	void writeNull() { if (_level == 1) _writer.write(EXPAND("null")); }
	UInt64 writeDate(const Date& date) { if (_level == 1) String::Append(_writer, String::Date(date, Date::FORMAT_HTTP));  return 0; }
	UInt64 writeBytes(const UInt8* data, UInt32 size) { if (_level == 1) _writer.write(STR data, size); return 0; }

	void   clear(UInt32 size = 0) { _hasContentType = false;  _level = 0; _writer.clear(size); }
private:
	UInt8			_level;
	BinaryWriter&	_writer;
	bool			_hasContentType;
};


HTTPWriter::HTTPWriter(TCPSession& session) : _requestCount(0),_requesting(false),_session(session) {}

HTTPWriter::~HTTPWriter() {
	for (shared<HTTPSender>& pSender : _flushings)
		pSender->onFlush = nullptr;
}

void HTTPWriter::closing(Int32 error, const char* reason) {
	// if normal close, or negative error (can't answer), or no before request (one request = one response) => no response
	if (error <= 0 || !_requestCount)
		return;
	if(!_requestCount)
		return;
	const char* code = HTTP::ErrorToCode(error);
	if (!code) {
		if (reason)
			writeError(HTTP_CODE_406, reason);
		else
			writeError(HTTP_CODE_406, "User error ", error);
	} else if(reason)
		writeError(code, reason);
	else
		writeError(code);
}


void HTTPWriter::beginRequest(const shared<const HTTP::Header>& pRequest) {
	++_requestCount;
	_pRequest = pRequest;
	_requesting = true;
}

void HTTPWriter::endRequest() {
	_requesting = false;
}

void HTTPWriter::flush(const shared<HTTPSender>& pSender) {
	bool flush = pSender->flush();
	if (!flush) {
		// Wait onFlush!
		pSender->onFlush = [this]() {
			_flushings.pop_front();
			while (!_flushings.empty()) {
				bool flush = _flushings.front()->flush();
				_session.send(_flushings.front());
				if (!flush)
					break; // Wait onFlush!
				_flushings.pop_front();
			}
		};	
	}
	if (_flushings.empty())
		_session.send(pSender);
	if (!flush || !_flushings.empty())
		_flushings.emplace_back(pSender);
}

void HTTPWriter::flushing() {
	for (shared<HTTPSender>& pSender : _flushings) {
		if(pSender.unique()) // just if pSender is not running!
			pSender->flush(); // continue to read the file when paused on congestion
	}
	if (_requesting) // during request wait the main response before flush
		return;
	if (_pResponse) {
		--_requestCount;
		flush(_pResponse);
		_pResponse.reset();
	}
	while(!_senders.empty()) {
		shared<HTTPSender>& pSender = _senders.front();
		if (!pSender->hasHeader()) {
			// partial message (without header) always sends!
			flush(pSender);
		} else {
			// send requestCount packet with header!
			if (!_requestCount)
				return;
			--_requestCount;
			flush(pSender);
		}
		_senders.pop_front();
	};
}

DataWriter& HTTPWriter::writeMessage(bool isResponse) {
	shared<HTTPDataSender> pSender;
	if (!_pRequest->mime) // JSON by default
		pSender = newSender<HTTPDataSender>(isResponse, HTTP_CODE_200, MIME::TYPE_APPLICATION, "json");
	else
		pSender = newSender<HTTPDataSender>(isResponse, HTTP_CODE_200, _pRequest->mime, _pRequest->subMime);
	return pSender ? pSender->writer() : DataWriter::Null();
}

void HTTPWriter::writeRaw(DataReader& reader) {
	// Take the entiere control
	shared<HTTPDataSender> pSender = newSender<HTTPDataSender>(HTTP_CODE_200, MIME::TYPE_UNKNOWN);
	if (!pSender)
		return;
	HTTP_BEGIN_HEADER(*pSender->writer())
		HeaderWriter writer(*pSender->writer());
		if (!reader.read(writer, 1) || !writer) {
			writer.clear();
			ERROR("HTTP content required at less a HTTP header object");
			_senders.pop_back();
		} else if (!writer.hasContentType()) {
			Media::Data::Type type = Media::Data::ToType(reader);
			if(type)
				writer->write(EXPAND("Content-Type: application/")).write(Media::Data::TypeToSubMime(type)).write("\r\n");
			else
				writer->write(EXPAND("Content-Type: text/html; charset=utf-8\r\n"));
		}
	HTTP_END_HEADER
}

BinaryWriter& HTTPWriter::writeRaw(const char* code) {
	shared<HTTPDataSender> pSender = newSender<HTTPDataSender>(code, MIME::TYPE_UNKNOWN);
	return pSender ? *pSender->writer() : BinaryWriter::Null();
}

bool HTTPWriter::beginMedia(const string& name) {
	// _pMediaWriter->begin(...)
	if (!newSender<HTTPMediaSender>(_pMediaWriter))
		return false; // writer closed!
	if (_pMediaWriter)
		return true;
	// 415 Unsupported media type
	_senders.pop_back();
	string error;
	ERROR(String::Assign(error, "Can't play ", name,", HTTP streaming doesn't support ", _pRequest->subMime, " media"));
	writeError(HTTP_CODE_415, error);
	return false;
}

void HTTPWriter::endMedia(const string& name) {
	if (_pMediaWriter)
		newSender<HTTPMediaSender>(_pMediaWriter); // End media => Close socket
}

} // namespace Mona
