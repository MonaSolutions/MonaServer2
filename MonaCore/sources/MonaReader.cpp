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

UInt32 MonaReader::parse(const Packet& packet, Media::Source& source) {

	BinaryReader reader(packet.data(), packet.size());

	while (reader.available()) {

		if (!_type) {
			// Read header
			while ((_size = reader.read8()) >= 0x0F) {
				if (!_syncError) {
					WARN("MonaReader parse an invalid media header size");
					_syncError = true;
				}
				if (!reader.available())
					return 0;
			}
			_syncError = false;
			if (reader.available() < _size)
				return reader.available();
			if (_size < 4) {
				_data = Media::Data::Type(reader.read8());
				--_size;
				_type = Media::TYPE_DATA;
			} else if (_size & 1) {
				_video.unpack(reader);
				_size -= _video.packSize();
				_type = Media::TYPE_VIDEO;
			} else {
				_audio.unpack(reader);
				_size -= _audio.packSize();
				_type = Media::TYPE_AUDIO;
			}
			if (_size>=2)
				_track = reader.read16();
			reader.next(_size);
			_size = 0;
		}

		if (!_size) {
			// reader content size
			if (reader.available() < 4)
				return reader.available();
			_size = reader.read32();
		}

		if (reader.available() < _size)
			return reader.available();

		if (_type == Media::TYPE_DATA) {
			source.writeData(_track, _data, Packet(packet, reader.current(), _size));
		} else if (_type == Media::TYPE_VIDEO) {
			source.writeVideo(_track, _video, Packet(packet, reader.current(), _size));
		} else
			source.writeAudio(_track, _audio, Packet(packet, reader.current(), _size));

		reader.next(_size);
		_type = Media::TYPE_NONE;
		_size = 0;
	}

	return 0;
}

void MonaReader::onFlush(const Packet& packet, Media::Source& source) {
	_type = Media::TYPE_NONE;
	_size = 0;
	_track = 0;
	_syncError = false;
	MediaReader::onFlush(packet, source);
}

} // namespace Mona
