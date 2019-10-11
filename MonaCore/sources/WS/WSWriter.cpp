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
		_client.send(pSender);
	_senders.clear();
}


void WSWriter::writeRaw(DataReader& arguments, const Packet& packet) {
	UInt8 type(WS::TYPE_BINARY);
	arguments.readNumber(type);
	WSSender* pSender = newSender(WS::Type(type), packet);
	if (!pSender || !arguments.available())
		return;
	StringWriter<> writer(pSender->writer()->buffer());
	arguments.read(writer);
}

DataWriter& WSWriter::writeInvocation(const char* name) {
	DataWriter& invocation(writeJSON());
	invocation.writeString(name,strlen(name));
	return invocation;
}

bool WSWriter::beginMedia(const string& name) {
	DataWriter& writer = writeJSON();
	writer.writeString(EXPAND("@media"));
	writer.writeString(name.data(), name.size());
	return !closed();
}

bool WSWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	Media::Pack(*write(WS::TYPE_BINARY, packet), tag);
	return !closed();
}

bool WSWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	Media::Pack(*write(WS::TYPE_BINARY, packet), tag);
	return !closed();
}

bool WSWriter::writeData(Media::Data::Type type, const Packet& packet, bool reliable) {
	// Always JSON (exception for Data::TYPE_MEDIA)
	// binary => Audio or Video
	// JSON => Data from server/publication
	if (type == Media::Data::TYPE_MEDIA) {
		// binary => means "format" option choosen by the client (client doesn't get more of writeAudio or writeVideo)
		newSender(WS::TYPE_BINARY, packet);
		return !closed();
	}
	DataWriter& writer(writeJSON(type, packet));
	// @ => Come from publication (to avoid confusion with message from server write by user)
	if (type == Media::Data::TYPE_TEXT)
		writer.writeString(EXPAND("@text"));
	else
		writer.writeString(EXPAND("@"));
	return !closed();
}

bool WSWriter::writeProperties(const Media::Properties& properties) {
	// Necessary TEXT(JSON) transfer (to match data publication)
	Media::Data::Type type(Media::Data::TYPE_JSON);
	writeJSON(properties.data(type)).writeString(EXPAND("@properties"));
	return !closed();
}

bool WSWriter::endMedia() {
	writeJSON().writeString(EXPAND("@end"));
	return !closed();
}


} // namespace Mona
