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
#include "Mona/Protocol.h"
#include "Mona/SRT.h"
#if defined(SRT_API)
//#include "Mona/SRT/SRTSession.h"
#include "Mona/TSReader.h"

namespace Mona {

struct SRTProtocol : Protocol, virtual Object {
	typedef Event<void(const shared<Socket>& pSocket)>  ON(Connection);
	//typedef Socket::OnFlush							ON(Flush);
	typedef Socket::OnError								ON(Error);

	typedef Socket::OnReceived							ON(Received);
	typedef Socket::OnFlush								ON(Flush);
	typedef Socket::OnDisconnection						ON(Disconnection);

	SRTProtocol(const char* name, ServerAPI& api, Sessions& sessions) : Protocol(name, api, sessions), _bound(false) {
		onConnection = [this](const shared<Socket>& pSocket) {
			NOTE("New connection from ", pSocket->id(), " ; address : ", pSocket->peerAddress());

			Exception ex;
			bool res = this->api.ioSocket.subscribe(ex, pSocket, newDecoder(this->api), onReceived, onFlush, onError, onDisconnection);
			NOTE("Subscription result : ", res, ex);
		};
		setNumber("port", 4900);
	}
	~SRTProtocol() { 
		close();
	}

	virtual bool load(Exception& ex) { return Protocol::load(ex) ? bind(ex, address) : true; }

	virtual const shared<Socket>& socket() { return _pSRT; }

private:

	void close() {
		if (_pSRT) {
			api.ioSocket.unsubscribe(_pSRT);
			_pSRT.reset();
		}
	}

	bool bind(Exception& ex, const SocketAddress& address) {
		if (_bound) {
			if (address == _pSRT->address())
				return true;
			close();
		}
		_pSRT.reset(new SRT::Socket());
		// can subscribe after bind + listen for server, no risk to miss an event
		if (_pSRT->bind(ex, address) && _pSRT->listen(ex) && api.ioSocket.subscribe(ex, _pSRT, onConnection, onError))
			return true;
		/*if (api.ioSocket.subscribe(ex, _pSRT, newDecoder(api), onReceived, onFlush, onError) && ((SRT::Socket&)*_pSRT).bindSRT(ex, address))
			return _bound = true;*/
		close(); // release resources
		return false;
	}

	virtual Socket::Decoder* newDecoder(ServerAPI& api) { 
		struct Decoder : Socket::Decoder, virtual Object {
			Decoder(ServerAPI& api) : _api(api) {
				onTSPacket = [this](TSPacket& obj) {
					_tsReader.read(obj, *_publication);
				};
				onTSReset = [this]() {
					_tsReader.flush(*_publication);
				};

				Exception ex;
				if (!(_publication = api.publish(ex, "srt"))) {
					ERROR("SRT publish: ", ex)
					return;
				}
			}

			virtual ~Decoder() {
				if (_publication) {
					_api.unpublish(*_publication);
					_publication = nullptr;
				}
			}
		private:
			void decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
				Packet packet(pBuffer); // capture pBuffer to not call onReception event (see Socket::decode comments)
				DUMP_REQUEST("SRT", packet.data(), packet.size(), address);
				if (!packet.size())
					return;

				// Push TS data to the publication (switch thread to main thread)
				_api.handler.queue(onTSPacket, packet);
			}
			Publication*	_publication;
			ServerAPI&	_api; 
			// Safe-Threaded structure to send TS data to the running publication
			struct TSPacket : Mona::Packet, virtual Mona::Object {
				TSPacket(const Mona::Packet& packet) : Packet(std::move(packet)) {}
			};
			typedef Mona::Event<void(TSPacket&)>	ON(TSPacket);
			typedef Mona::Event<void()>				ON(TSReset);

			TSReader _tsReader;
		};
		return new Decoder(api);
	}

	shared<Socket>	_pSRT;
	bool			_bound;
};


} // namespace Mona

#endif