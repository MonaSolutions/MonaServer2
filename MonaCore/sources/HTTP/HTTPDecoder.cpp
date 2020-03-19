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
#include "Mona/URL.h"

using namespace std;

namespace Mona {

HTTPDecoder::HTTPDecoder(const Handler& handler, const string& www, const char* name) :
	_name(name), _www(MAKE_FOLDER(www)), _stage(CMD), _handler(handler), _code(0), _lastRequest(0) {
}
HTTPDecoder::HTTPDecoder(const Handler& handler, const string& www, const shared<HTTP::RendezVous>& pRendezVous, const char* name) : _pRendezVous(pRendezVous),
	_name(name), _www(MAKE_FOLDER(www)), _stage(CMD), _handler(handler), _code(0), _lastRequest(0) {
}

void HTTPDecoder::onRelease(Socket& socket) {
	if (_pWSDecoder || socket.sendTime() >= _lastRequest)
		return;
	// We have gotten a HTTP request and since nothing has been answering,
	// to avoid client like XMLHttpRequest to interpret it like a timeout and request again (no response = timeout for some browers like chrome|firefox)
	// send a "204 no content" response (204 allows 'no content-length')
	// (usefull too for HTTP RDV to signal "no meeting" after timeout!)
	Exception ignore;
	static const Packet TimeoutClose(EXPAND("HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"));
	DUMP_RESPONSE(name(socket), TimeoutClose.data(), TimeoutClose.size(), socket.peerAddress());
	socket.write(ignore, TimeoutClose);
}

void HTTPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (_stage < BODY && _pWSDecoder) {
		// has upgraded the session, so is no more a HTTP Session!
		if (!_pWSDecoder.unique())
			return _pWSDecoder->decode(pBuffer, address, pSocket);
		DEBUG(_ex.set<Ex::Protocol>("HTTP upgrade rejected")); // DEBUG level because can happen on HTTPSession close (not an error and impossible to differenciate the both by the code)
		pSocket->shutdown(); // no more reception and sending (error sending can have been sent already by HTTPDecoder handler)s
	}

	Packet packet(pBuffer); // to capture pBuffer!
	DUMP_REQUEST(name(*pSocket), packet.data(), packet.size(), address); // dump

	if (_ex) // a request error is killing the session!
		return;

	if (!addStreamData(packet, pSocket->recvBufferSize(), pSocket)) {
		ERROR(_ex.set<Ex::Protocol>("HTTP message exceeds buffer maximum ", pSocket->recvBufferSize(), " size"));
		pSocket->shutdown(); // no more reception, no response because request is not finished!
	}
}

UInt32 HTTPDecoder::onStreamData(Packet& buffer, const shared<Socket>& pSocket) {
	
	do {
/////////////////////////////////////////////////////////////////
///////////////////// PARSE HEADER //////////////////////////////
		const char* signifiant(STR buffer.data());
		const char* key(NULL);

		while (_stage<BODY) {
			if (!_pHeader) {
				// reset header var to parse
				_file.reset();
				_code = 0;
				_pHeader.set(*pSocket, _pRendezVous.operator bool());
			}

			if (buffer.size() < 2) {
				// useless to parse if we have not at less one line which  to carry \r\n\r\n!
				UInt32 rest = buffer.size()  + (key ? (STR buffer.data() - key) : (STR buffer.data() - signifiant));
				if (rest <= 0x2000)
					return rest;
				_ex.set<Ex::Protocol>("HTTP header too large (>8KB)");
				break;	
			}

			// if == '\r\n' (or '\0\n' since that it can have been modified for key/value parsing)
			if (*buffer.data()=='\r' && *(buffer.data()+1) == '\n') {

				if (_stage == LEFT) {
					// \r\n\r\n!
					// BEGIN OF FINAL PARSE
					buffer += 2;

					// Try to fix mime if no content-type with file extension!
					if (!_pHeader->mime)
						_pHeader->mime = MIME::Read(_file, _pHeader->subMime);
					
					// Upgrade session?
					if (_pHeader->connection&HTTP::CONNECTION_UPGRADE && String::ICompare(_pHeader->upgrade, "websocket") == 0) {
						if (_code && _code != 101) {
							// was response, check is a 101 switching protocol response!
							_ex.set<Ex::Permission>("Connection ", pSocket->peerAddress(), " refused");
							break;
						}
						// Fix "ws://localhost/test" in "ws://localhost/test/" to connect to "test" application even if the last "/" is forgotten
						if(!_file.isFolder())
							_pHeader->path.set(_pHeader->path, _file.name(), '/');
						_pWSDecoder.set(_handler, _name);
						_pHeader->pWSDecoder = _pWSDecoder;
						// following is WebSocket data, not read the content!
						receive(Packet::Null(), true); // flush immediatly the swithing protocol response!
						if(buffer)
							_pWSDecoder->decode(buffer, pSocket->peerAddress(), pSocket);
						return 0;
					}

					if (_pHeader->chunked) {
						_stage = CHUNKED;
						_pHeader->progressive = true;
						// force no content-length!
						_pHeader->erase("content-length");
						_length = 0;
					} else {
						_stage = BODY;
						_pHeader->progressive = !_pHeader->getNumber("content-length", _length = -1);
					}

					bool invocation = false;
					switch (_pHeader->type) {
						case HTTP::TYPE_UNKNOWN:
							if (!_code) { // else is response!
								_ex.set<Ex::Protocol>("No HTTP type");
								break;
							}
							// is response => continue as a POST request!
						case HTTP::TYPE_POST:
							if (_pHeader->mime == MIME::TYPE_VIDEO || _pHeader->mime == MIME::TYPE_AUDIO) {
								// Publish = POST + VIDEO/AUDIO
								_pReader = MediaReader::New(_pHeader->subMime);
								if (!_pReader)
									_ex.set<Ex::Unsupported>("HTTP ", _pHeader->subMime, " media unsupported");
								break;
							}
						case HTTP::TYPE_PUT:
							if (!_code && _file.extension().empty()) // else write file if PUT or append file if POST (just valid if request => !_code)
								invocation = true;
							break;
						case HTTP::TYPE_RDV:
							if (!_pRendezVous) {
								_ex.set<Ex::Protocol>("HTTP RDV doesn't activated, set to true HTTP.rendezVous configuration");
								break;
							}
							invocation = true;
							if (!_file.isFolder())
								_pHeader->path.set(_pHeader->path, _file.name(), '/');
							// do here to detect possible _ex on the socket!
							pSocket->send(_ex, NULL, 0); // to differ HTTP timeout (we have received a valid request, wait now that nothing is sending until timeout!)
							_lastRequest.update(pSocket->sendTime() + 1); // to do working 204 response (see HTTPDecoder::onRelease)
							break;
						case HTTP::TYPE_GET:
							if (_pHeader->mime)
								_file.exists(); // preload disk attributes now in the thread!
						case HTTP::TYPE_DELETE:
							invocation = true; // must not have progressive content!
							break;
						default:
							_length = 0; // ignore content-length to avoid request corruption/saturation, if a content was present the next parsing will give a fatal decoding error
					}

					if (invocation) {
						if (_length < 0)
							_length = 0; // invocation request can ommit content-length if no content
						else if (_stage == CHUNKED)
							_ex.set<Ex::Protocol>("HTTP ", HTTP::TypeToString(_pHeader->type), " invocation doesn't accept chunked transfert-encoding");
					}
					if(_length>=0)
						_pHeader->progressive = false;
					break; // END OF FINAL PARSE
				}

				// KEY = VALUE
				char* endValue(STR buffer.data());
				while (isblank(*--endValue));

				String::Scoped scoped(++endValue);
				if (_stage==VERSION) {
					if (!_code) {
						// Set request version!
						String::ToNumber(signifiant + 5, _pHeader->version);
						_pHeader->setNumber("version", _pHeader->version);
					} else // Set response code!
						_pHeader->code = _pHeader->setString("code", signifiant).c_str();
				} else {
					const char* endValue(signifiant);
					while (isblank(*--endValue)); // after the :
					while (isblank(*--endValue)); // before the :
					String::Scoped scoped(++endValue);
					if(key)
						_pHeader->set(key, signifiant);
					else
						_pHeader->set(signifiant, "");
					key = NULL;
				}

				_stage = LEFT;
				buffer += 2;
				signifiant = STR buffer.data();
				continue;
			}

			if (isspace(*buffer.data())) {
				if (_stage == CMD) {
					// by default command == GET
					size_t size(STR buffer.data() - signifiant);
					if (!size) {
						_ex.set<Ex::Protocol>("No HTTP command");
						break;
					}
					if (size > 8) {
						_ex.set<Ex::Protocol>("Invalid HTTP packet");
						break;
					}
					String::Scoped scoped(STR buffer.data());
					if (String::ICompare(signifiant, EXPAND("HTTP")) != 0) { // else is response!
						if (!(_pHeader->type = HTTP::ParseType(signifiant, _pRendezVous.operator bool()))) {
							_ex.set<Ex::Protocol>("Unknown HTTP type ", String::Data(signifiant, 3));
							break;
						}
						_pHeader->setString("type", HTTP::TypeToString(_pHeader->type));
					}
					signifiant = STR buffer.data() + 1;
					_stage = PATH;
				} else if (_stage == PATH) {
					// parse query
					size_t size = STR buffer.data() - signifiant;
					if (_pHeader->type) {
						// Request
						signifiant = URL::ParseRequest(URL::Parse(signifiant, size, _pHeader->host), size, _pHeader->path, REQUEST_FORCE_RELATIVE);
						_pHeader->query.assign(signifiant, size);
						_file.set(_www, _pHeader->path);
						if (!_pHeader->path.isFolder())
							_pHeader->path = _pHeader->path.parent();
						signifiant = STR buffer.data() + 1;
					} else {
						// Response!
						if (size != 3 || !isdigit(signifiant[0]) || !isdigit(signifiant[1]) || !isdigit(signifiant[2])) {
							_ex.set<Ex::Protocol>("Invalid HTTP code ", String::Data(signifiant, min(size, 3u)));
							break;
						}
						_code = (signifiant[0] - '0') * 100 + (signifiant[1] - '0') * 10 + (signifiant[2] - '0');
					}
					_stage = VERSION;
				} else if(signifiant == STR buffer.data())
					++signifiant; // We are on a space char, so trim it
			} else if (_stage <= VERSION) {
				if (_stage == CMD && (STR buffer.data() - signifiant) > 7) { // not a HTTP valid packet, consumes all
					_ex.set<Ex::Protocol>("Invalid HTTP packet");
					break;
				}
			} else {
				if (!key && *buffer.data() == ':') {
					// KEY
					key = signifiant;
					signifiant = STR buffer.data() + 1;
				}
				_stage = RIGHT;
			}

			++buffer;
		}; // WHILE < BODY

		if (throwError(*pSocket))
			return 0;
///////////////////// PARSE HEADER //////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
//////////////////// BODY RECEPTION /////////////////////////////

		Packet packet(buffer);

		if (_stage == CHUNKED && !_length) {
			for (;;) {
				if (packet.size() < 2)
					return buffer.size();
				if (memcmp(packet.data(), EXPAND("\r\n")) == 0)
					break;
				++packet;
			}
			if (!String::ToNumber(STR buffer.data(), packet.data() - buffer.data(), _length, BASE_16)) {
				_ex.set<Ex::Protocol>("Invalid HTTP transfer-encoding chunked size ", String::Data(STR buffer.data(), packet.data() - buffer.data()));
				throwError(*pSocket);
				return 0;
			}
			_length += 2; // wait the both \r\n at the end of payload data!
			buffer = (packet += 2); // skip chunked size and \r\n
		}

		if (_length > packet.size())
			return packet.size(); // wait, return rest to concatenate all the body or the chunk if chunked
		if (_length >= 0) {
			if (_stage == CHUNKED) {
				_length -= 2; // remove "\r\n" at the end of payload data!
				buffer += 2;
			}
			packet.shrink(UInt32(_length));
			if (_stage == BODY)
				_length = 0;
		}
		buffer += packet.size();

		// HERE if _length=0 it means end of body or chunked transfering (chunk size = 0 is the end marker of chunked transfer)

		if (_pReader) { // POST media streaming (publish)
			_pReader->read(packet, self);
			if (!_length) { // end of reception!
				if (!_pHeader) // else publication has not begun, no flush need!
					_pReader->flush(self); // send end info!
				_pReader.reset();
			} else if (_pHeader)
				receive(false, false); // "publish" command if _pReader->read has not already done it (same as reset, but reset is impossible at the beginning)
		} else if(_pHeader) {
			if(_pHeader->type == HTTP::TYPE_RDV)
				_pRendezVous->meet(_pHeader, packet, pSocket);
			else
				receive(packet, !buffer); // !buffer => flush if no more data buffered
		} else if(packet) // progressive (no more header), usefull just if has content (can be empty on end of transfer chunked)
			receive(packet, !buffer); // !buffer => flush if no more data buffered
	
		if (throwError(*pSocket, true))
			return 0;

		if (!_length)
			_stage = CMD; // to parse Header next time!
		else if(_length>0) // chunked mode!
			_length = 0;

//////////////////// BODY RECEPTION /////////////////////////////
/////////////////////////////////////////////////////////////////

	} while (buffer);

	return 0;
}

bool HTTPDecoder::throwError(Socket& socket, bool onReceive) {
	if (!_ex)
		return false;
	if (onReceive) {
		if(_code)
			_ex.set<Ex::Protocol>(name(socket), " unsupport response");
		else
			_ex.set<Ex::Protocol>(name(socket), " unsupport request");
	}
	socket.shutdown(_code ? Socket::SHUTDOWN_BOTH : Socket::SHUTDOWN_RECV);
	ERROR(_ex)
	receive(_ex); // to signal HTTPDecoder user!;
	return true;
}


} // namespace Mona
