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

#include "Mona/MediaReader.h"

#include "Mona/FLVReader.h"
#include "Mona/TSReader.h"
#include "Mona/MP4Reader.h"
#include "Mona/H264NALReader.h"
#include "Mona/ADTSReader.h"
#include "Mona/MP3Reader.h"
#include "Mona/MonaReader.h"
#include "Mona/RTPReader.h"
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
	{ typeid(FLVReader).hash_code(), Format("FLV", MIME::TYPE_VIDEO, "x-flv") },
	{ typeid(TSReader).hash_code(), Format("TS", MIME::TYPE_VIDEO, "mp2t") },
	{ typeid(MP4Reader).hash_code(), Format("MP4", MIME::TYPE_VIDEO, "mp4") },
	{ typeid(H264NALReader).hash_code(), Format("H264", MIME::TYPE_VIDEO, "h264") },
	{ typeid(ADTSReader).hash_code(), Format("ADTS", MIME::TYPE_AUDIO, "aac") },
	{ typeid(MP3Reader).hash_code(), Format("MP3", MIME::TYPE_AUDIO, "mp3") },
	{ typeid(MonaReader).hash_code(), Format("MONA", MIME::TYPE_VIDEO, "mona") },
	{ typeid(RTPReader<RTP_MPEG>).hash_code(), Format("RTP_MPEG", MIME::TYPE_VIDEO, NULL) }, // Keep NULL to force RTPReader to redefine subMime()!
	{ typeid(RTPReader<RTP_H264>).hash_code(), Format("RTP_H264", MIME::TYPE_VIDEO, NULL) } // Keep NULL to force RTPReader to redefine subMime()!
});
const char* MediaReader::format() const {
	return _Formats.at(typeid(*this).hash_code()).format; // keep exception isf no exists => developper warn! Add it!
}
MIME::Type MediaReader::mime() const {
	return _Formats.at(typeid(*this).hash_code()).mime; // keep exception isf no exists => developper warn! Add it!
}
const char* MediaReader::subMime() const {
	return _Formats.at(typeid(*this).hash_code()).subMime; // keep exception isf no exists => developper warn! Add it!
}

MediaReader* MediaReader::New(const char* subMime) {
	if (String::ICompare(subMime, EXPAND("x-flv")) == 0 || String::ICompare(subMime, EXPAND("flv")) == 0)
		return new FLVReader();
	if (String::ICompare(subMime, EXPAND("mp2t")) == 0 || String::ICompare(subMime, EXPAND("ts")) == 0)
		return new TSReader();
	if (String::ICompare(subMime, EXPAND("mp4")) == 0 || String::ICompare(subMime, EXPAND("f4v")) == 0 || String::ICompare(subMime, EXPAND("mov")) == 0)
		return new MP4Reader();
	if (String::ICompare(subMime, EXPAND("h264")) == 0 || String::ICompare(subMime, EXPAND("264")) == 0)
		return new H264NALReader();
	if (String::ICompare(subMime, EXPAND("aac")) == 0)
		return new ADTSReader();
	if (String::ICompare(subMime, EXPAND("mp3")) == 0)
		return new MP3Reader();
	if (String::ICompare(subMime, EXPAND("mona")) == 0)
		return new MonaReader();
	return NULL;
}

void MediaReader::read(const Packet& packet, Media::Source& source) {
	shared<Buffer> pBuffer(_pBuffer);
	if (pBuffer) {
		pBuffer->append(packet.data(), packet.size());
		parsePacket(Packet(pBuffer), source);
	} else
		parsePacket(packet, source);
}
void MediaReader::flush(Media::Source& source) {
	if (_pBuffer)
		onFlush(Packet(_pBuffer), source);
	else
		onFlush(Packet::Null(), source);
}
void MediaReader::onFlush(const Packet& packet, Media::Source& source) {
	source.reset();
	source.flush(); // flush after reset!
}
void MediaReader::parsePacket(const Packet& packet, Media::Source& source) {
	UInt32 rest = parse(packet, source);
	if (!rest) {
		_pBuffer.reset();
		return;
	}
	if (rest > packet.size()) {
		WARN(typeof(*this)," indicatse a media rest more greather than data");
		rest = packet.size();
	}

	if (!_pBuffer) {
		_pBuffer.reset(new Buffer(rest, packet.data() + packet.size() - rest));
		return;
	}
	// here packet encapsulate _pBuffer!
	if (_pBuffer.unique()) {
		if (rest < _pBuffer->size()) {
			memmove(_pBuffer->data(), _pBuffer->data() + _pBuffer->size() - rest, rest);
			_pBuffer->resize(rest, true);
		}
	} else // else new buffer because has been captured in a packet by decoding code
		_pBuffer.reset(new Buffer(rest, _pBuffer->data() + _pBuffer->size() - rest));
}

} // namespace Mona
