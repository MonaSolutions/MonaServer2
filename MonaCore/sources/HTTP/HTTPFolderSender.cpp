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

#include "Mona/HTTP/HTTPFolderSender.h"

using namespace std;


namespace Mona {

HTTPFolderSender::HTTPFolderSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
	const Path& folder, Parameters& properties) : HTTPSender("HTTPFolderSender", pRequest, pSocket), _folder(folder), _properties(move(properties)) {
}

void HTTPFolderSender::run() {
	// FOLDER
	if (!_folder.exists()) {
		sendError(HTTP_CODE_404, "The requested URL ", pRequest->path, "/ not found on the server");
		return;
	}

	HTTP::Sort		sort(HTTP::SORT_ASC);
	HTTP::SortBy	sortBy(HTTP::SORTBY_NAME);

	if (_properties.count()) {
		string buffer;
		if (_properties.getString("N", buffer))
			sortBy = HTTP::SORTBY_NAME;
		else if (_properties.getString("M", buffer))
			sortBy = HTTP::SORTBY_MODIFIED;
		else if (_properties.getString("S", buffer))
			sortBy = HTTP::SORTBY_SIZE;
		if (buffer == "D")
			sort = HTTP::SORT_DESC;
	}

	bool success;
	Exception ex;
	BinaryWriter writer(buffer());
	AUTO_ERROR(success = HTTP::WriteDirectoryEntries(ex, writer, _folder, pRequest->path, sortBy, sort), "HTTP Folder view");
	if (success)
		send(HTTP_CODE_200, MIME::TYPE_TEXT, "html; charset=utf-8");
	else
		sendError(HTTP_CODE_500, "List folder files, ", ex);
}


} // namespace Mona
