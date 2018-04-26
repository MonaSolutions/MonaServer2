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

#include "Mona/HTTP/HTTProtocol.h"
#include "Mona/MapReader.h"
#include "Mona/MapWriter.h"
#include "Mona/QueryReader.h"
#include "Mona/StringReader.h"
#include "Mona/SplitReader.h"
#include "Mona/WriterReader.h"
#include "Mona/WS/WSSession.h"


using namespace std;


namespace Mona {


HTTPSession::HTTPSession(Protocol& protocol) : TCPSession(protocol), _pSubscription(NULL), _pPublication(NULL), _indexDirectory(true), _pWriter(new HTTPWriter(*this)), _fileWriter(protocol.api.ioFile),
	_onResponse([this](HTTP::Response& response) {
		kill(ERROR_PROTOCOL, "A HTTP response has been received instead of request");
	}),
	_onRequest([this](HTTP::Request& request) {
		// Invalid packet, answer with the appropriate response and useless to keep the session opened!
		// Do it after beginRequest/endRequest to get permission to write error response
		if (request.ex)
			return kill(ToError(request.ex), request.ex.c_str());

		if (request) { // else progressive! => PUT or POST media!

			_pWriter->beginRequest(request);

			//// Disconnection if requested or query changed (path changed is done in setPath)
			if (request->connection&HTTP::CONNECTION_UPGRADE || String::ICompare(peer.query, request->query) != 0) {
				unpublish();
				unsubscribe();
				peer.onDisconnection();
			}

			////  Fill peers infos
			peer.setPath(request->path);
			peer.setQuery(request->query);
			peer.setServerAddress(request->host);
			// properties = version + headers + cookies
			peer.properties().setParams(move(request));

			// Create parameters for onConnection or a GET onRead/onWrite/onInvocation
			QueryReader parameters(peer.query.data(), peer.query.size());

			Exception ex;

			// Upgrade protocol
			if (request->connection&HTTP::CONNECTION_UPGRADE) {
				if (handshake(request))  {
					// Upgrade to WebSocket
					if (request->pWSDecoder) {
						Protocol* pProtocol(this->api.protocols.find(self->isSecure() ? "WSS" : "WS"));
						if (pProtocol) {
							// Close HTTP
							close();
		
							// Create WSSession
							WSSession* pSession = new WSSession(*pProtocol, *this, move(request->pWSDecoder));
							_pUpgradeSession.reset(pSession);
							HTTP_BEGIN_HEADER(_pWriter->writeRaw(HTTP_CODE_101)) // "101 Switching Protocols"
								HTTP_ADD_HEADER("Upgrade", "WebSocket")
								WS::WriteKey(__writer.write(EXPAND("Sec-WebSocket-Accept: ")), request->secWebsocketKey).write("\r\n");
							HTTP_END_HEADER

							// SebSocket connection
							peer.onConnection(ex, pSession->writer, *self, parameters); // No response in WebSocket handshake (fail for few clients)
						} else
							ex.set<Ex::Unavailable>("Unavailable ", request->upgrade, " protocol");
					} else
						ex.set<Ex::Unsupported>("Unsupported ", request->upgrade, " upgrade");
				}
			} else {

				// onConnection
				if (!peer && handshake(request)) {
					peer.onConnection(ex, *_pWriter, *self, parameters);
					parameters.reset();
				}

				// onInvocation/<method>/onRead
				if (!ex && peer) {

					/// HTTP GET & HEAD
					switch (request->type) {
						case HTTP::TYPE_HEAD:
						case HTTP::TYPE_GET:
							processGet(ex, request, parameters);
							break;
						case HTTP::TYPE_POST:
							processPost(ex, request, parameters);
							break;
						case HTTP::TYPE_OPTIONS: // happen when Move Redirection is sent
							processOptions(ex, *request);
							break;
						case HTTP::TYPE_PUT:
							processPut(ex, request, parameters);
							break;
						case HTTP::TYPE_DELETE:
							processDelete(ex, request, parameters);
							break;
						default:
							ex.set<Ex::Protocol>(name(), ", unsupported command");
					}
				}
			}
	
			if (!died) { // if died _pWriter is null!
				if (ex) // Return error but keep connection keepalive if required to avoid multiple reconnection (and because HTTP client is valid contrary to a HTTPDecoder error)
					_pWriter->writeError(ex);
				_pWriter->endRequest();
			}
		}

		if (_fileWriter) {
			// write file (put or post progressive)
			_fileWriter.write(request);
			if ((request && !request->progressive) || !request.size()) { // if progressive a request empty means end of content!
				// send a PUT response + close the fileWriter
				_pWriter->writeRaw(_fileWriter->exists() ? HTTP_CODE_200 : HTTP_CODE_201);
				_fileWriter.close();
			}
		} else if (!request && request.size()) // can happen on chunked request unwanted (what to do with that?)
			return kill(Session::ERROR_PROTOCOL, "Progressive request unsupported");

		if (request.pMedia) {
			// Something to post!
			// POST partial data => push to publication
			if (!_pPublication)
				return kill(Session::ERROR_REJECTED, "Publication rejected");
			if (request.lost)
				_pPublication->reportLost(request.pMedia->type, request.lost, request.pMedia->track);
			else if (request.pMedia->type)
				_pPublication->writeMedia(*request.pMedia);
			else if(!request.flush && !request) // !request => if header it's the first "publish" command!
				return _pPublication->reset();
		} else if(_pPublication)
			unpublish();

		if (request.flush && !died) // if died _pWriter is null and useless to flush!
			flush();
	}) {
	
	// subscribe to client.properties(...)
	peer.onCallProperties = [this](DataReader& reader,string& value) {
		HTTP::OnCookie onCookie([this,&value](const char* key, const char* data, UInt32 size) {
			peer.properties().setString(key, data, size);
			value.assign(data,size);
		});
		return _pWriter->writeSetCookie(reader, onCookie);
	};

	_fileWriter.onError = [this](const Exception& ex) {
		_pWriter->writeError(ex); // keep session alive, normal error!
	};
}

bool HTTPSession::handshake(HTTP::Request& request) {
	SocketAddress redirection;
	if (!peer.onHandshake(redirection))
		return true;
	HTTP_BEGIN_HEADER(_pWriter->writeRaw(HTTP_CODE_307))
		HTTP_ADD_HEADER("Location", self->isSecure() ? "https://" : "http://", redirection, request->path, '/', request.path.isFolder() ? "" : request.path.name())
	HTTP_END_HEADER
	kill();
	return false;
}

Socket::Decoder* HTTPSession::newDecoder() {
	HTTPDecoder* pDecoder = new HTTPDecoder(api.handler, api.www, protocol<HTTProtocol>().pRendezVous);
	pDecoder->onRequest = _onRequest;
	pDecoder->onResponse = _onResponse;
	return pDecoder;
}

void HTTPSession::onParameters(const Parameters& parameters) {
	TCPSession::onParameters(parameters);
	if (_pUpgradeSession)
		return _pUpgradeSession->onParameters(parameters);
	_index.clear(); // default value
	_indexDirectory = true; // default value
	if (parameters.getString("index", _index)) {
		if (String::IsFalse(_index)) {
			_indexDirectory = false;
			_index.clear();
		} else if (String::IsTrue(_index))
			_index.clear();
		else
			FileSystem::GetName(_index); // Redirect to the file (get name to prevent path insertion)
	}
}

bool HTTPSession::manage() {
	if (_pUpgradeSession)
		return _pUpgradeSession->manage();

	UInt32 timeout = this->timeout;
	// Cancel timeout when subscribing, and use rather subscribing timeout! (HTTP which has no ping feature)
	// If answering waits end of response, usefull for VLC file playing for example (which can to ::recv after a quantity of data in socket.available())
	// In HTTP we can use some "auto feature" like RDV which doesn't pass over main thread (implemented in HTTPDecoder)
	// So timeout just on nothing more is sending during timeout (send = valid HTTP response)
	if (_pSubscription || _pWriter->answering() || !self->sendTime().isElapsed(timeout))
		(UInt32&)this->timeout = 0;
		
	if (!TCPSession::manage())
		return false;
	
	// check subscription
	if (_pSubscription) {
		if (!_pSubscription->streaming(timeout)) {
			INFO(name(), " timeout connection");
			kill(ERROR_IDLE, _pSubscription->name().c_str()); // _pSubscription->name().c_str() to disable kill=ERROR_IDLE possible canceling (see HTTP::kill)
			return false;
		}
		if(_pSubscription->ejected()) {
			if (_pSubscription->ejected() == Subscription::EJECTED_BANDWITDH)
				_pWriter->writeError(HTTP_CODE_509, "Insufficient bandwidth to play ", _pSubscription->name());
			// else HTTPWriter error, error already written!
			unsubscribe();
		}
	}

	(UInt32&)this->timeout = timeout;
	return true;
}

void HTTPSession::flush() {
	_pWriter->flush(); // before upgradeSession because something wrotten on HTTP master session has to be sent (make working HTTP upgrade response)
	if (_pUpgradeSession)
		return _pUpgradeSession->flush();
	if (_pPublication)
		_pPublication->flush();
}

void HTTPSession::close() {
	peer.onCallProperties = nullptr;
	// unpublish and unsubscribe
	unpublish();
	unsubscribe();
}

void HTTPSession::kill(Int32 error, const char* reason){
	if(died)
		return;

	// Stop decoding
	_onRequest = nullptr;
	_onResponse = nullptr;

	if(_pUpgradeSession)
		_pUpgradeSession->kill(error, reason);
	else
		close();

	// onDisconnection after "unpublish or unsubscribe", but BEFORE _writers.close() to allow last message
	peer.onDisconnection();

	// close writer (flush)
	_pWriter->close(error, reason);
	
	// in last because will disconnect
	TCPSession::kill(error, reason);

	// release resources (sockets)
	_pWriter.reset();
}

void HTTPSession::subscribe(Exception& ex, const string& stream) {
	if(!_pSubscription)
		_pSubscription = new Subscription(*_pWriter);
	if (api.subscribe(ex, stream, peer, *_pSubscription, peer.query.c_str()))
		return;
	delete _pSubscription;
	_pSubscription = NULL;
}

void HTTPSession::unsubscribe() {
	if (!_pSubscription)
		return;
	api.unsubscribe(peer, *_pSubscription);
	delete _pSubscription;
	_pSubscription = NULL;
}

void HTTPSession::publish(Exception& ex, const Path& stream) {
	unpublish();
	_pPublication = api.publish(ex, peer, stream, peer.query.c_str());
}

void HTTPSession::unpublish() {
	if (!_pPublication)
		return;
	api.unpublish(*_pPublication, peer);
	_pPublication = NULL;
}

void HTTPSession::processOptions(Exception& ex, const HTTP::Header& request) {
	// Control methods quiested
	HTTP_BEGIN_HEADER(_pWriter->writeRaw(HTTP_CODE_200))
		HTTP_ADD_HEADER("Access-Control-Allow-Methods", HTTP::Types(request.rendezVous))
		if (request.accessControlRequestHeaders)
			HTTP_ADD_HEADER("Access-Control-Allow-Headers", request.accessControlRequestHeaders)
		HTTP_ADD_HEADER("Access-Control-Max-Age", "86400") // max age of 24 hours
	HTTP_END_HEADER
}

void HTTPSession::processGet(Exception& ex, HTTP::Request& request, QueryReader& parameters) {
	// use index http option in the case of GET request on a directory
	
	Path& file(request.path);
	if (!file.isFolder()) {
		// FILE //

		// TODO m3u8
		// https://developer.apple.com/library/ios/technotes/tn2288/_index.html
		// CODECS => https://tools.ietf.org/html/rfc6381

		// 1 - priority on client method
		if (file.extension().empty() && invoke(ex, request, parameters)) // can be method!
			return;
	
		// 2 - try to get a file
		Parameters fileProperties;
		MapWriter<Parameters> mapWriter(fileProperties);
		if (!peer.onRead(ex=nullptr, file, parameters, mapWriter)) {
			if (!ex)
				ex.set<Ex::Net::Permission>("No authorization to see the content of ", peer.path,"/",file.name());
			return;
		}
		// If onRead has been authorised, and that the file is a multimedia file, and it doesn't exists (no VOD)
		// Subscribe for a live stream with the basename file as stream name
		if (!file.isFolder()) {
			switch (request->mime) {
				case MIME::TYPE_TEXT:
					// subtitle?
					if (String::ICompare(file.extension(), "srt") != 0 && String::ICompare(file.extension(), "vtt") != 0)
						break;
				case MIME::TYPE_VIDEO:
				case MIME::TYPE_AUDIO:
					if (file.exists())
						break; // VOD!
					if (request->query == "?") {
						// request publication properties!
						const auto& it = api.publications.find(file.baseName());
						if (it == api.publications.end())
							_pWriter->writeError(HTTP_CODE_404, "Publication ", file.baseName(), " unfound");
						else
							MapReader<Parameters>(it->second).read(_pWriter->writeResponse("json"));
					} else if (request->type == HTTP::TYPE_HEAD) {
						// HEAD, want just immediatly the header format of audio/video live streaming!
						HTTP_BEGIN_HEADER(*_pWriter->writeResponse())
							HTTP_ADD_HEADER_LINE(HTTP_LIVE_HEADER)
						HTTP_END_HEADER
					} else
						subscribe(ex, file.baseName());
					return;
				default:;
			}
			return _pWriter->writeFile(file, fileProperties);
		}
	}

	// FOLDER //
	
	// Redirect to the file (get name to prevent path insertion), if impossible (_index empty or invalid) list directory
	if (!_index.empty()) {
		if (_index.find_last_of('.') == string::npos && invoke(ex, request, parameters, _index.c_str())) // can be method!
			return;
		ex = nullptr;
		file.set(file, _index);
	} else if (!_indexDirectory) {
		ex.set<Ex::Net::Permission>("No authorization to see the content of ", peer.path, "/");
		return;
	}

	Parameters fileProperties;
	MapWriter<Parameters> mapWriter(fileProperties);
	parameters.read(mapWriter);
	_pWriter->writeFile(file, fileProperties); // folder view or index redirection (without pass by onRead because can create a infinite loop)
}

void HTTPSession::processPost(Exception& ex, HTTP::Request& request, QueryReader& parameters) {
	if (request.pMedia)
		return publish(ex, request.path); // Publish
	processPut(ex, request, parameters); // data or file append (as behavior as PUT)
}

void HTTPSession::processPut(Exception& ex, HTTP::Request& request, QueryReader& parameters) {
	if (!request.path.extension().empty()) {
		// write file!
		Parameters properties;
		MapWriter<Parameters> props(properties);
		bool append(request->type == HTTP::TYPE_POST);
		struct AppendReader : WriterReader {
			bool write(DataWriter& writer) {
				writer.writeString(EXPAND("append"));
				return false;
			}
		} appendReader;
		SplitReader params(parameters, append ? appendReader : DataReader::Null());
		if (peer.onWrite(ex, request.path, params, props)) {
			properties.getBoolean("append", append);
			_fileWriter.open(request.path, append);
		} // else "ex" is necessary set so HTTP session will be killed!
	} else if(!invoke(ex, request, parameters)) // invoke method!
		ERROR(ex);
}

void HTTPSession::processDelete(Exception& ex, HTTP::Request& request, QueryReader& query) {
	if (!request.path.extension().empty()) {
		// delete file!
		if (peer.onDelete(ex, request.path, query)) {
			_fileWriter.erase(request.path).close();
			_pWriter->writeRaw(HTTP_CODE_200);
		} // else "ex" is necessary set so HTTP session will be killed!	
	} else if (!invoke(ex, request, query)) // invoke method!
		ERROR(ex);
}

bool HTTPSession::invoke(Exception& ex, HTTP::Request& request, QueryReader& parameters, const char* name) {
	if (!name && !request.path.isFolder())
		name = request.path.name().c_str();

	bool hasContent = request->hasKey("content-length");
	string method;
	DataReader* pReader = hasContent ? Media::Data::NewReader(Media::Data::ToType(request->subMime), request) : &parameters;
	if (!pReader)
		pReader = new StringReader(request.data(), request.size());
	else if (!name) {
		pReader->readString(method);
		name = method.c_str();
	}

	bool result = peer.onInvocation(ex, name, *pReader);
	if (hasContent)
		delete pReader;
	if (!result && !ex)
		ex.set<Ex::Application>("Method client ", name, " not found in application ", peer.path);
	return result;
}



} // namespace Mona
