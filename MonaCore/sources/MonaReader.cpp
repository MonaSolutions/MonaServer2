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
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

void MonaReader::onFlush(const Packet& packet, Media::Source& source) {
	_size = 0;
	MediaReader::onFlush(packet, source);
}

UInt32 MonaReader::parse(const Packet& packet, Media::Source& source) {

	BinaryReader reader(packet.data(), packet.size());

	while (reader.available()) {
		if (!_size) {
			// reader content size
			if (reader.available() < 4)
				return reader.available();
			_size = reader.read32();
		}
		if (reader.available() < _size)
			return reader.available();

		BinaryReader content(reader.current(), _size);
		reader.next(_size);
		_size = 0;

		UInt8 track;
		Media::Audio::Tag audio;
		Media::Video::Tag video;
		Media::Data::Type  data;
		Media::Type type = Media::Unpack(content, audio, video, data, track);
		Packet media(packet, content.current(), content.available());
		switch (type) {
			case Media::TYPE_AUDIO:
				source.writeAudio(track, audio, media);
				break;
			case Media::TYPE_VIDEO:
				source.writeVideo(track, video, media);
				break;
			case Media::TYPE_DATA:
				source.writeData(track, data, media);
				break;
			default:
				ERROR("Malformed header size");
		}
	}
	return 0;
}


} // namespace Mona
