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

#include "Mona/HTTP/HTTPSegmentSender.h"

using namespace std;


namespace Mona {

HTTPSegmentSender::HTTPSegmentSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const Path& path, const Segment& segment, Parameters& params) : _segment(segment), _path(path),
		HTTPSender("HTTPSegmentSender", pRequest, pSocket), _subscription(self),
		_onWrite([this](const Packet& packet) {
			if(!send(packet))
				_pWriter.reset(); // connection death! Stop subscription!
		}) {
	_subscription.setParams(move(params));
	_itMedia = _segment.begin();
}


bool HTTPSegmentSender::run() {
	if (!_pWriter) {
		if (_itMedia == _segment.end()) {
			sendError(HTTP_CODE_404, "Segment ", _path.name(), " empty");
			return true;
		}
		const char* subMime = pRequest->subMime;
		MIME::Type mime = pRequest->mime;
		if (!mime)
			mime = MIME::Read(_path, subMime);
		if (!mime) {
			sendError(HTTP_CODE_406, "Segment ", _path.name(), " with a non acceptable type ", subMime);
			return true;
		}
		_pWriter = MediaWriter::New(subMime);
		if (!_pWriter) {
			sendError(HTTP_CODE_501, "Segment ", _path.name(), " not supported");
			return true;
		}
		send(HTTP_CODE_200, mime, subMime, UINT64_MAX);
	}

	// use subscription to support properties subscription
	while(_itMedia != _segment.end()) {
		if (HTTPSender::flushing())
			return false;; // socket queueing, wait!
		const Media::Base* pMedia = (_itMedia++)->get();
		_lastTime = pMedia->time(); // fix time (have to be strictly absolute, subscription is on a isolated segment)
		_subscription.writeMedia(*pMedia);
		if (!_pWriter)
			return true; // connection death!
	}
	_subscription.reset();
	return true;
}

bool HTTPSegmentSender::beginMedia(const std::string& name) {
	if (!_pWriter)
		return false;
	_pWriter->beginMedia(_onWrite);
	return true;
}
bool HTTPSegmentSender::writeProperties(const Media::Properties& properties) {
	if (!_pWriter)
		return false;
	_pWriter->writeProperties(properties, _onWrite);
	return true;
}
bool HTTPSegmentSender::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	if (!_pWriter)
		return false;
	// fix time (have to be strictly absolute, subscription is on a isolated segment)
	_pWriter->writeAudio(track, Media::Audio::Tag(tag, _lastTime), packet, _onWrite);
	return true;
}
bool HTTPSegmentSender::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	if (!_pWriter)
		return false;
	// fix time (have to be strictly absolute, subscription is on a isolated segment)
	_pWriter->writeVideo(track, Media::Video::Tag(tag, _lastTime), packet, _onWrite);
	return true;
}
bool HTTPSegmentSender::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) {
	if (!_pWriter)
		return false;
	_pWriter->writeData(track, type, packet, _onWrite);
	return true;
}
bool HTTPSegmentSender::endMedia() {
	if (!_pWriter)
		return false;
	_pWriter->endMedia(_onWrite);
	return true;
}

} // namespace Mona
