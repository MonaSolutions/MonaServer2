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
#include "Mona/Media.h"

namespace Mona {

struct MediaReader : virtual Object {
	
	static MediaReader* New(const char* subMime);

	void		 read(const Packet& packet, Media::Source& source);
	virtual void flush(Media::Source& source);

	const char*			format() const;
	virtual MIME::Type	mime() const; // Keep virtual to allow to RTPReader to redefine it
	virtual const char*	subMime() const; // Keep virtual to allow to RTPReader to redefine it

	virtual ~MediaReader();
protected:
	MediaReader() {}


	virtual void	onFlush(const Packet& packet, Media::Source& source);
	virtual bool	parsePacket(const Packet& packet, Media::Source& source);
private:
	/*!
	Implements this method, and return rest to wait more data.
	/!\ Must tolerate data lost, so on error displays a WARN and try to resolve the strem */
	virtual UInt32	parse(const Packet& packet, Media::Source& source) = 0;

	shared<Buffer> _pBuffer;
};

struct MediaTrackReader : virtual Object, MediaReader {

	UInt8  track;
	UInt32 time; // new container time
	UInt16 compositionOffset; // new container compositionOffset

	void flush(Media::Source& source) {
		MediaReader::flush(source);
		time = compositionOffset = 0;
	}

protected:
	MediaTrackReader(UInt8 track=1) : track(track), time(0),compositionOffset(0) {}

	bool	parsePacket(const Packet& packet, Media::Source& source) { MediaReader::parsePacket(packet, source); return false; } // no flush for track, container will do it rather
	void	onFlush(const Packet& packet, Media::Source& source) {} // no reset and flush for track, container will do it rather
};


} // namespace Mona
