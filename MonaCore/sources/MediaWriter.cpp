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
#include "Mona/MP4Writer.h"
#include "Mona/NALNetWriter.h"
#include "Mona/ADTSWriter.h"
//#include "Mona/MP3Writer.h"
#include "Mona/VTTWriter.h"
#include "Mona/DATWriter.h"
#include "Mona/MonaWriter.h"
#include "Mona/RTPWriter.h"
#include "Mona/RTP_MPEG.h"
#include "Mona/RTP_H264.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"

#include "Mona/Logs.h"


using namespace std;


namespace Mona {

struct Format {
	Format(const char* name, MIME::Type mime, const char* subMime) : name(name), mime(mime), subMime(subMime) {}
	const char*	name;
	MIME::Type	mime;
	const char*	subMime;
};
static const map<size_t, Format> _Formats({
	// Keep Format name in lower case because can be sometimes used to determine extension
	{ typeid(FLVWriter).hash_code(), Format("flv", MIME::TYPE_VIDEO, "x-flv") },
	{ typeid(TSWriter).hash_code(), Format("ts", MIME::TYPE_VIDEO, "mp2t") },
	{ typeid(MP4Writer).hash_code(), Format("mp4", MIME::TYPE_VIDEO, "mp4") },
	{ typeid(NALNetWriter<AVC>).hash_code(), Format("h264", MIME::TYPE_VIDEO, "h264") },
	{ typeid(NALNetWriter<HEVC>).hash_code(), Format("h265", MIME::TYPE_VIDEO, "hevc") },
	{ typeid(ADTSWriter).hash_code(), Format("adts", MIME::TYPE_AUDIO, "aac") },
	// { typeid(MP3Writer).hash_code(), Format("mp3", MIME::TYPE_AUDIO, "mp3") },
	{ typeid(SRTWriter).hash_code(), Format("srt", MIME::TYPE_APPLICATION, "x-subrip; charset=utf-8") },
	{ typeid(VTTWriter).hash_code(), Format("vtt", MIME::TYPE_TEXT, "vtt; charset=utf-8") },
	{ typeid(DATWriter).hash_code(), Format("dat", MIME::TYPE_TEXT, "plain; charset=utf-8") },
	{ typeid(MonaWriter).hash_code(), Format("mona", MIME::TYPE_VIDEO, "mona") },
	{ typeid(RTPWriter<RTP_MPEG>).hash_code(), Format("rtp_mpeg", MIME::TYPE_VIDEO, NULL) }, // Keep NULL to force RTPReader to redefine mime()!
	{ typeid(RTPWriter<RTP_H264>).hash_code(), Format("rtp_h264", MIME::TYPE_VIDEO, NULL) } // Keep NULL to force RTPReader to redefine mime()!
});
const char* MediaWriter::format() const {
	return _Formats.at(typeid(*this).hash_code()).name; // keep exception if no exists => developper warn! Add it!
}
MIME::Type MediaWriter::mime() const {
	return _Formats.at(typeid(*this).hash_code()).mime; // keep exception if no exists => developper warn! Add it!
}
const char* MediaWriter::subMime() const {
	return _Formats.at(typeid(*this).hash_code()).subMime; // keep exception if no exists => developper warn! Add it!
}

unique<MediaWriter> MediaWriter::New(const char* subMime) {
	if (String::ICompare(subMime, EXPAND("x-flv")) == 0 || String::ICompare(subMime, EXPAND("flv")) == 0)
		return make_unique<FLVWriter>();
	if (String::ICompare(subMime, EXPAND("mp2t")) == 0 || String::ICompare(subMime, EXPAND("ts")) == 0)
		return make_unique<TSWriter>();
	if (String::ICompare(subMime, EXPAND("mp4")) == 0 || String::ICompare(subMime, EXPAND("f4v")) == 0 || String::ICompare(subMime, EXPAND("mov")) == 0)
		return make_unique<MP4Writer>();
	if (String::ICompare(subMime, EXPAND("h264")) == 0 || String::ICompare(subMime, EXPAND("264")) == 0)
		return make_unique<NALNetWriter<AVC>>();
	if (String::ICompare(subMime, EXPAND("hevc")) == 0 || String::ICompare(subMime, EXPAND("265")) == 0)
		return make_unique<NALNetWriter<HEVC>>();
	if (String::ICompare(subMime, EXPAND("aac")) == 0)
		return make_unique<ADTSWriter>();
//	if (String::ICompare(subMime, EXPAND("mp3")) == 0)
//		return make_unique<MP3Writer>();
	if (String::ICompare(subMime, EXPAND("srt")) == 0 || String::ICompare(subMime, EXPAND("x-subrip")) == 0)
		return make_unique<SRTWriter>();
	if (String::ICompare(subMime, EXPAND("vtt")) == 0)
		return make_unique<VTTWriter>();
	if (String::ICompare(subMime, EXPAND("plain")) == 0)
		return make_unique<DATWriter>();
	if (String::ICompare(subMime, EXPAND("mona")) == 0)
		return make_unique<MonaWriter>();
	return nullptr;
}


void MediaWriter::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	DEBUG(TypeOf(self), " doesn't support data writing operation");
}
void MediaWriter::writeMedia(const Media::Base& media, const OnWrite& onWrite) {
	switch (media.type) {
		case Media::TYPE_AUDIO:
			return writeAudio(media.track, ((const Media::Audio&)media).tag, media, onWrite);
		case Media::TYPE_VIDEO:
			return writeVideo(media.track, ((const Media::Video&)media).tag, media, onWrite);
		case Media::TYPE_DATA: {
			const Media::Data& data = (const Media::Data&)media;
			if (data.isProperties)
				return writeProperties(Media::Properties(media.track, data.tag, media), onWrite);
			return writeData(media.track, data.tag, media, onWrite);
		}
		default:
			WARN(TypeOf(self), " write a unknown media");
	}
}

void MediaTrackWriter::writeData(Media::Data::Type type, const Packet& packet, const OnWrite& onWrite, UInt32& finalSize) {
	DEBUG(TypeOf(self), " doesn't support data writing operation");
}
void MediaTrackWriter::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!track) {
		UInt32 finalSize;
		writeAudio(tag, packet, onWrite, finalSize);
	} else
		WARN(TypeOf(self), " doesn't support multitrack")
}
void MediaTrackWriter::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!track) {
		UInt32 finalSize;
		writeVideo(tag, packet, onWrite, finalSize);
	} else
		WARN(TypeOf(self), " doesn't support multitrack")
}
void MediaTrackWriter::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	if (!track) {
		UInt32 finalSize;
		writeData(type, packet, onWrite, finalSize);
	} else
		WARN(TypeOf(self), " doesn't support multitrack")
}

} // namespace Mona
