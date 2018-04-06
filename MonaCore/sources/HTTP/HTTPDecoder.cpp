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


HTTPDecoder::HTTPDecoder(const Handler& handler, const Path& path, const string& name) :
	_name(name), _www(path), _stage(CMD), _handler(handler), _code(0), _lastRequest(0) {
	FileSystem::MakeFile(_www);
}
HTTPDecoder::HTTPDecoder(const Handler& handler, const Path& path, const shared<HTTP::RendezVous>& pRendezVous, const string& name) : _pRendezVous(pRendezVous),
	_name(name), _www(path), _stage(CMD), _handler(handler), _code(0), _lastRequest(0) {
	FileSystem::MakeFile(_www);
}

void HTTPDecoder::onRelease(Socket& socket) {
	if (_pUpgradeDecoder || socket.sendTime() >= _lastRequest)
		return;
	// We have gotten a HTTP request and since nothing has been answering,
	// to avoid client like XMLHttpRequest to interpret it like a timeout and request again (no response = timeout for some browers like chrome|firefox)
	// send a "204 no content" response (204 allows 'no content-length')
	// (usefull too for HTTP RDV to signal "no meeting" after timeout!)
	Exception ignore;
	static const Packet TimeoutClose(EXPAND("HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"));
	DUMP_RESPONSE(socket.isSecure() ? "HTTPS" : "HTTP", TimeoutClose.data(), TimeoutClose.size(), socket.peerAddress());
	socket.write(ignore, TimeoutClose);
}

void HTTPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (_stage < BODY && _pUpgradeDecoder) {
		// has upgraded the session, so is no more a HTTP Session!
		if (!_pUpgradeDecoder.unique())
			return _pUpgradeDecoder->decode(pBuffer, address, pSocket);
		DEBUG(_ex.set<Ex::Protocol>("HTTP upgrade rejected")); // DEBUG level because can happen on HTTPSession close (not an error and impossible to differenciate the both by the code)
		pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception
	}

	Packet packet(pBuffer); // to capture pBuffer!
	DUMP_REQUEST(_name.empty() ? (pSocket->isSecure() ? "HTTPS" : "HTTP") : _name.c_str(), packet.data(), packet.size(), address); // dump

	if (_ex) // a request error is killing the session!
		return;

	if (!addStreamData(packet, pSocket->recvBufferSize(), pSocket)) {
		ERROR(_ex.set<Ex::Protocol>("HTTP message exceeds buffer maximum ", pSocket->recvBufferSize(), " size"));
		pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception
	}
}

UInt32 HTTPDecoder::onStreamData(Packet& buffer, const shared<Socket>& pSocket) {
	
	do {
/////////////////////////////////////////////////////////////////
///////////////////// PARSE HEADER //////////////////////////////
		char* signifiant(STR buffer.data());
		char* key(NULL);

		while (_stage<BODY) {
			if (buffer.size() < 2) {
				// useless to parse if we have not at less one line which  to carry \r\n\r\n!
				UInt32 rest(key ? (STR buffer.data() - key) : (STR buffer.data() - signifiant));
				if (rest <= 0x2000)
					return rest;
				_ex.set<Ex::Protocol>("HTTP header too large (>8KB)");
				break;	
			}

			if (!_pHeader) {
				// reset header var to parse
				_path.reset();
				_code = 0;
				_pHeader.reset(new HTTP::Header(*pSocket, _pRendezVous.operator bool()));
			}
	
			// if == '\r\n' (or '\0\n' since that it can have been modified for key/value parsing)
			if (*buffer.data()=='\r' && *(buffer.data()+1) == '\n') {

				if (_stage == LEFT) {
					// \r\n\r\n!
					// BEGIN OF FINAL PARSE
					buffer += 2;

					// Try to fix mime if no content-type with file extension!
					if (!_pHeader->mime)
						_pHeader->mime = MIME::Read(_path, _pHeader->subMime);
	
					_pHeader->getNumber("content-length", _length = -1);

					// Upgrade session?
					if (_pHeader->connection&HTTP::CONNECTION_UPGRADE && String::ICompare(_pHeader->upgrade, "websocket") == 0) {
						// Fix "ws://localhost/test" in "ws://localhost/test/" to connect to "test" application even if the last "/" is forgotten
						if(!_path.isFolder()) {
							_pHeader->path += '/';
							_pHeader->path.append(_path.name());
						}
						_pHeader->pWSDecoder.reset(new WSDecoder(_handler));
						_pUpgradeDecoder = _pHeader->pWSDecoder;
					}

					if (_pHeader->encoding == HTTP::ENCODING_CHUNKED) {
						_length = 0; // ignore content-length
						_stage = CHUNKED;
					} else
						_stage = BODY;
					switch (_pHeader->type) {
						case HTTP::TYPE_UNKNOWN:
							if(!_code) {
								_ex.set<Ex::Protocol>("No HTTP type");
								break;
							} // else is response!
						case HTTP::TYPE_POST:
							if (_pHeader->mime != MIME::TYPE_VIDEO && _pHeader->mime != MIME::TYPE_AUDIO) {
								if(_length<0)
									_ex.set<Ex::Protocol>("HTTP post request without content-length (authorized just for POST media streaming or PUT request)");
								break;
							}
							// Publish = POST + VIDEO/AUDIO
							_pReader.reset(MediaReader::New(_pHeader->subMime));
							if (!_pReader)
								_ex.set<Ex::Unsupported>("HTTP ", _pHeader->subMime, " media unsupported");
						case HTTP::TYPE_PUT:
							if(_stage==BODY)
								_stage = PROGRESSIVE;
							break;
						case HTTP::TYPE_RDV:
							if (_length < 0)
								_length = 0; // RDV request can ommit content-length if no content-length!
							if (!_pRendezVous) {
								_ex.set<Ex::Protocol>("HTTP RDV doesn't activated, set to true HTTP.rendezVous configuration");
								break;
							}
							if (_stage == CHUNKED) {
								_ex.set<Ex::Protocol>("HTTP RDV doesn't accept chunked transfert-encoding");
								break;
							}
							if (!_path.isFolder()) {
								_pHeader->path += '/';
								_pHeader->path.append(_path.name());
							}
							// do here to detect possible _ex on the socket!
							pSocket->send(_ex, NULL, 0); // to differ HTTP timeout (we have received a valid request, wait now that nothing is sending until timeout!)
							_lastRequest.update(pSocket->sendTime() + 1); // to do working 204 response (see HTTPDecoder::onRelease)
							break;
						case HTTP::TYPE_GET:
							if (_pHeader->mime == MIME::TYPE_TEXT || _pHeader->mime == MIME::TYPE_VIDEO || _pHeader->mime == MIME::TYPE_AUDIO)
								_path.exists(); // preload disk attributes now in the thread!
						default:
							_length = 0; // ignore content-length to avoid request corruption/saturation, if a content was present the next parsing will give a fatal decoding error
					}
					break;
					// END OF FINAL PARSE
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
					char* endValue(signifiant);
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
					if (!String::ICompare(signifiant, EXPAND("HTTP")) == 0 && !(_pHeader->type = HTTP::ParseType(signifiant, _pRendezVous.operator bool()))) {
						_ex.set<Ex::Protocol>("Unknown HTTP type ", string(signifiant, 3));
						break;
					}
					signifiant = STR buffer.data() + 1;
					_stage = PATH;
				} else if (_stage == PATH) {
					// parse query
					String::Scoped scoped(STR buffer.data());
					if (_pHeader->type) {
						// Request
						size_t filePos = Util::UnpackUrl(signifiant, _pHeader->path, _pHeader->query);
						if (filePos != string::npos) {
							// is file!
							_path.set(_www, _pHeader->path);
							_pHeader->path.erase(filePos - 1);
						} else
							_path.set(_www, _pHeader->path, '/');
						signifiant = STR buffer.data() + 1;
					} else {
						// Response!
						size_t size = strlen(signifiant);
						if (size != 3 || !isdigit(signifiant[0]) || !isdigit(signifiant[1]) || !isdigit(signifiant[2])) {
							_ex.set<Ex::Protocol>("Invalid HTTP code ", string(signifiant, min(size, 3u)));
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

///////////////////// PARSE HEADER //////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
//////////////////// BODY RECEPTION /////////////////////////////

		Packet packet(buffer.buffer(), buffer.data(), buffer.size());

		if (!_length && !_ex && _stage == CHUNKED) {
			for (;;) {
				if (packet.size() < 2)
					return buffer.size();
				if (memcmp(packet.data(), EXPAND("\r\n")) != 0) {
					++packet;
					continue;
				}
				if(buffer.data()!=packet.data())
					break; // at less one char gotten (to accept double \r\n\r\n and \r\n after playload data!)
				packet += 2;
				buffer += 2;
			}
			if (String::ToNumber(STR buffer.data(), packet.data() - buffer.data(), _length, BASE_16)) {
				if (_length == 0) // end of chunked transfer-encoding
					_stage = CMD; // to parse Header next time!
			} else
				_ex.set<Ex::Protocol>("Invalid HTTP transfer-encoding chunked size ", String::Data(STR buffer.data(), packet.data() - buffer.data()));
			buffer = (packet += 2); // skip chunked size and \r\n
		}

		if (_ex) {
			// No more message if was a response, otherwise no more reception (allow a error response)
			pSocket->shutdown(_pHeader && _pHeader->type ? Socket::SHUTDOWN_RECV : Socket::SHUTDOWN_BOTH);
			// receive exception to warn the client
			ERROR(_ex)
			receive(_ex);
			return 0;
		}
		if(_length>=0) {
			if (_length > packet.size()) {
				if (_stage == BODY)
					return packet.size(); // wait (return rest, concatenate all the body)	
			} else
				packet.shrink(UInt32(_length));
			_length -= packet.size();
		}

		buffer += packet.size();

		if (_pReader) { // POST media streaming (publish)
			_pReader->read(packet, *this);
			packet = nullptr;
			if (_length) {
				if(_pHeader)
					receive(false, false); // "publish" command if _pReader->read has not already done it (same as reset, but reset is impossible at the beginning)
			} else if(_pHeader) { // else publication has not begun!
				_pReader->flush(*this); // send end info!
				_pReader.reset();
			}
		} else if (_stage != CMD) { // can be CMD on end of chunked transfer-encoding
			if (_pHeader && _pHeader->type == HTTP::TYPE_RDV)
				_pRendezVous->meet(_pHeader, packet, pSocket);
			else
				receive(packet, !buffer); // !buffer => flush if no more data buffered
		}

		if (_stage!=CHUNKED && !_length)
			_stage = CMD; // to parse Header next time!

//////////////////// BODY RECEPTION /////////////////////////////
/////////////////////////////////////////////////////////////////

	} while (buffer);

	return 0;
}


} // namespace Mona
