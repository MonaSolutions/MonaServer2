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
#include "Mona/StreamData.h"

namespace Mona {

struct MediaReader : virtual Object, private StreamData<Media::Source&> {
	
	static unique<MediaReader> New(const char* subMime);
	static unique<MediaReader> New(const std::string& subMime) { return New(subMime.c_str()); }

	virtual void setParams(const Parameters& parameters) {}

	void		 read(const Packet& packet, Media::Source& source);
	virtual void flush(Media::Source& source);

	const char*			format() const;
	MIME::Type			mime() const;
	virtual const char*	subMime() const; // Keep virtual to allow to RTPReader to redefine it

protected:
	MediaReader() {}
	
	virtual void	onFlush(Packet& buffer, Media::Source& source);
private:
	/*!
	Implements this method, and return rest to wait more data.
	/!\ Must tolerate data lost, so on error displays a WARN and try to resolve the strem */
	virtual UInt32	parse(Packet& buffer, Media::Source& source) = 0;

	UInt32 onStreamData(Packet& buffer, Media::Source& source) { return parse(buffer, source); }
};

struct MediaTrackReader : virtual Object, MediaReader {

	UInt8  track;
	UInt32 time; // new container time
	UInt16 compositionOffset; // new container compositionOffset

	void setParams(const Parameters& parameters) { parameters.getNumber("track", track=1);  }

	void flush(Media::Source& source) {
		MediaReader::flush(source);
		time = compositionOffset = 0;
	}

protected:
	MediaTrackReader(UInt8 track) : track(track ? track : 1), time(0),compositionOffset(0) {}

	void	onFlush(Packet& buffer, Media::Source& source) {} // no reset and flush for track, container will do it rather
};


} // namespace Mona
