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

#include "Mona/HTTP/HTTPDecoder.h"
#include "Mona/Util.h"

using namespace std;

namespace Mona {


void HTTPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (_ex) // a request error is killing the session!
		return;
	if (!addStreamData(Packet(pBuffer), pSocket->recvBufferSize(), *pSocket)) {
		ERROR(_ex.set<Ex::Protocol>("HTTP message exceeds buffer maximum ", pSocket->recvBufferSize(), " size"));
		pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception (works for HTTP and Upgrade session as WebSocket)
	}
}

UInt32 HTTPDecoder::onStreamData(Packet& buffer, UInt32 limit, Socket& socket) {
	if (_pUpgradeDecoder)
		return _pUpgradeDecoder->onStreamData(buffer, limit, socket);

	// Dump just one time
	if (buffer.size() > _decoded) {
		DUMP_REQUEST(socket.isSecure() ? "HTTPS" : "HTTP", buffer.data()+ _decoded, buffer.size()- _decoded, socket.peerAddress());
		_decoded = buffer.size();
	}

	do {
		/// read data
		char* signifiant(STR buffer.data());
		char* key(NULL);

		while (_stage<BODY) {

			if (buffer.size() < 2) {
				// useless to parse if we have not at less one line which  to carry \r\n\r\n!
				UInt32 rest(key ? (STR buffer.data() - key) : (STR buffer.data() - signifiant));
				_decoded += rest;
				if (rest > 0x2000) {
					_ex.set<Ex::Protocol>("HTTP header too large (>8KB)");
					break;
				}
				return rest;
			}

			if (!_pHeader)
				_pHeader.reset(new HTTP::Header(socket.isSecure() ? "https" : "http", socket.address()));
	
			// if == '\r\n' (or '\0\n' since that it can have been modified for key/value parsing)
			if (*buffer.data()=='\r' && *(buffer.data()+1) == '\n') {

				if (_stage == LEFT) {
					// \r\n\r\n!
					buffer += 2;
					_decoded -= 2;

					// Try to fix mime if no content-type with file extension!
					if (!_pHeader->mime)
						_pHeader->mime = MIME::Read(_file, _pHeader->subMime);
	
					_pHeader->getNumber("content-length", _length = -1);

					// Upgrade session?
					if (_pHeader->connection&HTTP::CONNECTION_UPGRADE && String::ICompare(_pHeader->upgrade, "websocket") == 0) {
						_pHeader->pWSDecoder.reset(new WSDecoder(_handler));
						_pUpgradeDecoder = _pHeader->pWSDecoder;
					}

					_stage = BODY;
					switch (_pHeader->type) {
						case HTTP::TYPE_UNKNOWN:
							_ex.set<Ex::Protocol>("No HTTP type");
							break;
						case HTTP::TYPE_POST:
							if (_pHeader->mime != MIME::TYPE_VIDEO && _pHeader->mime != MIME::TYPE_AUDIO) {
								if(_length<0)
									_ex.set<Ex::Protocol>("HTTP post request without content-length (authorized just for POST media streaming or PUT request)");
								break;
							}
							// Publish = POST + VIDEO/AUDIO
							_pReader = MediaReader::New(_pHeader->subMime);
							if (!_pReader)
								_ex.set<Ex::Unsupported>("HTTP ", _pHeader->subMime, " publication unsupported");
						case HTTP::TYPE_PUT:
							_stage = PROGRESSIVE;
							break;
						case HTTP::TYPE_GET:
							if (_pHeader->mime == MIME::TYPE_VIDEO || _pHeader->mime == MIME::TYPE_AUDIO)
								_file.exists(); // preload disk attributes now in the thread!
						default:
							_length = 0;
					}
					break;
				}

				// KEY = VALUE
				char* endValue(STR buffer.data());
				while (isblank(*--endValue));

				String::Scoped scoped(++endValue);
				if (!key) { // version case!
					String::ToNumber(signifiant + 5, _pHeader->version);
					_pHeader->setNumber("version", _pHeader->version);
				} else {
					char* endValue(signifiant-1);
					while (isblank(*--endValue)); // after the :
					while (isblank(*--endValue)); // before the :
					String::Scoped scoped(++endValue);
					_pHeader->set(key, signifiant);
					key = NULL;
				}
		
				_stage = LEFT;
				buffer += 2;
				_decoded -= 2;
				signifiant = STR buffer.data();
				continue;
			}

			if ((_stage == LEFT || _stage == CMD || _stage == PATH) && isspace(*buffer.data())) {
				if (_stage == CMD) {
					// by default command == GET
					size_t size(STR buffer.data() - signifiant);
					if (!size) {
						_ex.set<Ex::Protocol>("No HTTP command");
						break;
					}
					if (!(_pHeader->type = HTTP::ParseType(signifiant, size))) {
						_ex.set<Ex::Protocol>("Unknown HTTP type ", string(signifiant, 3));
						break;
					}
					signifiant = STR buffer.data();
					_stage = PATH;
				} else if (_stage == PATH) {
					// parse query
					String::Scoped scoped(STR buffer.data());
					size_t filePos = Util::UnpackUrl(signifiant, _pHeader->path, _pHeader->query);
					if (filePos != string::npos) {
						// is file!
						_file.set(_www, _pHeader->path);
						_pHeader->path.erase(filePos - 1);
					} else
						_file.set(_www, _pHeader->path, '/');
					signifiant = STR buffer.data();
					_stage = VERSION;
				}
				// We are on a space char, so trim it
				++signifiant;
			} else if (_stage <= VERSION) {
				if (_stage == CMD && (STR buffer.data() - signifiant) > 7) { // not a HTTP valid packet, consumes all
					_ex.set<Ex::Protocol>("Invalid HTTP packet");
					break;
				}
			} else if (!key && *buffer.data() == ':') {
				// KEY
				key = signifiant;
				_stage = LEFT;
				signifiant = STR buffer.data() + 1;
			} else
				_stage = RIGHT;

			--_decoded;
			++buffer;
		}; // WHILE < BODY


		// RECEPTION

		if (_ex) {
			socket.shutdown(Socket::SHUTDOWN_RECV); // no more reception!
			// receive exception to warn the client
			ERROR(_ex)
			receive(_pHeader, _ex);
			return 0;
		}


		Packet packet(buffer);

		if(_length>=0) {
			if (_length > packet.size()) {
				if (_stage == BODY)
					return packet.size(); // wait (return rest, concatenate all the body)	
			} else
				packet.shrink(UInt32(_length));
			_length -= packet.size();
		}
		if(!_length)
			_stage = CMD;
	
		_decoded -= packet.size();
		buffer += packet.size();

		if (_pReader) { // POST media streaming (publish)
			_pReader->read(packet, *this);
			if (!_length) {
				_pReader->flush(*this);
				delete _pReader;
				_pReader = NULL;
			}
		} else
			receive(_pHeader, _file, packet, !buffer); // flush if no more data buffered

	} while (buffer);

	return 0;
}


} // namespace Mona
