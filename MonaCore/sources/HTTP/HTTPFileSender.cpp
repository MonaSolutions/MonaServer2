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

#include "Mona/HTTP/HTTPFileSender.h"

using namespace std;


namespace Mona {


HTTPFileSender::HTTPFileSender(const shared<Socket>& pSocket,
	const shared<const HTTP::Header>& pRequest,
	shared<Buffer>& pSetCookie,
	IOFile& ioFile, const Path& file, Parameters& properties) : HTTPSender("HTTPFileSender", pSocket, pRequest, pSetCookie),
		_file(file), _properties(move(properties)), FileReader(ioFile), _head(pRequest->type==HTTP::TYPE_HEAD),
		_onEnd([this]() {
			if (_keepalive)
				return io.handler.queue(HTTPSender::onFlush);
			socket()->shutdown();
		}) {
}

bool HTTPFileSender::flush() {
	// /!\ Can be called by an other thread!
	if (opened()) {
		// continue to read file when paused on congestion
		read();
		return false;
	}
	return _head || _file.isFolder();
}


shared<File::Decoder> HTTPFileSender::newDecoder() {
	shared<Decoder> pDecoder(new Decoder(io.handler, socket()));
	pDecoder->onEnd = _onEnd;
	return pDecoder;
}

UInt32 HTTPFileSender::Decoder::decode(shared<Buffer>& pBuffer, bool end) {
	// /!\ Parallel thread!
	Packet packet(pBuffer);
	if (!HTTP::Send(*_pSocket, packet))
		return 0; // stop reading (socket is shutdown now!)
	if (end)
		_handler.queue(onEnd);
	return _pSocket->queueing() ? 0 : packet.size();
}

void HTTPFileSender::run(const HTTP::Header& request, bool& keepalive) {
	
	string buffer;
	Exception ex;

	if (_file.isFolder()) {
		// FOLDER
		if (_file.exists()) {
			shared<Buffer> pBuffer(new Buffer(4, "\r\n\r\n"));
			BinaryWriter writer(*pBuffer);
			HTTP_BEGIN_HEADER(writer)
				HTTP_ADD_HEADER("Last-Modified", String::Date(Date(_file.lastModified()), Date::FORMAT_HTTP));
			HTTP_END_HEADER

			HTTP::Sort		sort(HTTP::SORT_ASC);
			HTTP::SortBy	sortBy(HTTP::SORTBY_NAME);

			if (_properties.count()) {
				if (_properties.getString("N", buffer))
					sortBy = HTTP::SORTBY_NAME;
				else if (_properties.getString("M", buffer))
					sortBy = HTTP::SORTBY_MODIFIED;
				else if (_properties.getString("S", buffer))
					sortBy = HTTP::SORTBY_SIZE;
				if (buffer == "D")
					sort = HTTP::SORT_DESC;
			}

			bool success;
			AUTO_ERROR(success = HTTP::WriteDirectoryEntries(ex, writer, _file, request.path, sortBy, sort), "HTTP Folder view");
			if (success)
				send(HTTP_CODE_200, MIME::TYPE_TEXT, "html; charset=utf-8", Packet(pBuffer));
			else
				sendError(HTTP_CODE_500, "List folder files, ", ex);
		} else
			sendError(HTTP_CODE_404, "The requested URL ", request.path, "/ was not found on the server");
		return;
	}

	// FILE
	// /!\ Here if !_head we have to call _onEnd on end!
	if (!_head) {
		// keep alive the time to read the file!
		_keepalive = keepalive;
		keepalive = true;
	}

	onError = [this](const Exception& ex) {
		ERROR(ex);
		socket()->shutdown(); // can't repair the session (client wait content-length!)
		return;
	};
	if (!open(ex, _file)) {
		if (ex.cast<Ex::Unfound>()) {
			// File doesn't exists but maybe folder exists?
			if (FileSystem::Exists(MAKE_FOLDER(_file))) {
				/// Redirect to the real folder path
				shared<Buffer> pBuffer(new Buffer(4, "\r\n\r\n"));
				BinaryWriter writer(*pBuffer);
				// Full URL required here relating RFC2616 section 10.3.3
				String::Assign(buffer,request.protocol, "://", request.host, request.path, '/', _file.name(), '/');
				HTTP_BEGIN_HEADER(writer)
					HTTP_ADD_HEADER("Location", buffer);
				HTTP_END_HEADER
				HTML_BEGIN_COMMON_RESPONSE(writer, EXPAND("Moved Permanently"))
					writer.write(EXPAND("The document has moved <a href=\"")).write(buffer).write(EXPAND("\">here</a>."));
				HTML_END_COMMON_RESPONSE(buffer)
				if(!send(HTTP_CODE_301, MIME::TYPE_TEXT, "html; charset=utf-8", Packet(pBuffer)))
					return;
			} else if (!sendError(HTTP_CODE_404, "The requested URL ", request.path, '/', _file.name(), " was not found on the server"))
				return;

		} else if (!sendError(HTTP_CODE_401, "Impossible to open ", request.path, '/', _file.name(), " file"))
			return;
		if(!_head)
			_onEnd();
		return;
	}

	/// not modified if there is no parameters file (impossible to determinate if the parameters have changed since the last request)
	if (!_properties.count() && request.ifModifiedSince >= (*this)->lastModified()) {
		if(send(HTTP_CODE_304) && !_head) // NOT MODIFIED
			_onEnd();
		return;
	}

	// Parsing file and replace with parameters every time requested by user (when properties returned by onRead):
	// - Allow to parse every type of files! same a SVG file!
	// - Allow to control "304 not modified code" response in checking arguments "If-Modified-Since" and update properties time
	// - Allow by default to never parsing the document!

	if (_properties.count() && (*this)->size() > 0xFFFF) {
		ERROR(request.path, '/', _file.name(), " exceeds limit size of 65535 to allow parameter parsing, file parameters ignored");
		_properties.clear();
	}
	if(_properties.count()) {
		// parsing file!
		shared<Buffer> pBuffer(new Buffer(0xFFFF));
		int readen;
		AUTO_ERROR(readen = (*this)->read(ex, pBuffer->data(), pBuffer->size()),"HTTP get ", request.path, '/', _file.name());
		if (readen < 0) {
			ERROR(ex);
			socket()->shutdown(); // can't repair the session (client wait content-length!)
			return; 
		}
		if (sendFile(Packet(pBuffer, pBuffer->data(), readen)) && !_head)
			_onEnd();
	} else if(sendHeader((*this)->size()) && !_head)
		read();
}

bool HTTPFileSender::sendHeader(UInt64 fileSize) {
	// Create something to download by default!
	const char* subMime;
	MIME::Type mime = MIME::Read(_file, subMime);
	if (!mime) {
		mime = MIME::TYPE_APPLICATION;
		subMime = "octet-stream";
	}
	shared<Buffer> pBuffer(new Buffer(4, "\r\n\r\n"));
	BinaryWriter writer(*pBuffer);
	HTTP_BEGIN_HEADER(writer)
		HTTP_ADD_HEADER("Last-Modified", String::Date(Date((*this)->lastModified()), Date::FORMAT_HTTP));
	HTTP_END_HEADER
	return send(HTTP_CODE_200, mime, subMime, Packet(pBuffer), fileSize);
}

bool HTTPFileSender::sendFile(const Packet& packet) {

	vector<Packet> packets;

	// iterate on content to replace "<% key %>" fields
	// At less one char!

	const UInt8* begin = packet.data();
	const UInt8* cur = begin;
	const UInt8* end = begin + packet.size();
	UInt32 size(0);
	std::string	 tag;
	UInt8		 stage(0);

	while (cur<end) {

		if (*cur == '<') {
			tag.clear();
			stage = 1;
			++cur;
			continue;
		}

		switch (stage) {
			case 1:
				if (*cur == '%') {
					if ((cur - 1) > begin) {
						packets.emplace_back(packet, begin, cur - begin - 1);
						size += packets.back().size();
					}
					++stage;
				} else
					--stage;
				break;
			case 2:
				if (isspace(*cur))
					break;
				++stage;
			case 3:
				if (*cur == '%')
					++stage;
				else
					tag += *cur;
				break;
			case 4:
				if (*cur == '>') {
					const char* value = _properties.getString(String::TrimRight(tag));
					if (value) {
						packets.emplace_back(Packet(value, strlen(value)));
						size += packets.back().size();
					}
					begin = cur + 1;
					stage = 0;
				} else {
					--stage;
					tag += '%';
					tag += *cur;
				}
				break;
			default: if(stage) ERROR("HTTP parsing tag stage ", stage, " not expected");
		}

		if (tag.size() > 0xFF) {
			WARN("Script parameter exceeds maximum 256 size");
			tag.clear();
			stage = 0;
		}

		++cur;
	}


	// Add rest size
	size += cur - begin;
	if (!sendHeader(size))
		return false;
	if (_head)
		return true;

	for (const Packet& packet : packets) {
		if (!send(packet))
			return false;
	}

	// Send the rest
	return cur > begin ? send(Packet(packet, begin, cur - begin)) : true;
}

} // namespace Mona
