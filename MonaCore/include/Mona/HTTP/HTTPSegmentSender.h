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
Media Segment send,
send with TCPSender::send(pHTTPSender) but keep a reference on until get a onFlush,
If after onFlush the pHTTPSender.unique() && !pHTTPSender->flushing() the media has been fully sent, otherwise recall TCPSender::send(pHTTPSender) */
struct HTTPSegmentSender : HTTPSender, private Media::Target, virtual Object {
	
	HTTPSegmentSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const Path& path, const Segment& segment, Parameters& params);

	const Path& path() const override { return _path; }

private:

	bool run() override;

	bool beginMedia(const std::string& name) override;
	bool writeProperties(const Media::Properties& properties) override;
	bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) override;
	bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) override;
	bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) override;
	bool endMedia() override;

	MediaWriter::OnWrite	_onWrite;
	unique<MediaWriter>		_pWriter;
	const Segment			_segment;
	Segment::const_iterator _itMedia;
	Subscription			_subscription; // use subscription to support properties subscription
	Path					_path;
	UInt32					_lastTime;
};


} // namespace Mona
