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
	_name(name), _pReader(NULL), _www(path), _stage(CMD), _handler(handler), _code(0) {
	FileSystem::MakeFile(_www);
}

void HTTPDecoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	// Dump just one time
	DUMP_REQUEST(_name.empty() ? (pSocket->isSecure() ? "HTTPS" : "HTTP") : _name.c_str(), pBuffer->data(), pBuffer->size(), address);

	if (_ex) // a request error is killing the session!
		return;

	if (_stage < BODY && _pUpgradeDecoder) {
		if (_pUpgradeDecoder.unique()) {
			ERROR(_ex.set<Ex::Protocol>("HTTP upgrade rejected"));
			pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception
			return;
		}
		return _pUpgradeDecoder->decode(pBuffer, address, pSocket);
	}

	if (!addStreamData(Packet(pBuffer), pSocket->recvBufferSize(), *pSocket)) {
		ERROR(_ex.set<Ex::Protocol>("HTTP message exceeds buffer maximum ", pSocket->recvBufferSize(), " size"));
		pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception
	}
}

UInt32 HTTPDecoder::onStreamData(Packet& buffer, Socket& socket) {
	
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
				_path.reset();
				_code = 0;
				_pHeader.reset(new HTTP::Header(socket.isSecure() ? "https" : "http", socket.address()));
			}
	
			// if == '\r\n' (or '\0\n' since that it can have been modified for key/value parsing)
			if (*buffer.data()=='\r' && *(buffer.data()+1) == '\n') {

				if (_stage == LEFT) {
					// \r\n\r\n!
					buffer += 2;

					// Try to fix mime if no content-type with file extension!
					if (!_pHeader->mime)
						_pHeader->mime = MIME::Read(_path, _pHeader->subMime);
	
					_pHeader->getNumber("content-length", _length = -1);

					// Upgrade session?
					if (_pHeader->connection&HTTP::CONNECTION_UPGRADE && String::ICompare(_pHeader->upgrade, "websocket") == 0) {
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
							_pReader = MediaReader::New(_pHeader->subMime);
							if (!_pReader)
								_ex.set<Ex::Unsupported>("HTTP ", _pHeader->subMime, " media unsupported");
						case HTTP::TYPE_PUT:
							if(_stage==BODY)
								_stage = PROGRESSIVE;
							break;
						case HTTP::TYPE_GET:
							if (_pHeader->mime == MIME::TYPE_VIDEO || _pHeader->mime == MIME::TYPE_AUDIO)
								_path.exists(); // preload disk attributes now in the thread!
						default:
							_length = 0;
					}
					break;
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
					if (!String::ICompare(signifiant, EXPAND("HTTP")) == 0 && !(_pHeader->type = HTTP::ParseType(signifiant))) {
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

		Packet packet(buffer);

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
			socket.shutdown(_pHeader && _pHeader->type ? Socket::SHUTDOWN_RECV : Socket::SHUTDOWN_BOTH);
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
				delete _pReader;
				_pReader = NULL;
			}
		} else if(_stage != CMD) // can be CMD on end of chunked transfer-encoding
			receive(packet, !buffer); // !buffer => flush if no more data buffered

		if (_stage!=CHUNKED && !_length)
			_stage = CMD; // to parse Header next time!

//////////////////// BODY RECEPTION /////////////////////////////
/////////////////////////////////////////////////////////////////

	} while (buffer);

	return 0;
}


} // namespace Mona
