
#pragma once

#include "Mona/Mona.h"
#include "Mona/TCPClient.h"
#include "Mona/HTTP/HTTPDecoder.h"
#include "Mona/WS/WSWriter.h"
#include "Mona/Client.h"

namespace Mona {

struct WSClient : TCPClient, Client, virtual Object {
	typedef Event<void(DataReader& message)>	ON(Message);

	WSClient(IOSocket& io, const char* name = NULL);
	WSClient(IOSocket& io, const shared<TLS>& pTLS, const char* name = NULL);
	~WSClient() { disconnect(); }

	bool binaryData;

	const std::string& url() const { return _url; }
	UInt16 ping();


	/*!
	Connect to the WS server at peer address */
	bool connect(Exception& ex, const SocketAddress& address, const std::string& request="");
	void disconnect();


	bool send(Exception& ex, const Packet& packet, int flags = 0);

private:
	Socket::Decoder*		newDecoder();

	HTTPDecoder::OnResponse		_onResponse;
	WSDecoder::OnMessage		_onMessage;
	shared<const HTTP::Header>	_pHTTPHeader;
	std::string					_url;
	WSWriter					_writer;
};

} // namespace Mona
