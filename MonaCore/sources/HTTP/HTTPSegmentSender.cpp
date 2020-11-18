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
		const Segment& segment, Parameters& properties) : _segment(segment), _properties(move(properties)),
		HTTPSender("HTTPSegmentSender", pRequest, pSocket),
		_onWrite([this](const Packet& packet) {
			if(!send(packet))
				_pWriter.reset(); // connection death! Stop subscription!
		}) {
}

void HTTPSegmentSender::run() {
	/* TODO Il me faut réécrire HTTPWriter pour avoir un systeme pour envoyer des HTTPSender en attente avec un
	onFlush/flus() etc.. pour l'utiliser ici pour envoer les medias du segment un par un et pouvoir le mettre en pause en cas
	de congestion. Le pb de ce redesign est HTTPFileSender qui est un FileDecoder, pour avoir le pointeur pSender faire certainement
	un HTTPSender::Flush(...pSender) static qui permettrait dans le cas où le sender est un File de faire les appels à 
	api.ioFile.read(pSender) et dans le cas où ça n'est pas un File utiliser le onFlush etc... */

	const char* subType;
	MIME::Type mime = MIME::Read(pRequest->path, subType);
	if(mime) {
		_pWriter = MediaWriter::New(subType);
		if (_pWriter) {
			send(HTTP_CODE_200, mime, subType, UINT64_MAX);
			// use subscription to support properties subscription
			Subscription subscription(self);
			subscription.setParams(move(_properties));
			for (const shared<const Media::Base>& pMedia : _segment) {
				subscription.writeMedia(*pMedia);
				if (!_pWriter)
					return; // connection death!
			}
			subscription.flush();
		} else
			sendError(HTTP_CODE_501, "Segment ", pRequest->path.name(), " of type ", pRequest->path.extension(), " not supported");
	} else
		sendError(HTTP_CODE_406, "Segment ", pRequest->path.name(), " with a non acceptable type ", pRequest->path.extension());
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
	_pWriter->writeAudio(track, tag, packet, _onWrite);
	return true;
}
bool HTTPSegmentSender::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	if (!_pWriter)
		return false;
	_pWriter->writeVideo(track, tag, packet, _onWrite);
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
