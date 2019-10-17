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
#include "Mona/TCPSession.h"
#include "Mona/WS/WSWriter.h"
#include "Mona/WS/WSDecoder.h"

namespace Mona {


struct WSSession : Session, virtual Object {
	WSSession(Protocol& protocol, TCPSession& session, const shared<WSDecoder>& pDecoder);

	WSWriter	writer;

	bool		manage();
	void		flush();

private:

	void		subscribe(Exception& ex, std::string& stream, WSWriter& writer);
	void		unsubscribe();

	void		publish(Exception& ex, std::string& stream);
	void		unpublish();

	void		kill(Int32 error = 0, const char* reason = NULL);

	/// \brief Read message and call method if needed
	/// \param packet Content message to read
	void		processMessage(Exception& ex, const Packet& message, bool isBinary = false);


	WSDecoder::OnMessage	_onMessage;

	Publication*			_pPublication;
	UInt8					_track;
	Media::Type				_media;
	Media::Video::Tag		_video;
	Media::Audio::Tag		_audio;
	Media::Data::Type		_data;
	Subscription*			_pSubscription;
};


} // namespace Mona
