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

#include "Mona/RTMP/RTMPSession.h"
#include "Mona/RTMP/RTMPDecoder.h"
#include "math.h"


using namespace std;


namespace Mona {


RTMPSession::RTMPSession(Protocol& protocol, const shared<Socket>& pSocket) : _first(true), _controller(2, *this, _pEncryptKey), _mainStream(api,peer), TCPSession(protocol, pSocket),
	_onRequest([this](RTMP::Request& request) {
		RTMPWriter& writer(request.channelId == 2 ? _controller : _writers.emplace(SET, forward_as_tuple(request.channelId), forward_as_tuple(request.channelId, *this, _pEncryptKey)).first->second);
		writer.streamId = request.streamId;

		if (_first) {
			_first = false;
			// first reception => client settings
			_controller.writeProtocolSettings();
			_controller.flush(); // before other writting!
		}
		if (request.ackBytes)
			_controller.writeAck(request.ackBytes);

		if (request.type == AMF::TYPE_BANDWIDTH) {
			// send a win_acksize message for accept this change
			_controller.writeWinAckSize(BinaryReader(request.data(), request.size()).read32());
		} else {
			FlashStream* pStream(_mainStream.getStream(request.streamId));
			if (!pStream) {
				ERROR(name(), " indicates a non-existent ", request.streamId, " FlashStream");
				kill(ERROR_PROTOCOL);
				return;
			}
			if (!peer) {
				SocketAddress address;
				if (peer.onHandshake(address)) {
					// redirection
					AMFWriter& amf = writer.writeAMFError("NetConnection.Connect.Rejected", "Redirection (see info.ex.redirect)", true);
					amf.writePropertyName("ex");
					amf.beginObject();
					amf.writeNumberProperty("code", 302);
					amf.writeStringProperty("redirect", String(String::Lower(peer.protocol), "://", address, "/", peer.path));
					amf.endObject();
					amf.endObject();
					writer.writeInvocation("close");
					kill();
					return;
				}
			}
			if(pStream->process(request.type, request.time, request, writer, *self))
				writer.flush();
		}
		if (!request.flush)
			return;
		// controller flush
		_controller.flush();
		// publication flush
		_mainStream.flush();
	}) {
	_mainStream.onStart = [this](UInt16 id, FlashWriter& writer) {
		// Stream Begin signal
		_controller.writeRaw().write16(0).write32(id);
		_controller.flush();
	};
	_mainStream.onStop = [this](UInt16 id, FlashWriter& writer) {
		// Stream EOF signal
		_controller.writeRaw().write16(1).write32(id);
		_controller.flush();
	};
	
}

Socket::Decoder* RTMPSession::newDecoder() {
	RTMPDecoder* pDecoder = new RTMPDecoder(api.handler);
	pDecoder->onEncryptKey = [this](unique<RC4_KEY>& pEncryptKey) {
		_pEncryptKey = move(pEncryptKey);
		((string&)peer.protocol) = _pEncryptKey ? "RTMPE" : "RTMP";
	};
	pDecoder->onRequest = _onRequest;
	return pDecoder;
}

void RTMPSession::kill(Int32 error, const char* reason) {
	if (died)
		return;

	// Stop reception
	_onRequest = nullptr;

	// unpublish and unsubscribe
	_mainStream.onStart = nullptr;
	_mainStream.onStop = nullptr;
	_mainStream.clearStreams();

	// onDisconnection after "unpublish or unsubscribe", but BEFORE _writers.close() to allow last message
	peer.onDisconnection();

	// close writers (flush)
	for (auto& it : _writers)
		it.second.close(error, reason);
	
	// in last because will disconnect
	TCPSession::kill(error, reason);

	// release resources (sockets)
	_writers.clear();
	_controller.clear();
}

bool RTMPSession::manage() {
	if (!TCPSession::manage())
		return false;
	if (peer && peer.pingTime.isElapsed(timeout/2)) {
		_controller.writePing(peer.connection);
		peer.pingTime.update();
	}
	return true;
}

void RTMPSession::flush() {
	// controller flush
	_controller.flush();
	// publication flush
	_mainStream.flush();
	// flush writers
	for (auto& it : _writers)
		it.second.flush();
}


} // namespace Mona
