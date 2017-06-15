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

#include "Mona/MediaWriter.h"

#include "Mona/FLVWriter.h"
#include "Mona/TSWriter.h"
#include "Mona/H264NALWriter.h"
#include "Mona/ADTSWriter.h"
//#include "Mona/MP3Writer.h"
#include "Mona/MonaWriter.h"
#include "Mona/RTPWriter.h"
#include "Mona/RTP_MPEG.h"
#include "Mona/RTP_H264.h"

#include "Mona/Logs.h"


using namespace std;


namespace Mona {

struct Format {
	Format(const char* format, MIME::Type mime, const char* subMime) : format(format), mime(mime), subMime(subMime) {}
	const char*	format;
	MIME::Type	mime;
	const char*	subMime;
};
static const map<size_t, Format> _Formats({
	{ typeid(FLVWriter).hash_code(), Format("FLV", MIME::TYPE_VIDEO, "x-flv") },
	{ typeid(TSWriter).hash_code(), Format("TS", MIME::TYPE_VIDEO, "mp2t") },
	{ typeid(H264NALWriter).hash_code(), Format("H264", MIME::TYPE_VIDEO, "h264") },
	{ typeid(ADTSWriter).hash_code(), Format("ADTS", MIME::TYPE_AUDIO, "aac") },
	// { typeid(MP3Writer).hash_code(), Format("MP3", MIME::TYPE_AUDIO, "mp3") },
	{ typeid(MonaWriter).hash_code(), Format("MONA", MIME::TYPE_VIDEO, "mona") },
	{ typeid(RTPWriter<RTP_MPEG>).hash_code(), Format("RTP_MPEG", MIME::TYPE_VIDEO, NULL) }, // Keep NULL to force RTPReader to redefine mime()!
	{ typeid(RTPWriter<RTP_H264>).hash_code(), Format("RTP_H264", MIME::TYPE_VIDEO, NULL) } // Keep NULL to force RTPReader to redefine mime()!
});
const char* MediaWriter::format() const {
	return _Formats.at(typeid(*this).hash_code()).format; // keep exception if no exists => developper warn! Add it!
}
MIME::Type MediaWriter::mime() const {
	return _Formats.at(typeid(*this).hash_code()).mime; // keep exception if no exists => developper warn! Add it!
}
const char* MediaWriter::subMime() const {
	return _Formats.at(typeid(*this).hash_code()).subMime; // keep exception if no exists => developper warn! Add it!
}

MediaWriter* MediaWriter::New(const char* subMime) {
	if (String::ICompare(subMime, "x-flv") == 0 || String::ICompare(subMime, "flv") == 0)
		return new FLVWriter();
	if (String::ICompare(subMime, "mp2t") == 0 || String::ICompare(subMime, "ts") == 0)
		return new TSWriter();
	if (String::ICompare(subMime, "h264") == 0 || String::ICompare(subMime, "264") == 0)
		return new H264NALWriter();
	if (String::ICompare(subMime, "aac") == 0)
		return new ADTSWriter();
//	if (String::ICompare(subMime, "mp3") == 0)
//		return new MP3Writer();
	if (String::ICompare(subMime, "mona") == 0)
		return new MonaWriter();
	return NULL;
}

void MediaWriter::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	WARN(typeof(*this), " doesn't support data writing operation");
}
void MediaWriter::writeMedia(const Media::Base& media, const OnWrite& onWrite) {
	switch (media.type) {
		case Media::TYPE_AUDIO:
			return writeAudio(media.track, ((const Media::Audio&)media).tag, media, onWrite);
		case Media::TYPE_VIDEO:
			return writeVideo(media.track, ((const Media::Video&)media).tag, media, onWrite);
		case Media::TYPE_DATA:
			return writeData(media.track, ((const Media::Data&)media).tag, media, onWrite);
		default:
			writeProperties(Media::Properties((const Media::Data&)media), onWrite);
	}
}

void MediaTrackWriter::writeData(Media::Data::Type type, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	WARN(typeof(*this), " doesn't support data writing operation");
}
void MediaTrackWriter::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!track) {
		UInt32 finalSize;
		writeAudio(tag, packet, onWrite, finalSize);
	} else
		WARN(typeof(*this), " doesn't support multitrack")
}
void MediaTrackWriter::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!track) {
		UInt32 finalSize;
		writeVideo(tag, packet, onWrite, finalSize);
	} else
		WARN(typeof(*this), " doesn't support multitrack")
}
void MediaTrackWriter::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	if (!track) {
		UInt32 finalSize;
		writeData(type, packet, onWrite, finalSize);
	} else
		WARN(typeof(*this), " doesn't support multitrack")
}

} // namespace Mona
