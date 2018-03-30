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
#include "Mona/HTTP/HTTPSender.h"

namespace Mona {


struct HTTPErrorSender : HTTPSender, virtual Object {
	template <typename ...Args>
	HTTPErrorSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const char* errorCode, Args&&... args) : _code(errorCode), HTTPSender("HTTPErrorSender", pRequest, pSocket) {
		if (!_code)
			_code = HTTP_CODE_406;
		writeError(_code, std::forward<Args>(args)...);
	}

private:
	void run() { send(_code, MIME::TYPE_TEXT, "html; charset=utf-8"); }

	const char*				_code;
};


} // namespace Mona
