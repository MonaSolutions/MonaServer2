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

#include "Mona/MonaReader.h"
#include "Mona/JSONReader.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

bool MonaReader::Read(const Packet& packet, Media::Source& source) {
	BinaryReader reader(packet.data(), packet.size());
	UInt8 track;
	Media::Audio::Tag audio;
	Media::Video::Tag video;
	Media::Data::Type  data;
	Media::Type type = Media::Unpack(reader, audio, video, data, track);
	Packet media(packet, reader.current(), reader.available());
	switch (type) {
		case Media::TYPE_AUDIO:
			source.writeAudio(audio, media, track);
			return true;
		case Media::TYPE_VIDEO:
			source.writeVideo(video, media, track);
			return true;
		case Media::TYPE_DATA: {
			if (!track && data == Media::Data::TYPE_JSON) {
				// properties?
				string handler;
				if (JSONReader(media).readString(handler) && handler == "@properties") {
					source.addProperties(0, data, media);
					return true;
				}
			}
			source.writeData(data, media, track);
			return true;
		}
		default:
			ERROR("Unknown type ", type);
	}
	return false;
}

void MonaReader::onFlush(Packet& buffer, Media::Source& source) {
	_size = 0;
	MediaReader::onFlush(buffer, source);
}

UInt32 MonaReader::parse(Packet& buffer, Media::Source& source) {

	BinaryReader reader(buffer.data(), buffer.size());

	while (reader.available()) {
		if (!_size) {
			// reader content size
			if (reader.available() < 4)
				return reader.available();
			_size = reader.read32();
		}
		if (reader.available() < _size)
			return reader.available();

		Read(Packet(buffer, reader.current(), _size), source);
		reader.next(_size);
		_size = 0;
		
	}
	return 0;
}


} // namespace Mona
