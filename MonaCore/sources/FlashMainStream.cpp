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

#include "Mona/FlashMainStream.h"
#include "Mona/MapWriter.h"
#include "Mona/Logs.h"
#include "Mona/URL.h"

using namespace std;


namespace Mona {

FlashMainStream::FlashMainStream(ServerAPI& api, Peer& peer) : FlashStream(0, api, peer, false), keepaliveServer(0), keepalivePeer(0) {
	
}

FlashMainStream::~FlashMainStream() {
	clearStreams();
}

void FlashMainStream::clearStreams() {
	for (auto& it : _streams) {
		it.second.onStart = nullptr;
		it.second.onStop = nullptr;
	}
	_streams.clear();
}

FlashStream* FlashMainStream::getStream(UInt16 id) {
	if (id == 0)
		return this;
	const auto& it = _streams.find(id);
	if (it == _streams.end())
		return NULL;
	return &it->second;
}

void FlashMainStream::messageHandler(const string& name, AMFReader& message, FlashWriter& writer, Net::Stats& netStats) {
	if(name=="connect") {
		message.stopReferencing();
	
		MapWriter<Parameters> mapWriter(peer.properties());
		if(message.read(AMFReader::OBJECT, mapWriter)) {
			const char* url = peer.properties().getString("tcUrl");
			if (url) {
				string serverAddress;
				peer.setQuery(URL::ParseRequest(URL::Parse(url, serverAddress), (Path&)peer.path, REQUEST_MAKE_FOLDER));
				URL::ParseQuery(peer.query, peer.properties());
				peer.setServerAddress(serverAddress);
			}
			if (peer.properties().getNumber<UInt32,3>("objectEncoding")==0) {
				writer.amf0 = amf0 = true;
				WARN("Client ",peer.protocol," not compatible with AMF3, few complex object can be not supported");
			}
		}

		message.startReferencing();
		
		// Check if the client is authorized
		AMFWriter& response = writer.writeAMFSuccess("NetConnection.Connect.Success","Connection succeeded",true);
		response.amf0 = true;
		response.writeNumberProperty("objectEncoding", writer.amf0 ? 0.0 : 3.0);
		response.amf0 = writer.amf0;
		Exception ex;
		(bool&)writer.isMain = true;
		peer.onConnection(ex, writer, netStats, message, response);
		if (ex) {
			if (ex.cast<Ex::Unfound>())
				writer.writeAMFError("NetConnection.Connect.InvalidApp", ex);
			else if(ex.cast<Ex::Application::Error>())
				writer.writeAMFError("NetConnection.Connect.Rejected", ex);
			else
				writer.writeAMFError("NetConnection.Connect.Failed", ex);
			writer.writeInvocation("close");
			writer.close();
			return;
		}
			
		response.endObject();

		if (keepaliveServer) {
			BinaryWriter& response = writer.writeRaw();
			response.write16(0x29); // Unknown!
			response.write32(keepaliveServer * 1000);
			response.write32(keepalivePeer * 1000);
		}
		return;
	}

	if (!peer) {
		// writer on FlashMainStream is necessary main request => a close will close the entiere session!
		writer.writeAMFError("NetConnection.Connect.Failed", "Connect before to send any message");
		writer.writeInvocation("close");
		writer.close();
		return;
	}

	if (name == "createStream") {

		UInt16 idStream(1);
		auto it(_streams.begin());
		for (; it != _streams.end(); ++it) {
			if (it->first > idStream)
				break;
			++idStream;
		}
		it = _streams.emplace_hint(it, SET, forward_as_tuple(idStream), forward_as_tuple(idStream, api, peer, amf0));

		it->second.onStart = onStart;
		it->second.onStop = onStop;

		AMFWriter& response(writer.writeMessage());
		response.amf0 = true;
		response.writeNumber(idStream);
		return;
	}
	
	if (name == "deleteStream") {
		UInt16 streamId;
		if (message.readNumber(streamId)) {
			const auto& it = _streams.find(streamId);
			if (it != _streams.end()) {
				it->second.onStart = nullptr;
				it->second.onStop = nullptr;
				_streams.erase(it);
			}
		} else
			ERROR("deleteStream message without id on flash stream ", streamId)
		return;
	}

	Exception ex;
	if (!peer.onInvocation(ex, name, message))
		LOG(name == "setPeerInfo" ? LOG_DEBUG : LOG_ERROR, ex);
	if(ex)
		writer.writeAMFState("_error", "error", "NetConnection.Call.Failed", ex); // AMS sends a AMFState+_error message and no a AMFStatusError here (allows to fix an issue too with ffmpeg/vlc and getStreamLength invocation)
}

void FlashMainStream::rawHandler(UInt16 type, const Packet& packet, FlashWriter& writer) {

	BinaryReader reader(packet.data(), packet.size());

	// setBufferTime
	if(type==0x0003) {
		UInt16 streamId = reader.read32();
		if(streamId==0) {
			bufferTime(reader.read32());
			return;
		}
		const auto& it = _streams.find(streamId);
		if (it==_streams.end()) {
			ERROR("setBufferTime message for an unknown ",streamId," stream")
			return;
		}
		it->second.bufferTime(reader.read32());
		return;
	}

	
	 // ping message
	if(type==0x0006) {
		writer.writePong(reader.read32());
		return;
	}

	 // pong message
	if(type==0x0007) {
		UInt32 elapsed0(reader.read32());
		if (!elapsed0)
			return; // can be equals to 0 in RTMP for example if a other ping is following
		UInt32 elapsed1 = UInt32(peer.connection.elapsed());
		if (elapsed1>elapsed0)
			peer.setPing(elapsed1 - elapsed0);
		return;
	}

	ERROR("Raw message ",String::Format<UInt16>("%.4x",type)," unknown on main stream");
}



} // namespace Mona
