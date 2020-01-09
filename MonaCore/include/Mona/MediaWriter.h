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

	static unique<MediaWriter> New(const char* subMime);
	static unique<MediaWriter> New(const std::string& subMime) { return New(subMime.c_str()); }

	virtual const char*	format() const;
	virtual MIME::Type	mime() const;
	virtual const char* subMime() const;


	typedef std::function<void(const Packet& packet)> OnWrite;

	virtual void beginMedia(const OnWrite& onWrite) {}
	virtual void writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {}
	virtual void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) = 0;
	virtual void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) = 0;
	virtual void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite);
	virtual void endMedia(const OnWrite& onWrite) {}
	
	void writeMedia(const Media::Base& media, const OnWrite& onWrite);
};

struct MediaTrackWriter : MediaWriter, virtual Object {
	/// Media container writer must be able to support a dynamic change of audio/video codec!
	virtual void beginMedia() {} // no container for track writer!
	virtual void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) = 0;
	virtual void writeVideo(const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) = 0;
	virtual void writeData(Media::Data::Type type, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize);
	virtual void endMedia() {}  // no container for track writer!
private:
	// Define MediaWriter implementation to redirect on MediaTrackWriter implementation
	void beginMedia(const OnWrite& onWrite) { beginMedia(); }
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite);
	void endMedia(const OnWrite& onWrite) { endMedia(); }
};

} // namespace Mona
