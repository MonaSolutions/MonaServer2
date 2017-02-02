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

#include "Mona/Protocols.h"
#include "Mona/ServerAPI.h"


#include "Mona/RTMP/RTMProtocol.h"
#include "Mona/RTMFP/RTMFProtocol.h"
#include "Mona/HTTP/HTTProtocol.h"
#include "Mona/WS/WSProtocol.h"
//#include "Mona/RTSP/RTSProtocol.h"

using namespace std;

namespace Mona {


void Protocols::load(ServerAPI& api, Sessions& sessions) {
	loadProtocol<RTMFProtocol>("RTMFP", api, sessions);
	loadProtocol<RTMProtocol>("RTMP", api, sessions);
if(api.pTLSServer)
	loadProtocol<RTMProtocol, false>("RTMPS", api, sessions, api.pTLSServer); // disable by default
	loadProtocol<WSProtocol>("WS", loadProtocol<HTTProtocol>("HTTP", api, sessions));
if(api.pTLSServer)
	loadProtocol<WSProtocol>("WSS", loadProtocol<HTTProtocol>("HTTPS", api, sessions, api.pTLSServer));
	//loadProtocol<RTSProtocol>("RTSP", api, sessions);
}


} // namespace Mona
