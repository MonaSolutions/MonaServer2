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

#include "Mona/MP4Writer.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


void MP4Writer::beginMedia(const OnWrite& onWrite) {
	_loading = true;
	_audioOutOfRange = _videoOutOfRange = false;
	_pAVFront = _pAVBack = NULL;

}

void MP4Writer::endMedia(const OnWrite& onWrite) {
	
}

void MP4Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	
}

void MP4Writer::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (track)
		--track; // zero based!
	if (track >= _audios.size()) {
		if (!_loading) {
			if (!_audioOutOfRange) {
				_audioOutOfRange = true;
				WARN("Audio track ",track," is ignored, MP4 doesn't support dynamic track addition");
			}
			return;
		}
		vector<deque<Media::Audio>> oldAudios(track + 1);
		for (UInt8 i = 0; i < _audios.size(); ++i)
			oldAudios[i] = move(_audios[i]);
		_audios = move(oldAudios);
	}
	_audios[track].emplace_back(tag, packet, track);
	if (tag.isConfig)
		return; // config packet is not displaid
	_pAVBack = &_audios[track].back();
	if (!_pAVFront)
		_pAVFront = _pAVBack;
	flush(onWrite);
}

void MP4Writer::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	return;
	if (track)
		--track; // zero based!
	if (track >= _videos.size()) {
		if (!_loading) {
			if (!_videoOutOfRange) {
				_videoOutOfRange = true;
				WARN("Video track ", track, " is ignored, MP4 doesn't support dynamic track addition");
			}
			return;
		}
		vector<deque<Media::Video>> oldVideos(track+1);
		for (UInt8 i = 0; i < _videos.size(); ++i)
			oldVideos[i] = move(_videos[i]);
		_videos = move(oldVideos);
	}
	_videos[track].emplace_back(tag, packet, track);
	if (tag.frame==Media::Video::FRAME_CONFIG)
		return; // config packet is not displaid
	_pAVBack = &_videos[track].back();
	if (!_pAVFront)
		_pAVFront = _pAVBack;
	flush(onWrite);
}

void MP4Writer::flush(const OnWrite& onWrite) {
	if (!_pAVBack || !_pAVFront)
		return;
	UInt32 front(_pAVFront->type == Media::TYPE_AUDIO ? ((Media::Audio*)_pAVFront)->tag.time : ((Media::Video*)_pAVFront)->tag.time);
	UInt32 back(_pAVBack->type == Media::TYPE_AUDIO ? ((Media::Audio*)_pAVBack)->tag.time : ((Media::Video*)_pAVBack)->tag.time);
	if ((back - front) < 1000) // Wait one second!
		return;

	if (_loading) {
		_loading = false;
		shared<Buffer> pBuffer(new Buffer());
		BinaryWriter writer(*pBuffer);

		// fftyp box
		writer.write(EXPAND("\x00\x00\x00\x24\x66\x74\x79\x70\x69\x73\x6f\x6d\x00\x00\x02\x00\x69\x73\x6f\x6d\x69\x73\x6f\x32\x61\x76\x63\x31\x69\x73\x6f\x36\x6d\x70\x34\x31"));
		
		// moov
		UInt32 sizePos = writer.size();
		writer.next(4); // skip size!
		writer.write(EXPAND("moov"));
		{	// mvhd
			writer.write(EXPAND("\x00\x00\x00\x6c\x6d\x76\x68\x64\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\xe8\x00\x00\x00\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			writer.write32(_audios.size() + _videos.size() + 1); // next track id
		}
		UInt16 track(0);
		for(deque<Media::Audio>& audios : _audios) {
			// trak
			UInt32 sizePos = writer.size();
			writer.next(4); // skip size!
			writer.write(EXPAND("trak"));
			{ // tkhd
				writer.write(EXPAND("\x00\x00\x00\x5c\x74\x6b\x68\x64\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00"));
				writer.write32(++track);
				writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			} { // mdia
				UInt32 sizePos = writer.size();
				writer.next(4); // skip size!
				writer.write(EXPAND("mdia"));
				{	// mdhd
					writer.write(EXPAND("\x00\x00\x00\x20\x6d\x64\x68\x64\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
					writer.write32(0xFFFFFFFF); // timescale!
					writer.write(EXPAND("\x00\x00\x00\x00")); // duration
					writer.write16(0x55C4); // TODO lang (0x55C4 = undefined)
					writer.write16(0); // predefined
				} {	// hdlr
					writer.write(EXPAND("\x00\x00\x00\x21\x68\x64\x6c\x72\x00\x00\x00\x00\x00\x00\x00\x00\x73\x6f\x75\x6e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
				} { // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco

					Packet* pConfig(NULL);
					UInt32 size(223);
					for (Media::Audio& audio : audios) {
						if (audio.tag.isConfig) {
							if (audio.size() > 0xFF) {
								WARN("Audio config with size of ", audio.size(), " too large for mp4a/esds box");
								continue;
							}
							size += audio.size() + 2;
							pConfig = &audio;
							break;
						}
					}
			
						// \x00\x00\x00\xE3
					writer.write32(size);
					// \x6D\x69\x6E\x66 minf
					writer.write(EXPAND("\x6D\x69\x6E\x66"));
					// \x00\x00\x00\x10
					// \x73\x6D\x68\x64 smhd
					// \x00\x00\x00\x00\x00\x00\x00\x00
					// \x00\x00\x00\x24 
					// \x64\x69\x6E\x66 dinf
					// \x00\x00\x00\x1C 
					// \x64\x72\x65\x66 dref
					// \x00\x00\x00\x00\x00\x00\x00\x01
					// \x00\x00\x00\x0C
					// \x75\x72\x6C\x20 url
					// \x00\x00\x00\x01
					writer.write(EXPAND("\x00\x00\x00\x10\x73\x6D\x68\x64\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x24\x64\x69\x6E\x66\x00\x00\x00\x1C\x64\x72\x65\x66\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x0C\x75\x72\x6C\x20\x00\x00\x00\x01"));
						// \x00\x00\x00\xB3
					writer.write32(size -= 60);
					// \x73\x74\x62\x6C stbl
					writer.write(EXPAND("\x73\x74\x62\x6C"));
						// \x00\x00\x00\x5B
					size = 0x52;
					if (pConfig)
						size += 2 + pConfig->size();
					writer.write32(size);
					// \x73\x74\x73\x64 stsd
					// \x00\x00\x00\x00\x00\x00\x00\x01
					writer.write(EXPAND("\x73\x74\x73\x64\x00\x00\x00\x00\x00\x00\x00\x01"));
						// 00 00 00 4B
					writer.write32(size -= 16);
					// 6D 70 34 61		 mp4a
					// 00 00 00 00 00 00 reserved
					// 00 01			 data reference index			
					// 00 00			 version
					// 00 00			 revision level
					// 00 00 00 00		 vendor	
					// 00 02			 channels
					// 00 10			 bits 
					// 00 00			 compression id
					// 00 00			 packet size
					writer.write(EXPAND("\x6D\x70\x34\x61\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x10\x00\x00\x00\x00"));
						// BB 80 00 00		 rate
					writer.write16(limit16(audios.front().tag.rate)).write16(0); // just a introduction indication, config packet is more precise!
						// 00 00 00 27		 length
					writer.write32(size -= 36);
					// 65 73 64 73		 esds
					// 00 00 00 00		 version
					// 03				 
					writer.write(EXPAND("\x65\x73\x64\x73\x00\x00\x00\x00\x03"));
						// 16				 length
					size = 0x12;
					if (pConfig)
						size += 2 + pConfig->size();
					writer.write8(size);
					// 00 02			 ES ID
					// 00				 flags
					// 04
					writer.write(EXPAND("\x00\x02\x00\x04"));
						// 11			 length
					writer.write8(size-=5);
					// 40 15 00 00 00 00 06 B9 B2 00 00 00 00 => decoder config descriptor
					writer.write(EXPAND("\x40\x15\x00\x00\x00\x00\x06\xB9\xB2\x00\x00\x00\x00"));
					// 05				Audio config descriptor
						// 02				length
						// 11 B0			audio specific config
					if (pConfig) {
						writer.write8(5);
						writer.write8(pConfig->size());
						writer.write(*pConfig);
					}
					// 06				SL config descriptor
					// 01				length
					// 02				flags
					writer.write(EXPAND("\x06\x01\x02"));

					// decoder config descriptor =>
					// 40 MPEG4 audio
					// 15 Audio!
					// 00 00 00 buffer size = 0
					// 00 00 00 00 => max bitrate
					// 00 00 00 00 => average bitrate
	
					// stts + stsc + stsz + stco =>
					writer.write(EXPAND("\x00\x00\x00\x10\x73\x74\x74\x73\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x73\x74\x73\x63\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x73\x74\x73\x7A\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x73\x74\x63\x6F\x00\x00\x00\x00\x00\x00\x00\x00"));

				}  // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
				BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size()-sizePos);
			} // mdia
			BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
		} // trak for _audios
		// TODO try without mvex!
		writer.write(EXPAND("\x00\x00\x00\x48\x6D\x76\x65\x78")); // mvex
		do { // track necessary superior to 0!
			writer.write(EXPAND("00\x00\x00\x20\x74\x72\x65\x78\x00\x00\x00\x00"));
			writer.write32(track--);
			writer.write(EXPAND("\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
		} while (track);

		BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);

		if (onWrite)
			onWrite(Packet(pBuffer));
	} // _loading!
}




} // namespace Mona
