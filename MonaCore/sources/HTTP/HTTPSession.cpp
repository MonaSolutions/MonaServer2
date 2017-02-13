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

#include "Mona/HTTP/HTTPSession.h"
#include "Mona/MapWriter.h"
#include "Mona/QueryReader.h"
#include "Mona/StringReader.h"
#include "Mona/WS/WSSession.h"


using namespace std;


namespace Mona {


HTTPSession::HTTPSession(Protocol& protocol) : _pUpgradeSession(NULL), TCPSession(protocol), _pSubscription(NULL), _pPublication(NULL), _indexDirectory(true), _writer(*this),
	_onRequest([this](HTTP::Request& request) {
		if (request) { // else progressive! => PUT or POST media!

			_writer.beginRequest(request);

			if (!request.ex) {
				// HTTP is a simplex communication, so if request, remove possible old subscription or pubication (but should never happen because a request/response without content-length should be stopped just by a disconnection)
				closePublication();
				closeSusbcription();

				//// Disconnection if requested or query changed (path changed is done in setPath)
				if (request->connection&HTTP::CONNECTION_UPDATE || String::ICompare(peer.query, request->query) != 0)
					peer.onDisconnection();

				////  Fill peers infos
				peer.setPath(request->path);
				peer.setQuery(request->query);
				peer.setServerAddress(request.serverAddress);
				// properties = version + headers + cookies
				peer.properties() = move(request);

				// Create parameters for onConnection or a GET onRead/onWrite/onInvocation
				QueryReader parameters(peer.query.data(), peer.query.size());

				Exception ex;

				// Upgrade protocol
				if (request->connection&HTTP::CONNECTION_UPGRADE) {
					// Upgrade to WebSocket
					if (request->pWSDecoder) {
						Protocol* pProtocol(this->api.protocols.find(socket()->isSecure() ? "WSS" : "WS"));
						if (pProtocol) {
							// Close HTTP
							close();
							// HTTP disconnection
							peer.onDisconnection();
							// Create WSSession
							WSSession* pSession = new WSSession(*pProtocol, *this, request->pWSDecoder);
							_pUpgradeSession = pSession;
							HTTP_BEGIN_HEADER(_writer.writeRaw(HTTP_CODE_101)) // "101 Switching Protocols"
								HTTP_ADD_HEADER("Upgrade", EXPAND("WebSocket"))
								WS::WriteKey(__writer.write(EXPAND("Sec-WebSocket-Accept: ")), request->secWebsocketKey).write("\r\n");
							HTTP_END_HEADER

								// SebSocket connection
								peer.onConnection(ex, pSession->writer, *socket(), parameters); // No response in WebSocket handshake (fail for few clients)
						} else
							ex.set<Ex::Unavailable>("Unavailable ", request->upgrade, " protocol");
					} else
						ex.set<Ex::Unsupported>("Unsupported ", request->upgrade, " upgrade");
				} else {

					// onConnection
					if (!peer.connected) {
						peer.onConnection(ex, _writer, *socket(), parameters);
						parameters.reset();
					}

					// onInvocation/<method>/onRead
					if (!ex && peer.connected) {

						/// HTTP GET & HEAD
						switch (request->type) {
						case HTTP::TYPE_HEAD:
						case HTTP::TYPE_GET:
							processGet(ex, request, parameters);
							break;
						case HTTP::TYPE_POST:
						{
							processPost(ex, request);
							break;
						}
						case HTTP::TYPE_OPTIONS: // happen when Move Redirection is sent
							processOptions(ex, *request);
							break;
						case HTTP::TYPE_PUT:
							processPut(ex, request);
							break;
						default:
							ex.set<Ex::Protocol>(name(), ", unsupported command");
						}
					}
				}


				// Return error but keep connection keepalive if required to avoid multiple reconnection (and because HTTP client is valid contrary to a HTTPDecoder error)
				if (ex)
					_writer.writeError(ex);

			}
			_writer.endRequest();
		}

		// Invalid packet, answer with the appropriate response and useless to keep the session opened!
		// Do it after beginRequest/endRequest to get permission to write error response
		if (request.ex)
			return kill(ToError(request.ex), request.ex.c_str());

		if (_pPublication) {
			// Something to post!
			// POST partial data => push to publication
			if (request.pMedia) {
				if (request.lost) {
					if (request.lost>0)
						_pPublication->reportLost(request.pMedia->type, request.pMedia->track, request.lost);
					else
						_pPublication->reportLost(request.pMedia->type, -request.lost);
				} else
					_pPublication->writeMedia(*request.pMedia);
			} else if (request.lost) {
				if (request.flush)
					closePublication();
				else
					_pPublication->reset();
			}

		} else {
			// PUT partial data => write file

		}

		if (request.flush)
			_writer.flush();
	}) {
	
	// subscribe to client.properties(...)
	peer.onCallProperties = [this](DataReader& reader,string& value) {
		HTTP::OnCookie onCookie([this,&value](const char* key, const char* data, UInt32 size) {
			peer.properties().setString(key, data, size);
			value.assign(data,size);
		});
		return _writer.writeSetCookie(reader, onCookie);
	};

}

shared<Socket::Decoder> HTTPSession::newDecoder() {
	shared<HTTPDecoder> pDecoder(new HTTPDecoder(api));
	pDecoder->onRequest = _onRequest;
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

	if (!TCPSession::manage())
		return false;

	// check subscription
	if (_pSubscription && _pSubscription->ejected()) {
		if (_pSubscription->ejected() == Subscription::EJECTED_BANDWITDH)
			_writer.writeError(HTTP_CODE_509, "Insufficient bandwidth to play ", _pSubscription->name());
		else if(_pSubscription->ejected() == Subscription::EJECTED_TIMEOUT)
			_writer.writeError(HTTP_CODE_404, _pSubscription->name(), " not found");
		// else HTTPWriter error, error already written!
		closeSusbcription();
	}

	return true;
}

void HTTPSession::flush() {
	if (_pUpgradeSession)
		return _pUpgradeSession->flush();
	if (_pPublication)
		_pPublication->flush();
	_writer.flush(); // can flush on response while polling!
}

void HTTPSession::close() {
	peer.onCallProperties = nullptr;
	// unpublish and unsubscribe
	closePublication();
	closeSusbcription();
}

void HTTPSession::kill(Int32 error, const char* reason){
	if(died)
		return;

	// Stop reception
	_onRequest = nullptr;

	if(_pUpgradeSession)
		_pUpgradeSession->kill(error, reason);
	else
		close();

	// kill (onDisconnection) after "unpublish or unsubscribe", but BEFORE _writers.close() because call onDisconnection and writers can be used here
	TCPSession::kill(error, reason);

	// close writer (flush)
	_writer.close(error, reason);
}

void HTTPSession::openSubscribtion(Exception& ex, const string& stream, Writer& writer) {
	closeSusbcription();
	_pSubscription = new Subscription(writer);
	_pSubscription->setString("mode", "play");
	if (api.subscribe(ex, stream, peer, *_pSubscription, peer.query.c_str()))
		return;
	delete _pSubscription;
	_pSubscription = NULL;
}

void HTTPSession::closeSusbcription() {
	if (!_pSubscription)
		return;
	api.unsubscribe(peer, *_pSubscription);
	delete _pSubscription;
	_pSubscription = NULL;
}

void HTTPSession::openPublication(Exception& ex, const Path& stream) {
	closePublication();
	_pPublication = api.publish(ex, peer, stream, peer.query.c_str());
}

void HTTPSession::closePublication() {
	if (!_pPublication)
		return;
	api.unpublish(*_pPublication, peer);
	_pPublication = NULL;
}

void HTTPSession::processOptions(Exception& ex, const HTTP::Header& request) {
	// Control methods quiested
	if (request.accessControlRequestMethod&HTTP::TYPE_DELETE) {
		ex.set<Ex::Permission>("HTTP Deletion not allowed");
		return;
	}

	HTTP_BEGIN_HEADER(_writer.writeRaw(HTTP_CODE_200))
		HTTP_ADD_HEADER("Access-Control-Allow-Methods", EXPAND("GET, HEAD, PUT, PATH, POST, OPTIONS"))
		if (request.accessControlRequestHeaders)
			HTTP_ADD_HEADER("Access-Control-Allow-Headers", request.accessControlRequestHeaders)
		HTTP_ADD_HEADER("Access-Control-Max-Age", EXPAND("86400")) // max age of 24 hours
	HTTP_END_HEADER
}

void HTTPSession::processGet(Exception& ex, HTTP::Request& request, QueryReader& parameters) {
	// use index http option in the case of GET request on a directory

	Path& file(request.file);
	if (!file.isFolder()) {
		// FILE //


		// TODO m3u8
		// https://developer.apple.com/library/ios/technotes/tn2288/_index.html
		// CODECS => https://tools.ietf.org/html/rfc6381

		// 1 - priority on client method
		if (file.extension().empty() && peer.onInvocation(ex, file.name(), parameters)) // can be method!
			return;
	
		// 2 - try to get a file
		Parameters fileProperties;
		MapWriter<Parameters> mapWriter(fileProperties);
		if (!peer.onRead(ex, file, parameters, mapWriter)) {
			if (!ex)
				ex.set<Ex::Net::Permission>("No authorization to see the content of ", peer.path,"/",file.name());
			return;
		}
		// If onRead has been authorised, and that the file is a multimedia file, and it doesn't exists (no VOD, filePath.lastModified()==0 means "doesn't exists")
		// Subscribe for a live stream with the basename file as stream name
		if (!file.exists() && request->type == HTTP::TYPE_GET && (request->mime == MIME::TYPE_VIDEO || request->mime == MIME::TYPE_AUDIO))
			return openSubscribtion(ex, file.baseName(), _writer);
		if (!file.isFolder())
			return _writer.writeFile(file, fileProperties);
	}


	// FOLDER //
	
	// Redirect to the file (get name to prevent path insertion), if impossible (_index empty or invalid) list directory
	if (!_index.empty()) {
		if (_index.find_last_of('.') == string::npos && peer.onInvocation(ex, _index, parameters)) // can be method!
			return;
		file.set(file, _index);
	} else if (!_indexDirectory) {
		ex.set<Ex::Net::Permission>("No authorization to see the content of ", peer.path, "/");
		return;
	}

	// folder view or index redirection (without pass by onRead because can create a infinite loop)

	Parameters fileProperties;
	MapWriter<Parameters> mapWriter(fileProperties);
	parameters.read(mapWriter);
	_writer.writeFile(file, fileProperties); // folder view or index redirection (without pass by onRead because can create a infinite loop)
}

void HTTPSession::processPut(Exception& ex, HTTP::Request& request) {
	if (request) {
		// TODO peer.onWriteFile + Forbidden response if return false
	}
	// TODO Write file (add packet to current writing file) + answer OK after writing success if content-length < 0xFFFFFFFF (had content-length)
	ex.set<Ex::Unsupported>("HTTP PUT unsupported today");
}

void HTTPSession::processPost(Exception& ex, HTTP::Request& request) {
	
	if (request->mime == MIME::TYPE_VIDEO || request->mime == MIME::TYPE_AUDIO)
		return openPublication(ex, request.file); // Publish
	
	// Else data
	unique_ptr<DataReader> pReader(Media::Data::NewReader(Media::Data::ToType(request->subMime), request));
	string name;
	if (!pReader)
		pReader.reset(new StringReader(request.data(), request.size()));
	else
		pReader->readString(name);
	if (!peer.onInvocation(ex, name, *pReader))
		ERROR(ex.set<Ex::Application>("Method client ", name, " not found in application ", peer.path));
}


} // namespace Mona
