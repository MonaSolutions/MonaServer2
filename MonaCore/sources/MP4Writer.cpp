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
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/Logs.h"
#include "Mona/Util.h"

using namespace std;

namespace Mona {

deque<MP4Writer::Frame> MP4Writer::Frames::flush() {
	hasKey = hasCompositionOffset = false;
	deque<MP4Writer::Frame> frames(move(self));
	if (writeConfig) {
		if(config)
			frames.emplace_front(0, config);
		writeConfig = false;
	}
	return frames;
}

MP4Writer::MP4Writer(UInt16 bufferTime) : bufferTime(max(bufferTime, BUFFER_RESET_SIZE)), _timeFront(0), _timeBack(0), _started(false) {
	INFO("MP4 bufferTime set to ", bufferTime, "ms");
}

void MP4Writer::beginMedia(const OnWrite& onWrite) {
	_buffering = bufferTime;
	_bufferMinSize = BUFFER_MIN_SIZE;
	_sequence = 0;
	_errors = 0;
	_seekTime = _timeBack;
	// not reset this._started to keep _timeFront on old value to smooth transition and silence creation
}

void MP4Writer::endMedia(const OnWrite& onWrite) {
	if(_started)
		flush(onWrite, -1);
	// release resources
	_videos.clear();
	_audios.clear();
}

void MP4Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	// TODO langs?
}

void MP4Writer::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (tag.codec != Media::Audio::CODEC_AAC && tag.codec != Media::Audio::CODEC_MP3) {
		if (!(_errors & 1)) {
			_errors |= 1;
			WARN("Audio codec ",Media::Audio::CodecToString(tag.codec)," ignored, Web MP4 supports currently just AAC and MP3 codecs");
		}
		return;
	}
	if (!track)
		++track; // 1 based!

	UInt32 time = tag.time + _seekTime;
	//INFO(tag.isConfig ? "Audio config " : "Audio ", time);

	if (track > _audios.size())
		_audios.emplace_back(time);

	Frames& audios = _audios[track - 1];
	if (tag.isConfig) {
		if (audios.config != packet) {
			if (audios)
				flush(onWrite, true); // reset if "config" change
			audios.config = move(packet); // after flush reset!
		}
		if (!audios.rate) // sometimes present just config packet
			audios.rate = tag.rate; // after flush reset!
		return;
	}
	if (Util::Distance(audios.empty() ? audios.lastTime : audios.back().time, time)<0) {
		WARN("Non-monotonic audio time ", time,", packet ignored");
		return;
	}
	if (!_started || Util::Distance(_timeBack, time)>0)
		_timeBack = time;
	// flush before emplace_back, with reset if codec change (if audios, codec is set) or started
	if (_buffering)
		flush(onWrite);
	else
		flush(onWrite, audios ? audios.codec != tag.codec : false);
	if (!audios.rate)
		audios.rate = tag.rate;
	if (!audios)
		audios.lastTime = _timeFront; // will add silence to sync audio/video
	audios.push(time, tag, packet);
}

void MP4Writer::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (tag.codec != Media::Video::CODEC_H264 && tag.codec != Media::Video::CODEC_HEVC) {
		if (!(_errors & 2)) {
			_errors |= 2;
			WARN("Video codec ", Media::Video::CodecToString(tag.codec), " ignored, Web MP4 supports just H264 codec");
		}
		return;
	}
	if (!track)
		++track; // 1 based!
	
	UInt32 time = tag.time + _seekTime;
	// NOTE(tag.frame == Media::Video::FRAME_CONFIG ? "Video config " : "Video ", tag.frame, ' ', time);

	if (track > _videos.size())
		_videos.emplace_back(time);

	Frames& videos = _videos[track - 1];
	if (tag.frame == Media::Video::FRAME_CONFIG) {
		if (videos.config != packet) {
			if (videos)
				flush(onWrite, true); // reset if "config" change
			videos.config = move(packet); // after flush reset!
		}
		return;
	}
	if (Util::Distance(videos.empty() ? videos.lastTime : videos.back().time, time) < 0) {
		WARN("Non-monotonic video time ", time, ", packet ignored");
		return;
	}
	if (!_started || Util::Distance(_timeBack, time)>0)
		_timeBack = time;
	// flush before emplace_back, with reset if codec change (if videos, codec is set) or started
	if(_buffering)
		flush(onWrite);
	else
		flush(onWrite, videos ? videos.codec != tag.codec : false);
	if (tag.frame == Media::Video::FRAME_KEY)
		videos.hasKey = true;
	if (tag.compositionOffset)
		videos.hasCompositionOffset = true;
	if (!videos)
		videos.lastTime = _timeFront; // will add silence to sync audio/video
	videos.push(time, tag, packet);
}

void MP4Writer::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
	//if (type != Media::Data::TYPE_TEXT) TODO uncomment after fix ticket https://haivision.jira.com/jira/software/projects/WEBG/boards/322/backlog?selectedIssue=WEBG-9
		return; // only subtitles are allowed

	if (!track)
		++track; // 1 based!

	if (track > _datas.size())
		_datas.emplace_back(_timeBack);

	Frames& datas = _datas[track - 1];
	// flush before emplace_back
	flush(onWrite, !_buffering);
	shared<Buffer> pBuffer(SET);
	BinaryWriter writer(*pBuffer);
	writer.write16(packet.size()); // subtitle size on 2 bytes
	writer.write(*packet.buffer());
	datas.push(_timeBack, Packet(pBuffer)); // TODO: this is an approximation of time, add a new type Subtitles with time?
	// TODO: Add styl from ASS styles?
}

bool MP4Writer::computeSizeMoof(deque<Frames>& tracks, Int8 reset, UInt32& sizeMoof) {
	for (Frames& frames : tracks) {
		if (!frames)
			continue;
		if (_sequence<14) // => trick to delay firefox play and bufferise more than 2 seconds before to start playing (when hasKey flags absent firefox starts to play! doesn't impact other browsers)
			frames.hasKey = true;
		if (frames.empty()) {
			if (!reset) {
				_bufferMinSize *= 2;
				if (_bufferMinSize < BUFFER_RESET_SIZE)
					return false; // wait bufferTime to get at less one media on this track!
				_bufferMinSize = BUFFER_MIN_SIZE;
				DEBUG("MP4 track removed");
			}
			_buffering = true; // just to write header now!
			frames = nullptr;
		} else
			sizeMoof += frames.sizeTraf();
	}
	return true;
}

void MP4Writer::flush(const OnWrite& onWrite, Int8 reset) {
	if (!_started) {
		_started = true;
		_timeFront = _timeBack;
	}

	UInt16 delta = Util::Distance(_timeFront, _timeBack);
	if (!reset && delta < (_buffering ? _buffering : _bufferMinSize))
		return;

	// Search if there is empty track => MSE requires to get at less one media by track on each segment
	// In same time compute sizeMoof!
	UInt32 sizeMoof = 0;
	if (!computeSizeMoof(_videos, reset, sizeMoof) || !computeSizeMoof(_audios, reset, sizeMoof))
		return;// wait additionnal buffering
	// INFO(delta, " ", _audios[0].size(), " ", _videos[0].size());

	for (Frames& datas : _datas) {
		if (!datas.empty())
			sizeMoof += datas.sizeTraf();
	}
	if (!sizeMoof) {
		// nothing to write!
		_buffering = BUFFER_RESET_SIZE;
		return;
	}

	shared<Buffer> pBuffer(SET);
	BinaryWriter writer(*pBuffer);

	UInt16 track(0);

	if (_buffering) {
		// fftyp box => iso5....iso6mp41
		writer.write(EXPAND("\x00\x00\x00\x18""ftyp\x69\x73\x6F\x35\x00\x00\x02\x00""iso6mp41"));

		// moov
		UInt32 sizePos = writer.size();
		writer.next(4); // skip size!
		writer.write(EXPAND("moov"));
		{	// mvhd
			writer.write(EXPAND("\x00\x00\x00\x6c""mvhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\xe8\x00\x00\x00\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			writer.write32(_audios.size() + _videos.size() + 1); // next track id
		}

		// VIDEOS
		for (Frames& videos : _videos) {
			if (!videos)
				continue;
			videos.writeConfig = true;
			// trak
			UInt32 sizePos = writer.size();
			writer.next(4); // skip size!
			writer.write(EXPAND("trak"));

			Packet sps, pps, vps;
			UInt32 dimension = 0;
			if (videos.config && ((videos.codec == Media::Video::CODEC_H264 && AVC::ParseVideoConfig(videos.config, sps, pps)) || (videos.codec == Media::Video::CODEC_HEVC && HEVC::ParseVideoConfig(videos.config, vps, sps, pps))))
				dimension = (videos.codec == Media::Video::CODEC_H264)? AVC::SPSToVideoDimension(sps.data(), sps.size()) : HEVC::SPSToVideoDimension(sps.data(), sps.size());
			else
				WARN("No avcC configuration");

			{ // tkhd
				writer.write(EXPAND("\x00\x00\x00\x5c""tkhd\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00"));
				writer.write32(++track);
				writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00"));
				writer.write16(dimension >> 16).write16(0); // width
				writer.write16(dimension & 0xFFFF).write16(0); // height
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

							// avc1/avc2 => get config packet in moov
							// avc3/avc4 => allow config packet dynamically in the stream itself
							// The sample entry name ‘avc1’ or 'avc3' may only be used when the stream to which this sample entry applies is a compliant and usable AVC stream as viewed by an AVC decoder operating under the configuration (including profile and level) given in the AVCConfigurationBox. The file format specific structures that resemble NAL units (see Annex A) may be present but must not be used to access the AVC base data; that is, the AVC data must not be contained in Aggregators (though they may be included within the bytes referenced by the additional_bytes field) nor referenced by Extractors.
							// The sample entry name ‘avc2’ or 'avc4' may only be used when Extractors or Aggregators (Annex A) are required to be supported, and an appropriate Toolset is required (for example, as indicated by the file-type brands). This sample entry type indicates that, in order to form the intended AVC stream, Extractors must be replaced with the data they are referencing, and Aggregators must be examined for contained NAL Units. Tier grouping may be present.			

							{ // avc1 TODO => switch to avc3 when players are ready? (test video config packet inband!)	
								UInt32 sizePos = writer.size();
								writer.next(4); // skip size!
								// hev1/avc1
								// 00 00 00 00 00 00	reserved (6 bytes)
								// 00 01				data reference index (2 bytes)
								// 00 00 00 00			version + revision level
								// 00 00 00 00			vendor
								// 00 00 00 00			temporal quality
								// 00 00 00 00			spatial quality
								if (vps)
									writer.write(EXPAND("hev1\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
								else
								writer.write(EXPAND("avc1\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
								// 05 00 02 20			width + height
								writer.write32(dimension); // width + height	
								// 00 00 00 00			horizontal resolution
								// 00 00 00 00			vertical resolution
								// 00 00 00 00			data size
								// 00 01				frame by sample, always 1
								writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01"));
								// 32 x 0				32 byte pascal string - compression name
								writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
								// 00 18				depth (24 bits)
								// FF FF				default color table
								writer.write(EXPAND("\x00\x18\xFF\xFF"));

								if (vps) {
									UInt32 sizePos = writer.size();
									BinaryWriter(pBuffer->data() + sizePos, 4).write32(HEVC::WriteVideoConfig(writer.next(4).write(EXPAND("hvcC")), vps, sps, pps).size() - sizePos);
								}
								else if (sps) {
									// file:///C:/Users/mathieu/Downloads/standard8978%20(1).pdf => 5.2.1.1
									UInt32 sizePos = writer.size();
									BinaryWriter(pBuffer->data() + sizePos, 4).write32(AVC::WriteVideoConfig(writer.next(4).write(EXPAND("avcC")), sps, pps).size() - sizePos);
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
		for (Frames& audios : _audios) {
			if (!audios)
				continue;
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

					UInt32 size(223);
					Packet config;
					if (audios.config) {
						if (audios.config.size() <= 0xFF) {
							config = audios.config;
							size += config.size() + 2;
						} else
							WARN("Audio config with size of ", audios.config.size(), " too large for mp4a/esds box")
					} else if (audios.codec == Media::Audio::CODEC_AAC)
							WARN("Audio AAC track without any configuration packet preloaded");

					writer.write32(size);
					writer.write(EXPAND("minf\x00\x00\x00\x10""smhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x24""dinf\x00\x00\x00\x1C""dref\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x0C""url \x00\x00\x00\x01"));


					writer.write32(size -= 60);
					writer.write(EXPAND("stbl"));

					writer.write32(size -= 76);
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
					// writer.write(EXPAND(".mp3\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x10\x00\x00\x00\x00"));
					// BB 80 00 00		 rate
					writer.write16(range<UInt16>(audios.rate)).write16(0); // just a introduction indication, config packet is more precise!
																									// 00 00 00 27		 length
					writer.write32(size -= 36);
					// 65 73 64 73		 esds
					// 00 00 00 00		 version
					// http://www.etsi.org/deliver/etsi_ts/102400_102499/102428/01.01.01_60/ts_102428v010101p.pdf
					// 03				 
					writer.write(EXPAND("esds\x00\x00\x00\x00\x03"));
					// 25				 length
					writer.write8(size -= 14);
					// 00 02			 ES ID
					writer.write16(track);
					// 00				 flags + stream priority
					// 04				 decoder config descriptor
					writer.write(EXPAND("\x00\x04"));
					// 11			 length
					writer.write8(size -= 8); // size includes just decoder config description and audio config desctription

					// decoder config descriptor =>
					// http://xhelmboyx.tripod.com/formats/mp4-layout.txt
					// http://www.mp4ra.org/object.html
					// 40 MPEG4 audio, 69 MPEG2 audio
					writer.write8(audios.codec == Media::Audio::CODEC_AAC ? 0x40 : 0x69);
					// 15 Audio!
					// 00 00 00 buffer size = 0
					// 00 00 00 00 => max bitrate
					// 00 00 00 00 => average bitrate
					writer.write(EXPAND("\x15\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));

					if (config) {
						// 05				Audio config descriptor
						// 02				length
						// 11 B0			audio specific config
						writer.write8(5);
						writer.write8(config.size()).write(config);
					}
					// 06				SL config descriptor
					// 01				length
					// 02				flags
					writer.write(EXPAND("\x06\x01\x02"));


					// stts + stsc + stsz + stco =>
					writer.write(EXPAND("\x00\x00\x00\x10""stts\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stsc\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14""stsz\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stco\x00\x00\x00\x00\x00\x00\x00\x00"));

				}  // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
				BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
			} // mdia
			BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
		} // AUDIOS

		// DATAS
		for (Frames& datas : _datas) {
			if (datas.empty())
				continue;
			// trak
			writer.write32(385);
			writer.write(EXPAND("trak"));
			{ // tkhd
				writer.write(EXPAND("\x00\x00\x00\x5c""tkhd\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00"));
				writer.write32(++track);
				writer.write(EXPAND("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
			}
			{ // mdia
				writer.write32(285);
				writer.write(EXPAND("mdia"));
				{	// mdhd
					writer.write(EXPAND("\x00\x00\x00\x20""mdhd\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
					writer.write32(1000); // timescale, precision 1ms
					writer.write(EXPAND("\x00\x00\x00\x00")); // duration
					writer.write16(0x55C4); // TODO lang (0x55C4 = undefined)
					writer.write16(0); // predefined
				}
				{	// hdlr
					writer.write(EXPAND("\x00\x00\x00\x21""hdlr\x00\x00\x00\x00\x00\x00\x00\x00""sbtl\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
				}
				{ // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
					writer.write32(212);
					writer.write(EXPAND("minf\x00\x00\x00\x0C""nmhd\x00\x00\x00\x00\x00\x00\x00\x24""dinf\x00\x00\x00\x1C""dref\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x0C""url \x00\x00\x00\x01"));
					{ // stbl
						writer.write32(156);
						writer.write(EXPAND("stbl"));
						{ // stsd
							writer.write32(80);
							writer.write(EXPAND("stsd\x00\x00\x00\x00\x00\x00\x00\x01"));

							// tx3g
							writer.write32(64);
							writer.write(EXPAND("tx3g\x00\x00\x00\x00\x00\x00\x00\x01"));

							/*
							* For now, we'll use a fixed default style. When we add styling
							* support, this will be generated from the ASS style.
							*/
							writer.write(EXPAND("\x00\x00\x00\x00")); // uint32_t displayFlags
							writer.write(EXPAND("\x01")); // int8_t horizontal-justification
							writer.write(EXPAND("\xFF")); // int8_t vertical-justification
							writer.write(EXPAND("\x00\x00\x00\x00")); // uint8_t background-color-rgba[4]
							writer.write(EXPAND("\x00\x00")); // int16_t top
							writer.write(EXPAND("\x00\x00")); // int16_t left
							writer.write(EXPAND("\x00\x00")); // int16_t bottom
							writer.write(EXPAND("\x00\x00")); // int16_t right
							writer.write(EXPAND("\x00\x00")); // uint16_t startChar
							writer.write(EXPAND("\x00\x00")); // uint16_t endChar
							writer.write(EXPAND("\x00\x01")); // uint16_t font-ID
							writer.write(EXPAND("\x00")); // uint8_t face-style-flags
							writer.write(EXPAND("\x12")); // uint8_t font-size
							writer.write(EXPAND("\xFF\xFF\xFF\xFF")); // uint8_t text-color-rgba[4]
							writer.write(EXPAND("\x00\x00\x00\x12")); // uint32_t size
							writer.write(EXPAND("ftab")); // uint8_t name[4]
							writer.write(EXPAND("\x00\x01")); // uint16_t entry-count
							writer.write(EXPAND("\x00\x01")); // uint16_t font-ID
							writer.write(EXPAND("\x05")); // uint8_t font-name-length
							writer.write(EXPAND("Serif"));	// uint8_t font[font-name-length]
						} // stsd
						  // stts + stsc + stsz + stco =>
						writer.write(EXPAND("\x00\x00\x00\x10""stts\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stsc\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14""stsz\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10""stco\x00\x00\x00\x00\x00\x00\x00\x00"));
					}
				}  // minf + smhd + dinf + dref + url + stbl + stsd + stts + stsc + stsz + stco
			} // mdia
		} // DATAS

		// MVEX is required by spec => https://www.w3.org/TR/mse-byte-stream-format-isobmff/
		writer.write32(8 + (track * 32)); // size of mvex
		writer.write(EXPAND("mvex")); // mvex
		do { // track necessary superior to 0!
			// trex
			writer.write(EXPAND("\x00\x00\x00\x20""trex\x00\x00\x00\x00"));
			writer.write32(track--);
			writer.write(EXPAND("\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"));
		} while (track);

		BinaryWriter(pBuffer->data() + sizePos, 4).write32(writer.size() - sizePos);
	}
	if (reset) {
		if (reset < 0) { // end
			// End all tracks on the same _timeBack time to add a silence to allow on a onEnd/onBegin without interval (smooth transition, especially on chrome)
			_timeBack += BUFFER_MIN_SIZE;
			_buffering = bufferTime;
		} else {
			DEBUG("MP4 dynamic configuration change");
			_buffering = BUFFER_RESET_SIZE;
		}
	} else
		_buffering = 0;

	//////////// MOOF /////////////
	writer.write32(sizeMoof+=24);
	writer.write(EXPAND("moof"));
	{	// mfhd
		writer.write(EXPAND("\x00\x00\x00\x10""mfhd\x00\x00\x00\x00"));
		writer.write32(++_sequence); // starts from 1!
	}

	UInt32 dataOffset(sizeMoof + 8); // 8 for [size]mdat
	for (Frames& videos : _videos) {
		if(videos)
			writeTrack(writer, ++track, videos, dataOffset, reset<0);
	}
	for (Frames& audios : _audios) {
		if(audios)
			writeTrack(writer, ++track, audios, dataOffset, reset<0);
	}
	for (Frames& datas : _datas) {
		if (datas.size())
			writeTrack(writer, ++track, datas, dataOffset, reset<0);
	}

	// MDAT
	writer.write32(dataOffset - sizeMoof);
	writer.write(EXPAND("mdat"));

	vector<deque<Frame>> mediaFrames(track);
	track = 0;
	for (Frames& videos : _videos) {
		if (videos)
			mediaFrames[track++] = videos.flush();
	}
	for (Frames& audios : _audios) {
		if (audios)
			mediaFrames[track++] = audios.flush();
	}
	for (Frames& datas : _datas) {
		if (datas.size())
			mediaFrames[track++] = datas.flush();
	}
	_timeFront = _timeBack; // set timestamp progression

	// onWrite in last to avoid possible recursivity (for example if onWrite call flush again: _videos or _audios not flushed!)
	if (!onWrite)
		return;
	// header
	onWrite(Packet(pBuffer)); 
	// payload
	for (const deque<Frame>& frames : mediaFrames) {
		for (const Frame& frame : frames)
			onWrite(frame);
	}
}

void MP4Writer::writeTrack(BinaryWriter& writer, UInt32 track, Frames& frames, UInt32& dataOffset, bool isEnd) {
	UInt32 position = writer.size();
	UInt32 sizeTraf = frames.sizeTraf();
	writer.write32(sizeTraf); // skip size!
	writer.write(EXPAND("traf"));
	{ // tfhd
		writer.write(EXPAND("\x00\x00\x00\x10""tfhd\x00\x02\x00\x00")); // 020000 => default_base_is_moof
		writer.write32(track);
	}
	// frames.front().time is necessary a more upper time than frames.lastTime because non-monotonic packet are removed in writeAudio/writeVideo
	Int32 delta = Util::Distance(frames.lastTime, frames.front().time) - frames.lastDuration;
	if (!frames.codec && delta < 0)
		delta = 0; // Data track, just "duration conflict" has to be fixed (delta>0)
	if (abs(delta)>6) { // if lastTime == _timeFront it's a silence added intentionaly to sync audio and video
		if(frames.codec) {}
		if (!frames.lastDuration && frames.lastTime == _timeFront)
			INFO("Add ", delta, "ms of silence to track ", track)
		else
			WARN("Timestamp delta ", delta, " superior to 6 (", frames.lastDuration, "=>", (frames.front().time - frames.lastTime), ")");
	}
	{ // tfdt => required by https://w3c.github.io/media-source/isobmff-byte-stream-format.html
	  // http://www.etsi.org/deliver/etsi_ts/126200_126299/126244/10.00.00_60/ts_126244v100000p.pdf
	  // when any 'tfdt' is used, the 'elst' box if present, shall be ignored => tfdt time manage the offset!
		writer.write(EXPAND("\x00\x00\x00\x10""tfdt\x00\x00\x00\x00"));
		writer.write32(frames.front().time - delta);
	}
	{ // trun
		writer.write32(sizeTraf - (writer.size()- position));
		writer.write(EXPAND("trun"));
		UInt32 flags = 0x00000301; // flags = sample duration + sample size + data-offset
		if (frames.hasCompositionOffset)
			flags |= 0x00000800;
		if (frames.hasKey)
			flags |= 0x00000400;
		writer.write32(flags); // flags
		writer.write32(frames.size()); // array length
		writer.write32(dataOffset); // data-offset

		const Frame* pFrame = NULL;
		UInt32 size = frames.writeConfig ? frames.config.size() : 0;
		for (const Frame& nextFrame : frames) {
			if (!pFrame) {
				pFrame = &nextFrame;
				continue;
			}
			// medias is already a list sorted by time, so useless to check here if pMedia->tag.time inferior to pNext.time
			delta = writeFrame(writer, frames, size += pFrame->size(), pFrame->isSync,
				nextFrame.time - pFrame->time,
				pFrame->compositionOffset, delta);
			dataOffset += size;
			size = 0;
			pFrame = &nextFrame;
		}
		// write last
		if (isEnd) // at the end all the tracks must finish to the same time to get smooth transition, here force to finish to timeBack
			frames.lastDuration = 0;
		frames.lastTime = pFrame->time - writeFrame(writer, frames, size += pFrame->size(), pFrame->isSync,
			frames.lastDuration ? frames.lastDuration : (_timeBack - pFrame->time),
			pFrame->compositionOffset, delta);
		dataOffset += size;
	}
}

Int32 MP4Writer::writeFrame(BinaryWriter& writer, Frames& frames, UInt32 size, bool isSync, UInt32 duration, UInt32 compositionOffset, Int32 delta) {
	// duration
	frames.lastDuration = duration;
	if (delta >= 0 || duration > UInt32(-delta)) {
		duration += delta;
		delta = 0;
	} else { // delta<=-duration
		delta += duration;
		duration = 0;
	}
	writer.write32(duration);
	writer.write32(size); // size
	// 0x01010000 => no-key => sample_depends_on YES | sample_is_difference_sample
	// 0X02000000 => key or audio => sample_depends_on NO
	if (frames.hasKey)
		writer.write32(isSync ? 0x02000000 : 0x1010000);
	if (frames.hasCompositionOffset)
		writer.write32(compositionOffset);
	return delta;
}



} // namespace Mona
