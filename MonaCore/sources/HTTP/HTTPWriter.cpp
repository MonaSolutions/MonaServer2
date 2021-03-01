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
#include "Mona/QueryWriter.h"

using namespace std;

namespace Mona {

struct RawWriter : DataWriter, virtual Object {

	RawWriter(Buffer& buffer) : _level(0), DataWriter(buffer), _header(false), _first(true) {}

	UInt64 beginObject(const char* type = NULL) {
		if (_level++ || !_first)
			return 0;
		// begin header!
		writer.buffer().resize(writer.buffer().size() - 2);
		_header = true;
		return 0;
	}
	void endObject() {
		if (--_level || !_header)
			return;
		// end header!
		if (!_first) // has no content-type
			writer.write(EXPAND("Content-Type: text/html; charset=utf-8\r\n"));
		writer.write("\r\n");
		_first = _header = false;
	}

	void writePropertyName(const char* value) {
		if (!_header)
			return writeString(value, strlen(value));
		if(_first)
			_first = String::ICompare(value, "Content-Type") != 0;
		writer.write(value).write(": ");
	}

	UInt64 beginArray(UInt32 size) { if(!_level++ && _first) _first=false; return 0; }
	void   endArray() { --_level; }

	void writeNumber(double value) { append(value); }
	void writeString(const char* value, UInt32 size) { append(String::Data(value, size)); }
	void writeBoolean(bool value) { append(value ? "true" : "false"); }
	void writeNull() { append("null"); }
	UInt64 writeDate(const Date& date) { append(String::Date(date, Date::FORMAT_HTTP));  return 0; }
	UInt64 writeByte(const Packet& bytes) { append(bytes); return 0; }

	void   reset() { _header = false; _first = true; _level = 0; DataWriter::reset(); }
private:
	template<typename ValueType>
	void append(ValueType&& value) {
		if (!_header && !_level && _first)
			_first = false;
		String::Append(writer, std::forward<ValueType>(value));
	}

	UInt8				_level;
	bool				_header;
	bool				_first;
};


HTTPWriter::HTTPWriter(TCPSession& session) : _requestCount(0), _requesting(false), _session(session), crossOriginIsolated(false),
	_onSenderEnd([&]() {
#if !defined(_DEBUG)
		if (_flushings.empty()) {
			CRITIC(_session.name(), " HTTPSender::onEnd without flushing sender");
			return;
		}
#endif
		if (_flushings.front()->isFile()) {
			shared<HTTPFileSender> pFile = static_pointer_cast<HTTPFileSender>(_flushings.front());
			_session.api.ioFile.unsubscribe(pFile);
		}
		_flushings.pop_front();
		flush();

	}),
	_onFileError([&](const Exception& ex) {
		if (ex.cast<Ex::Intern>()) // trick to solve HTTPFileSender::load override
			return _onSenderEnd();
		// Don't call _onSenderEnd() to avoid to write new response with flush call,
		// kill will not write a new response because pFile is not unique (IOFile::ErrorHandler has a handler on)
		if (!ex.cast<Ex::Net::Socket>()) // if socket shutdown WARN already displaid
			WARN(ex);
		// File error kill the session => can't repair the session (client wait content-length!)
		_session.kill(Session::ToError(ex));
	}) {
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


HTTPWriter& HTTPWriter::beginRequest(const shared<const HTTP::Header>& pRequest) {
	++_requestCount;
	_pRequest = pRequest;
	_requesting = true;
	return self;
}

void HTTPWriter::endRequest() {
	_requesting = false;
}


void HTTPWriter::flush() {
	Writer::flush();
	// RESUME FLUSHINGS
	while (!_flushings.empty()) {
		const shared<HTTPSender>& pSender = _flushings.front();
		if (pSender->flushing())
			return; // wait socket onFlush
		// send or resend
		if (pSender.unique() && *pSender) {
			if (pSender->isFile())
				_session.api.ioFile.read(static_pointer_cast<HTTPFileSender>(pSender));
			else
				_session.send(pSender);
		}
		if (pSender->onEnd)
			return; // wait onEnd!
		_flushings.pop_front();
	}
}


void HTTPWriter::flushing() {
	// SENDERS => FLUSHINGS
	if (_requesting) // during request wait the main response before flush
		return;

	if (_pResponse) {
		--_requestCount;
		_flushings.emplace_back(move(_pResponse));
	}
	while(!_senders.empty()) {
		shared<HTTPSender>& pSender = _senders.front();
		if (pSender->hasHeader()) {
			// send requestCount packet with header!
			if (!_requestCount)
				break;
			--_requestCount;
		} // else partial message (without header) always sends!
		_flushings.emplace_back(move(pSender));
		_senders.pop_front();
	};
}

void HTTPWriter::writeSetCookie(const string& key, DataReader& reader) {
	if (!_pSetCookie)
		_pSetCookie.set();

	String::Append(*_pSetCookie, "\r\nSet-Cookie: ", key, '=');
	StringWriter<> value(*_pSetCookie);
	reader.read(value, 1);
	String::Append(*_pSetCookie, "; path=", _session.peer.path.length() ? _session.peer.path : "/");
	// write other params
	if (!reader.available())
		return;
	QueryWriter writer(_pSetCookie ? *_pSetCookie : _pSetCookie.set());
	writer.separator = "; ";
	writer.dateFormat = Date::FORMAT_RFC1123;
	writer.uriChars = false;
	writer->write(EXPAND("; "));
	reader.read(writer);
}

void HTTPWriter::writeFile(const Path& file, Parameters& properties) {
	if (!file.isFolder()) {
		shared<HTTPFileSender> pFileSender = newSender<HTTPFileSender>(true, file, properties);
		if (!pFileSender)
			return;
		pFileSender->onEnd = _onSenderEnd;
		_session.api.ioFile.subscribe(pFileSender, (File::Decoder*)pFileSender.get(), nullptr, _onFileError);
	} else
		newSender<HTTPFolderSender>(true, file, properties);
}

void HTTPWriter::writeSegment(const Path& path, const Segment& segment, Parameters& params) {
	shared<HTTPSegmentSender> pSender = newSender<HTTPSegmentSender>(true, path, segment, params);
	if (pSender)
		pSender->onEnd = _onSenderEnd;
}

DataWriter& HTTPWriter::writeMessage(bool isResponse) {
	shared<HTTPDataSender> pSender;
	if (_pRequest->mime)
		pSender = newSender<HTTPDataSender>(isResponse, HTTP_CODE_200, _pRequest->mime, _pRequest->subMime);
	else // else JSON by default
		pSender = newSender<HTTPDataSender>(isResponse, HTTP_CODE_200, MIME::TYPE_APPLICATION, "json");
	return pSender ? pSender->writer() : DataWriter::Null();
}

DataWriter& HTTPWriter::writeResponse(const char* subMime) {
	shared<HTTPDataSender> pSender = newSender<HTTPDataSender>(true, HTTP_CODE_200, MIME::TYPE_APPLICATION, subMime ? subMime : "json");
	return pSender ? pSender->writer() : DataWriter::Null();
}

void HTTPWriter::writeRaw(DataReader& arguments, const Packet& packet) {
	// Take the entiere control
	// first parameter is HTTP headers in a object view
	shared<HTTPDataSender> pSender = newSender<HTTPDataSender>(HTTP_CODE_200, MIME::TYPE_UNKNOWN, nullptr, packet); // null mime => raw format and no content_type (can be to set in arguments or directly in packets)
	if (!pSender || !arguments.available())
		return;
	RawWriter writer(pSender->writer()->buffer());
	arguments.read(writer);
}

BinaryWriter& HTTPWriter::writeRaw(const char* code) {
	shared<HTTPDataSender> pSender = newSender<HTTPDataSender>(code, MIME::TYPE_UNKNOWN);
	return pSender ? *pSender->writer() : BinaryWriter::Null();
}

bool HTTPWriter::beginMedia(const string& name) {
	// _pMediaWriter->begin(...)
	if (_pMediaWriter)
		return true; // MBR switch!
	if (!newSender<HTTPMediaSender>(_pMediaWriter))
		return false; // writer closed!
	if (_pMediaWriter)
		return true; // started!
	// 415 Unsupported media type
	_senders.pop_back();
	String error("Can't play ", name, ", HTTP streaming doesn't support ", _pRequest->subMime, " media");
	ERROR(error);
	writeError(HTTP_CODE_415, error);
	return false;
}

bool HTTPWriter::endMedia() {
	if (!_pMediaWriter)
		return true;
	newSender<HTTPMediaSender>(_pMediaWriter); // End media => Close socket
	return false;
}

} // namespace Mona
