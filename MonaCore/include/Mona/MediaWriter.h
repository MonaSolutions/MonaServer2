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

struct MediaWriter : virtual Object {
	/// Media container writer must be able to support a dynamic change of audio/video codec!

	static MediaWriter* New(const char* subMime);

	const char*			format() const;
	virtual MIME::Type	mime() const; // Keep virtual to allow to RTPReader to redefine it
	virtual const char* subMime() const; // Keep virtual to allow to RTPReader to redefine it


	typedef std::function<void(const Packet& packet)> OnWrite;

	virtual void beginMedia(const OnWrite& onWrite) {}
	virtual void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) = 0;
	virtual void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) = 0;
	virtual void writeData(UInt16 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite);
	virtual void endMedia(const OnWrite& onWrite) {}

	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { writeAudio(0, tag, packet, onWrite); }
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { writeVideo(0, tag, packet, onWrite); }
	void writeData(Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { writeData(0, type, packet, onWrite); }

	void writeMedia(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) { writeAudio(track, tag, packet, onWrite); }
	void writeMedia(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) { writeVideo(track, tag, packet, onWrite); }
	void writeMedia(UInt16 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { writeData(track, type, packet, onWrite); }
};

class TrackWriter : public MediaWriter, public virtual Object {
	/// Media container writer must be able to support a dynamic change of audio/video codec!
public:
	virtual void beginMedia() {} // no container for track writer!
	virtual void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) = 0;
	virtual void writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) = 0;
	virtual void writeData(Media::Data::Type type, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize);
	virtual void endMedia() {}  // no container for track writer!
private:
	// Define MediaWriter implementation to redirect on TrackWriter implementation
	void beginMedia(const OnWrite& onWrite) { beginMedia(); }
	void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeData(UInt16 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite);
	void endMedia(const OnWrite& onWrite) { endMedia(); }
};

} // namespace Mona
