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

#include "Mona/WS/WSWriter.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

void WSWriter::closing(Int32 error, const char* reason) {
	if (error < 0)
		return;
	BinaryWriter& writer(*write(WS::TYPE_CLOSE));
	writer.write16(WS::ErrorToCode(error));
	if(reason)
		writer.write(reason);
}

void WSWriter::flushing() {
	for (auto& pSender : _senders)
		_session.send(pSender);
	_senders.clear();
}


void WSWriter::writeRaw(DataReader& reader) {
	UInt8 type;
	if (!reader.readNumber(type)) {
		ERROR("WebSocket content required at less a WS number type");
		return;
	}
	StringWriter writer(write(WS::Type(type))->buffer());
	reader.read(writer);
}

DataWriter& WSWriter::writeInvocation(const char* name) {
	DataWriter& invocation(writeJSON());
	invocation.writeString(name,strlen(name));
	return invocation;
}


bool WSWriter::beginMedia(const string& name) {
	writeJSON().writeString(EXPAND("@publishing"));
	return true;
}

bool WSWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	Media::Pack(*write(WS::TYPE_BINARY, packet), tag);
	return true;
}

bool WSWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	Media::Pack(*write(WS::TYPE_BINARY, packet), tag);
	return true;
}

bool WSWriter::writeData(Media::Data::Type type, const Packet& packet, bool reliable) {
	// Always JSON (exception for Data::TYPE_MEDIA)
	// binary => Audio or Video
	// JSON => Data from server/publication
	if (type == Media::Data::TYPE_MEDIA) {
		// binary => means "format" option choosen by the client (client doesn't get more of writeAudio or writeVideo)
		write(WS::TYPE_BINARY, packet);
		return true;
	}
	DataWriter& writer(writeJSON(type, packet));
	// @ => Come from publication (to avoid confusion with message from server write by user)
	if (type == Media::Data::TYPE_TEXT)
		writer.writeString(EXPAND("@text"));
	else
		writer.writeString(EXPAND("@"));
	return true;
}

bool WSWriter::writeProperties(const Media::Properties& properties) {
	// Necessary TEXT(JSON) transfer (to match data publication)
	writeJSON(properties[Media::Data::TYPE_JSON]).writeString(EXPAND("@properties"));
	return true;
}

void WSWriter::endMedia(const string& name) {
	writeJSON().writeString(EXPAND("@unpublishing"));
}


} // namespace Mona
