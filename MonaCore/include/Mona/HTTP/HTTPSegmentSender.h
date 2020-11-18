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
#include "Mona/Segment.h"


namespace Mona {

/*!
Segment send */
struct HTTPSegmentSender : HTTPSender, private Media::Target, virtual Object {
	
	HTTPSegmentSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const Segment& segment, Parameters& properties);


private:
	const Path& path() const { return pRequest->path; }

	void  run();

	bool beginMedia(const std::string& name);
	bool writeProperties(const Media::Properties& properties);
	bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
	bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable);
	bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable);
	bool endMedia();

	MediaWriter::OnWrite	_onWrite;
	unique<MediaWriter>		_pWriter;
	const Segment			_segment;
	Parameters				_properties;
};


} // namespace Mona
