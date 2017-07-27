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
#include "Mona/MPEG4.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


void MP4Writer::beginMedia(const OnWrite& onWrite) {
	_loading = true;
	_audioOutOfRange = _videoOutOfRange = false;
	_pAVFront = _pAVBack = NULL;
	_sequence = 0;
	_offset = 0;
}

void MP4Writer::endMedia(const OnWrite& onWrite) {
	_pAVBack = NULL; // to force flush!
	flush(onWrite);
	// release resources
	_videos.clear();
	_audios.clear();
}

void MP4Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	
}

void MP4Writer::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!track)
		++track; // 1 based!
	if (track > _audios.size()) {
		if (!_loading) {
			if (!_audioOutOfRange) {
				_audioOutOfRange = true;
				WARN("Audio track ",track," is ignored, MP4 doesn't support dynamic track addition");
			}
			return;
		}
		_audios.resize(track);
	}
	Audios& audios = _audios[track-1];
	if (tag.isConfig && _loading && !audios.config) {
		if (packet.size() <= 0xFF) {
			audios.config = move(packet);
			return; // not dispatched
		}
		WARN("Audio config with size of ", packet.size(), " too large for mp4a/esds box")	
	}
	audios.emplace_back(tag, packet, track);
	if (_loading && !audios.rate)
		audios.rate = tag.rate;
	if (tag.isConfig)
		return; // config packet is not displaid (not timed)
	_pAVBack = &audios.back();
	if (!_pAVFront)
		_pAVFront = &audios.front();
	flush(onWrite);
}

void MP4Writer::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!track)
		++track; // 1 based!
	if (track > _videos.size()) {
		if (!_loading) {
			if (!_videoOutOfRange) {
				_videoOutOfRange = true;
				WARN("Video track ", track, " is ignored, MP4 doesn't support dynamic track addition");
			}
			return;
		}
		_videos.resize(track);
	}
	Videos& videos = _videos[track - 1];
	if (tag.frame == Media::Video::FRAME_CONFIG && _loading && !videos.sps) {
		MPEG4::ParseVideoConfig(packet, videos.sps, videos.pps);
		return; // not dispatched
	}
	videos.emplace_back(tag, packet, track);
	if (tag.frame == Media::Video::FRAME_CONFIG)
		return; // config packet is not displaid (not timed)
	_pAVBack = &videos.back();
	if (!_pAVFront)
		_pAVFront = &videos.front();
	flush(onWrite);
}

void MP4Writer::flush(const OnWrite& onWrite) {
	if (!_pAVFront)
		return;
	UInt32 duration;
	if(_pAVBack) { //else can come from "endMedia" to force the flush!
		 duration = _pAVBack->type == Media::TYPE_AUDIO ? ((Media::Audio*)_pAVBack)->tag.time : ((Media::Video*)_pAVBack)->tag.time;
		duration -= _pAVFront->type == Media::TYPE_AUDIO ? ((Media::Audio*)_pAVFront)->tag.time : ((Media::Video*)_pAVFront)->tag.time;
		if ((duration) < 1000) // Wait one second!
			return;
	}
	_pAVBack = _pAVFront = NULL;

	shared<Buffer> pBuffer(new Buffer());
	BinaryWriter writer(*pBuffer);

	if (_loading) {
		_loading = false;

		// fftyp box
		writer.write(EXPAND("\x00\x00\x00\x24""ftyp\x69\x73\x6f\x6d\x00\x00\x02\x00\x69\x73\x6f\x6d\x69\x73\x6f\x32\x61\x76\x63\x31\x69\x73\x6f\x36\x6d\x70\x34\x31"));
		
		// moov
		UInt32 sizePos = writer.size();
		writer.next(4); // skip size!
		writer.write(EXPAND("moov"));
		{	// mvhd
			writer.write(EXPAND("\x00\x00\x00\x6c""mvhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\xe8\x00\x00\x00\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			writer.write32(_audios.size() + _videos.size() + 1); // next track id
		}

		UInt16 track(0);

		// VIDEOS
		for (const Videos& videos : _videos) {
			// trak
			UInt32 sizePos = writer.size();
			writer.next(4); // skip size!
			writer.write(EXPAND("trak"));
			{ // tkhd
				writer.write(EXPAND("\x00\x00\x00\x5c""tkhd\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00"));
				writer.write32(++track);
				writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			}
			{ // mdia
				UInt32 sizePos = writer.size();
				writer.next(4); // skip size!
				writer.write(EXPAND("mdia"));
				{	// mdhd
					writer.write(EXPAND("\x00\x00\x00\x20""mdhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
					writer.write32(1000); // timescale, precision 1ms
					writer.write(EXPAND("\x00\x00\x00\x00")); // duration
					writer.write16(0x55C4); // TODO lang (0x55C4 = undefined)
					writer.write16(0); // predefined
				}
				{	// hdlr
					writer.write(EXPAND("\x00\x00\x00\x21""hdlr\x00\x00\x00\x00\x00\x00\x00\x00""vide\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
				}
				{ // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
					UInt32 sizePos = writer.size();
					writer.next(4); // skip size!
					writer.write(EXPAND("minf\x00\x00\x00\x14""vmhd\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x24""dinf\x00\x00\x00\x1C""dref\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x0C""url \x00\x00\x00\x01"));
					{ // stbl
						UInt32 sizePos = writer.size();
						writer.next(4); // skip size!
						writer.write(EXPAND("stbl"));
						{ // stsd
							UInt32 sizePos = writer.size();
							writer.next(4); // skip size!
							writer.write(EXPAND("stsd\x00\x00\x00\x00\x00\x00\x00\x01"));
							{ // avc1	
								UInt32 sizePos = writer.size();
								writer.next(4); // skip size!
								// avc1
								// 00 00 00 00 00 00	reserved (6 bytes)
								// 00 01				data reference index (2 bytes)
								// 00 00 00 00			version + revision level
								// 00 00 00 00			vendor
								// 00 00 00 00			temporal quality
								// 00 00 00 00			spatial quality
								// 00 00 00 00			width + height
								// 00 00 00 00			horizontal resolution
								// 00 00 00 00			vertical resolution
								// 00 00 00 00			data size
								// 00 01				frame by sample, always 1
								writer.write(EXPAND("avc1\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01"));
								// 32 x 0				32 byte pascal string - compression name
								writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
								// 00 18				depth (24 bits)
								// FF FF				default color table
								writer.write(EXPAND("\x00\x18\xFF\xFF"));
					
								if (videos.sps) {
									// file:///C:/Users/mathieu/Downloads/standard8978%20(1).pdf => 5.2.1.1
									UInt32 sizePos = writer.size();
									writer.next(4); // skip size!
									writer.write(EXPAND("avcC"));
									MPEG4::WriteVideoConfig(videos.sps, videos.pps, writer);
									BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
								}
								BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
							} // avc1
							BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
						} // stsd
						// stts + stsc + stsz + stco =>
						writer.write(EXPAND("\x00\x00\x00\x10""stts\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stsc\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14""stsz\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stco\x00\x00\x00\x00\x00\x00\x00\x00"));
						BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
					}
					BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
				}  // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
				BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
			} // mdia
			BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
		} // VIDEOS

		  // AUDIOS
		for (const Audios& audios : _audios) {
			// trak
			UInt32 sizePos = writer.size();
			writer.next(4); // skip size!
			writer.write(EXPAND("trak"));
			{ // tkhd
				writer.write(EXPAND("\x00\x00\x00\x5c""tkhd\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00"));
				writer.write32(++track);
				writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			}
			{ // mdia
				UInt32 sizePos = writer.size();
				writer.next(4); // skip size!
				writer.write(EXPAND("mdia"));
				{	// mdhd
					writer.write(EXPAND("\x00\x00\x00\x20""mdhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
					writer.write32(1000); // timescale, precision 1ms
					writer.write(EXPAND("\x00\x00\x00\x00")); // duration
					writer.write16(0x55C4); // TODO lang (0x55C4 = undefined)
					writer.write16(0); // predefined
				}
				{	// hdlr
					writer.write(EXPAND("\x00\x00\x00\x21""hdlr\x00\x00\x00\x00\x00\x00\x00\x00soun\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
				}
				{ // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco

					UInt32 size(225 + audios.config.size());

					writer.write32(size);
					writer.write(EXPAND("minf\x00\x00\x00\x10""smhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x24""dinf\x00\x00\x00\x1C""dref\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x0C""url \x00\x00\x00\x01"));

					writer.write32(size -= 60);
					writer.write(EXPAND("stbl"));

					writer.write32(size = 0x59 + audios.config.size());
					writer.write(EXPAND("stsd\x00\x00\x00\x00\x00\x00\x00\x01"));

					writer.write32(size -= 16);
					// mp4a version = 0, save bandwidth (more lower than 1 or 2) and useless anyway for mp4a
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
					writer.write(EXPAND("mp4a\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x10\x00\x00\x00\x00"));
					// BB 80 00 00		 rate
					writer.write16(limit16(audios.rate)).write16(0); // just a introduction indication, config packet is more precise!
																	 // 00 00 00 27		 length
					writer.write32(size -= 36);
					// 65 73 64 73		 esds
					// 00 00 00 00		 version
					// 03				 
					writer.write(EXPAND("esds\x00\x00\x00\x00\x03"));
					// 25				 length
					writer.write8(size = 23 + audios.config.size());
					// 00 02			 ES ID
					writer.write16(track);
					// 00				 flags
					// 04				 decoder config descriptor
					writer.write(EXPAND("\x00\x04"));
					// 11			 length
					writer.write8(size -= 8); // size includes just decoder config description and audio config desctription
											  // 40 15 00 00 00 00 06 B9 B2 00 00 00 00 => decoder config description
					writer.write(EXPAND("\x40\x15\x00\x00\x00\x00\x06\xB9\xB2\x00\x00\x00\x00"));
					// 05				Audio config descriptor
					// 02				length
					// 11 B0			audio specific config
					writer.write8(5);
					writer.write8(audios.config.size()).write(audios.config);
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
					writer.write(EXPAND("\x00\x00\x00\x10""stts\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stsc\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14""stsz\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stco\x00\x00\x00\x00\x00\x00\x00\x00"));

				}  // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
				BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
			} // mdia
			BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
		} // AUDIOS

		// MVEX is required by spec => https://w3c.github.io/media-source/isobmff-byte-stream-format.html
		writer.write32(8 + (track * 32)); // size of mvex
		writer.write(EXPAND("mvex")); // mvex
		do { // track necessary superior to 0!
			// trex
			writer.write(EXPAND("\x00\x00\x00\x20""trex\x00\x00\x00\x00"));
			writer.write32(track--);
			writer.write(EXPAND("\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
		} while (track);

		BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
	} // _loading!


	// moof
	UInt32 sizeData = 8; // for [size]mdat
	UInt32 sizeMoof = 0;
	for (Videos& videos : _videos) {
		if (videos.size()>1)
			sizeMoof += videos.sizeTraf = (56 + (videos.size() * 12)); // 52 = 68 - 12 (last audio media is not transmit)
	}
	for (Audios& audios : _audios) {
		if (audios.size()>1)
			sizeMoof += audios.sizeTraf = (60 + (audios.size() * 8)); // 56 = 68 - 8 (last audio media is not transmit)
	}
	
	UInt32 offset = writer.size(); // moof offset!

	if (!sizeMoof) {
		_offset += offset;
		return; // nothing to flush!
	}

	writer.write32(sizeMoof+=24);
	writer.write(EXPAND("moof"));
	{	// mfhd
		writer.write(EXPAND("\x00\x00\x00\x10""mfhd\x00\x00\x00\x00"));
		writer.write32(++_sequence);
	}
	UInt16 track(0);
	for (Videos& videos : _videos) {
		// traf
		++track;
		if (videos.size()<2)
			continue;
		writer.write32(videos.sizeTraf);
		writer.write(EXPAND("traf"));
		{ // tfhd
			writer.write(EXPAND("\x00\x00\x00\x18""tfhd\x00\x00\x00\x01"));
			writer.write32(track);
			writer.write64(_offset+offset);
		}
		{ // tfdt => required by https://w3c.github.io/media-source/isobmff-byte-stream-format.html
		  // http://www.etsi.org/deliver/etsi_ts/126200_126299/126244/10.00.00_60/ts_126244v100000p.pdf
		  // 13.5
			writer.write(EXPAND("\x00\x00\x00\x10""tfdt\x00\x00\x00\x00"));
			writer.write32(videos.timeRef);
		}
		{ // trun
			writer.write32(videos.sizeTraf - 48);
			writer.write(EXPAND("trun\x00\x00\x0B\x01")); // flags = samples duration + samples size + data-offset + composition offset
			writer.write32(videos.size() - 1); // array length
			writer.write32(sizeMoof + sizeData); // data-offset
			Media::Video* pVideo = NULL;
			for (Media::Video& video : videos) {
				if (!pVideo) {
					pVideo = &video;
					continue;
				}
				UInt32 duration = max<UInt32>(video.tag.time - pVideo->tag.time, 0);
				videos.timeRef += duration;
				writer.write32(duration); // duration
				writer.write32(pVideo->size()); // size
				writer.write32(pVideo->tag.compositionOffset); // composition offset
				sizeData += pVideo->size();
				pVideo = &video;
			}
		}
	}
	for (Audios& audios : _audios) {
		// traf
		++track;
		if (audios.size()<2)
			continue;
		writer.write32(audios.sizeTraf);
		writer.write(EXPAND("traf"));
		{ // tfhd
			writer.write(EXPAND("\x00\x00\x00\x18""tfhd\x00\x00\x00\x01"));
			writer.write32(track);
			writer.write64(_offset + offset);
		}
		{ // tfdt => required by https://w3c.github.io/media-source/isobmff-byte-stream-format.html
			// http://www.etsi.org/deliver/etsi_ts/126200_126299/126244/10.00.00_60/ts_126244v100000p.pdf
			// 13.5
			writer.write(EXPAND("\x00\x00\x00\x10""tfdt\x00\x00\x00\x00"));
			writer.write32(audios.timeRef);
		}
		{ // trun
			writer.write32(audios.sizeTraf - 48);
			writer.write(EXPAND("trun\x00\x00\x03\x01")); // flags = samples duration + samples size + data-offset
			writer.write32(audios.size()-1); // array length
			writer.write32(sizeMoof + sizeData); // data-offset
			Media::Audio* pAudio = NULL;
			for (Media::Audio& audio : audios) {
				if (!pAudio) {
					pAudio = &audio;
					continue;
				}
				UInt32 duration = max<UInt32>(audio.tag.time - pAudio->tag.time, 0);
				audios.timeRef += duration;
				writer.write32(duration); // duration
				writer.write32(pAudio->size()); // size
				sizeData += pAudio->size();
				pAudio = &audio;
			}
		}
	}

	_offset += writer.size();

	if (!onWrite)
		return;

	// MDAT
	writer.write32(sizeData);
	writer.write(EXPAND("mdat"));
	_offset += 8;
	onWrite(Packet(pBuffer));

	for (Videos& videos : _videos) {
		while (videos.size() > 1) {
			//DEBUG("Video time => ", videos.front().tag.time);
			_offset += videos.front().size();
			onWrite(videos.front());
			videos.pop_front();
		}
	}
	for (Audios& audios : _audios) {
		while (audios.size() > 1) {
			//DEBUG("Audio time => ", audios.front().tag.time);
			_offset += audios.front().size();
			onWrite(audios.front());
			audios.pop_front();
		}
	}

}




} // namespace Mona
