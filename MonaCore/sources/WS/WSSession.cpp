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

#include "Mona/WS/WSSession.h"
#include "Mona/ByteReader.h"
#include "Mona/JSONReader.h"

using namespace std;


namespace Mona {

WSSession::WSSession(Protocol& protocol, TCPSession& session, const shared<WSDecoder>& pDecoder) : Session(protocol, session), writer(session), _pSubscription(NULL), _pPublication(NULL),
	_onMessage([this](WS::Message& message) {
		Exception ex;

		switch (message.type) {
			case WS::TYPE_BINARY: {
				processMessage(ex, message, true);
				break;
			}
			case WS::TYPE_TEXT: {
				processMessage(ex, message);
				break;
			}
			case WS::TYPE_CLOSE: {
				BinaryReader reader(message.data(), message.size());
				UInt16 code(reader.read16());
				if (reader.available()) // if message, display a log, otherwise not necessary
					ERROR(name(), " close, ", String::Data(reader.current(), reader.available()));
				switch (code) {
					case 0: // no error code
					case WS::CODE_NORMAL_CLOSE:
						// normal error
						close();
						break;
					case WS::CODE_ENDPOINT_GOING_AWAY:
						// client is dying
						close(ERROR_SOCKET);
						break;
					case WS::CODE_POLICY_VIOLATION:
						// no permission
						close(ERROR_REJECTED);
						break;
					case WS::CODE_RESERVED_ABNORMAL_CLOSE:
						close(ERROR_APPLICATION);
						break;
					case WS::CODE_PROTOCOL_ERROR:
					case WS::CODE_PAYLOAD_NOT_ACCEPTABLE:
					case WS::CODE_MALFORMED_PAYLOAD:
					case WS::CODE_PAYLOAD_TOO_BIG:
						// protocol error
						close(ERROR_PROTOCOL);
						break;
					case WS::CODE_EXTENSION_REQUIRED:
						// Unsupported
						close(ERROR_UNSUPPORTED);
						break;
					default:
						// unexpected
						close(ERROR_UNEXPECTED);
						break;
				}
				return;
			}
			case WS::TYPE_PING:
				writer.writePong(message);
				writer.flush();
				break;
			case WS::TYPE_PONG: {
				UInt32 elapsed0(BinaryReader(message.data(), message.size()).read32());
				UInt32 elapsed1 = UInt32(peer.connection.elapsed());
				if (elapsed1>elapsed0)
					peer.setPing(elapsed1 - elapsed0);
				return;
			}
			default:
				ERROR(ex.set<Ex::Protocol>(name(), String::Format<UInt8>(" request type %#x unknown", message.type)));
				break;
		}

		// Close the session on exception beause nothing is expected in WebSocket to send an error excepting in "close" message (has a code and a string)
		if (ex)
			return close(TO_ERROR(ex));

		if (message.flush)
			flush();
	}) {
	pDecoder->onMessage = _onMessage;
}

void WSSession::kill(Int32 error, const char* reason) {
	if (died)
		return;

	// Stop reception
	_onMessage = nullptr;

	// unpublish and unsubscribe
	unpublish();
	unsubscribe();

	// onDisconnection after "unpublish or unsubscribe", but BEFORE _writers.clear() because call onDisconnection and writers can be used here
	Session::kill(error, reason);

	// close writer (flush) and release socket resources
	writer.close(error, reason);
}

void WSSession::subscribe(Exception& ex, string& stream, WSWriter& writer) {
	if(!_pSubscription)
		_pSubscription = new Subscription(writer);
	if (!api.subscribe(ex, peer, stream, *_pSubscription)) {
		delete _pSubscription;
		_pSubscription = NULL;
	}
	// support a format data!
	_pSubscription->setFormat(_pSubscription->getString("format"));
}
void WSSession::unsubscribe(){
	if (!_pSubscription)
		return;
	api.unsubscribe(peer,*_pSubscription);
	delete _pSubscription;
	_pSubscription = NULL;
}

void WSSession::publish(Exception& ex, string& stream) {
	unpublish();
	_media = Media::TYPE_NONE;
	_track = 0; // default for data!
	_pPublication=api.publish(ex, peer, stream);
}
void WSSession::unpublish() {
	if (!_pPublication)
		return;
	api.unpublish(*_pPublication, peer);
	_pPublication=NULL;
}

void WSSession::processMessage(Exception& ex, const Packet& message, bool isBinary) {

	Media::Data::Type type = isBinary ? Media::Data::TYPE_UNKNOWN : Media::Data::TYPE_JSON;
	unique<DataReader> pReader = Media::Data::NewReader<ByteReader>(type, message); // Use NewReader to check JSON validity
	string name;
	if (type==Media::Data::TYPE_JSON && pReader->readString(name) && name[0]=='@') {
		if (name == "@publish") {
			if (!pReader->readString(name)) {
				ERROR(ex.set<Ex::Protocol>("@publish method takes a stream name in first parameter"));
				return;
			}
			return publish(ex, name);
		}

		if (name == "@subscribe") {
			if (pReader->readString(name))
				return subscribe(ex, name, writer);
			ERROR(ex.set<Ex::Protocol>("@subscribe method takes a stream name in first parameter"));
			return;
		}

		if (name == "@unpublish")
			return unpublish();


		if (name == "@unsubscribe")
			return unsubscribe();
	
	} else if (_pPublication) {
		if (!isBinary)
			return _pPublication->writeData(type, message, _track);

		// Binary => audio or video or data

		Packet content(message);

		if(!_media) {
			BinaryReader header(message.data(), message.size());

			_media = Media::Unpack(header, _audio, _video, _data, _track);
			if (!_media) {
				ERROR("Malformed media header size");
				return;
			}
			if (!(content+=header.position()))
				return; // wait next binary!
		}
		switch (_media) {
			case Media::TYPE_AUDIO:
				_pPublication->writeAudio(_audio, content, _track);
				break;
			case Media::TYPE_VIDEO:
				_pPublication->writeVideo(_video, content, _track);
				break;
			default:
				_pPublication->writeData(_data, content, _track);
		}
		// reset header info because we have get the related content
		_track = 0;
		_media = Media::TYPE_NONE;
		return;
	}

	if (!peer.onInvocation(ex, name, *pReader, isBinary ? WS::TYPE_BINARY : (!type  ? WS::TYPE_TEXT : 0)))
		ERROR(ex);
}


bool WSSession::manage() {
	if (!Session::manage())
		return false;
	if (peer && peer.pingTime.isElapsed(timeout/2)) {
		writer.writePing(peer.connection);
		peer.pingTime.update();
	}
	// check subscription
	if (!_pSubscription)
		return true;
	switch (_pSubscription->ejected()) {
		case Subscription::EJECTED_BANDWITDH:
			writer.writeInvocation("@unsubscribe").writeString(EXPAND("Insufficient bandwidth"));
			break;
		case Subscription::EJECTED_ERROR:
			writer.writeInvocation("@unsubscribe").writeString(EXPAND("Unknown error"));
			break;
		default: return true;// no ejected!
	}
	unsubscribe();
	return true;
}

void WSSession::flush() {
	// flush publication
	if (_pPublication)
		_pPublication->flush(peer.ping());
	// flush writer
	writer.flush();
}

} // namespace Mona
