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
	if (writeConfig && !frames.empty()) {
		if(config)
			frames.emplace_front(config);
		writeConfig = false;
	}
	return frames;
}

void MP4Writer::beginMedia(const OnWrite& onWrite) {
	_reset = false;
	_buffering = true;
	_sequence = 0;
	_errors = 0;
	_firstTime = true;
	_timeBack = 0;
}

void MP4Writer::endMedia(const OnWrite& onWrite) {
	if(!_firstTime) {
		_timeBack = _timeFront + 1000; // to force flush!
		flush(onWrite);
	}
	// release resources
	_videos.clear();
	_audios.clear();
}

void MP4Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	
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

	//INFO(tag.isConfig ? "Audio config " : "Audio ", tag.time);

	if (track > _audios.size()) {
		if (_sequence)
			_reset = true;
		_audios.emplace_back(tag.codec);
	}

	Frames& audios = _audios[track - 1];
	if (Util::Distance(audios.empty() ? audios.lastTime : audios.back()->time(), tag.time)<0) {
		WARN("Non-monotonic audio timestamp, packet ignored");
		return;
	}
	if (audios.codec && audios.codec != tag.codec)
		_reset = true;
	if (tag.isConfig) {
		if (audios.config != packet) {
			if (audios.config || _sequence)
				_reset = true; // force flush if "config" change OR after first sequence!
			audios.config = move(packet);
		}
	} else if (tag.time != _timeBack) {
		_timeBack = tag.time;
		flush(onWrite); // flush before emplace_back and just if time progression (something to flush!)
	}

	audios.codec = tag.codec;
	if (!audios.rate && tag.rate)
		audios.rate = tag.rate;
	if (!tag.isConfig)
		audios.push(tag, packet);
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
	
	if (track > _videos.size()) {
		if (_sequence)
			_reset = true;
		_videos.emplace_back(tag.codec);
	}

	Frames& videos = _videos[track - 1];
	// NOTE(tag.frame == Media::Video::FRAME_CONFIG ? "Video config " : "Video ", tag.frame, ' ', tag.time, " (", tag.time - (videos.empty() ? videos.lastTime : videos.back()->time()), ")");
	if (Util::Distance(videos.empty() ? videos.lastTime : videos.back()->time(), tag.time) < 0) {
		WARN("Non-monotonic video timestamp, packet ignored");
		return;
	}

	if (tag.frame == Media::Video::FRAME_CONFIG) {
		if (videos.config != packet) {
			if (videos.config || _sequence)
				_reset = true; // force flush if "config" change OR after first sequence!
			videos.config = move(packet);
		}
	} else if (_timeBack != tag.time) {
		_timeBack = tag.time;
		flush(onWrite); // flush before emplace_back and just if time progression (something to flush!)
	}

	videos.codec = tag.codec;
	if (tag.frame == Media::Video::FRAME_CONFIG)
		return;
	if (tag.frame == Media::Video::FRAME_KEY)
		videos.hasKey = true;
	if (tag.compositionOffset)
		videos.hasCompositionOffset = true;
	videos.push(tag, packet);
}

void MP4Writer::flush(const OnWrite& onWrite) {
	if (_firstTime) {
		_timeFront = _timeBack;
		_firstTime = false;
		return;
	}
	Int32 delta = Util::Distance(_timeFront, _timeBack);
	if (!delta)
		return; // no medias!
	if (!_reset &&  delta < ((_buffering && !_sequence)  ? bufferTime : 100)) // wait one second to get at less one video frame the first time (1fps is the min possibe for video)
		return;

	shared<Buffer> pBuffer(new Buffer());
	BinaryWriter writer(*pBuffer);

	UInt16 track(0);

	if (!_sequence) {
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

	// _sequence>10 => trick to do working firefox correctly in live video mode (fix the bufferization, TODO fix firefox?), _sequence>10 to get 2 seconds of bufferization (no impact on others browsers)
	if (_buffering && _sequence >= 10)
		_buffering = false;

	//////////// MOOF /////////////
	UInt32 sizeMoof = 24;

	for (Frames& videos : _videos) {
		if (_buffering)
			videos.hasKey = true;
		if (!videos.empty())
			sizeMoof += videos.sizeTraf = 60 + (videos.size() * (videos.hasCompositionOffset ? (videos.hasKey ? 16 : 12) : (videos.hasKey ? 12 : 8)));
	}
	for (Frames& audios : _audios) {
		if (!audios.empty())
			sizeMoof += audios.sizeTraf = 60 + (audios.size() * 8);
	}

	writer.write32(sizeMoof);
	writer.write(EXPAND("moof"));
	{	// mfhd
		writer.write(EXPAND("\x00\x00\x00\x10""mfhd\x00\x00\x00\x00"));
		writer.write32(++_sequence); // starts to 1!
	}

	UInt32 dataOffset(sizeMoof + 8); // 8 for [size]mdat
	for (Frames& videos : _videos)
		writeTrack(writer, ++track, videos, dataOffset);
	for (Frames& audios : _audios)
		writeTrack(writer, ++track, audios, dataOffset);


	// MDAT
	writer.write32(dataOffset - sizeMoof);
	writer.write(EXPAND("mdat"));

	vector<deque<Frame>> mediaFrames;
	for (Frames& videos : _videos)
		mediaFrames.emplace_back(videos.flush());
	for (Frames& audios : _audios)
		mediaFrames.emplace_back(audios.flush());
	_timeFront = _timeBack;
	if (_reset) {
		_reset = false;
		_sequence = 0;
		DEBUG("MP4 dynamic configuration change");
	}

	// onWrite in last to avoid possible recursivity (for example if onWrite call flush again and _videos or _audios are not empty!)
	if (!onWrite)
		return;
	// header
	onWrite(Packet(pBuffer)); 
	if (!_sequence)
		return; // has flush!
	// payload
	for (const deque<Frame>& frames : mediaFrames) {
		for (const Frame& frame : frames) {
			onWrite(*frame);
			if (!_sequence)
				return; // has flush!
		}
	}
}

void MP4Writer::writeTrack(BinaryWriter& writer, UInt32 track, Frames& frames, UInt32& dataOffset) {
	if (frames.empty())
		return;
	writer.write32(frames.sizeTraf); // skip size!
	writer.write(EXPAND("traf"));
	{ // tfhd
		writer.write(EXPAND("\x00\x00\x00\x10""tfhd\x00\x02\x00\x00")); // 020000 => default_base_is_moof
		writer.write32(track);
	}
	// frames.front()->time() is necessary a more upper time than frames.lastTime because non-monotonic packet are removed in writeAudio/writeVideo
	Int32 delta = (frames.front()->time() - frames.lastTime) - frames.lastDuration;
	if (abs(delta) > 2)
		WARN("Timestamp delta ",delta, " superior to 2 (", (frames.front()->time() - frames.lastTime) , " - ", frames.lastDuration,")");
	{ // tfdt => required by https://w3c.github.io/media-source/isobmff-byte-stream-format.html
	  // http://www.etsi.org/deliver/etsi_ts/126200_126299/126244/10.00.00_60/ts_126244v100000p.pdf
	  // 13.5
		writer.write(EXPAND("\x00\x00\x00\x10""tfdt\x00\x00\x00\x00"));
		writer.write32(frames.front()->time() - delta);
	}
	{ // trun
		writer.write32(frames.sizeTraf-40);
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
			// medias is already a list sorted by time, so useless to check here if pMedia->tag.time inferior to pNext->time()
			delta = writeFrame(writer, frames, size += (*pFrame)->size(), pFrame->isSync,
				nextFrame->time() - (*pFrame)->time(),
				(*pFrame)->compositionOffset(), delta);
			dataOffset += size;
			size = 0;
			pFrame = &nextFrame;
		}
		// write last
		frames.lastTime = (*pFrame)->time() - writeFrame(writer, frames, size += (*pFrame)->size(), pFrame->isSync,
			frames.lastDuration ? frames.lastDuration : (_timeBack - (*pFrame)->time()),
			(*pFrame)->compositionOffset(), delta);
		dataOffset += size;
	}
}

Int32 MP4Writer::writeFrame(BinaryWriter& writer, Frames& frames, UInt32 size, bool isSync, UInt32 duration, UInt32 compositionOffset, Int32 delta) {
	// duration
	frames.lastDuration = duration;
	if (delta>=0 || duration > UInt32(-delta)) {
		writer.write32(duration + delta);
		delta = 0;
	} else { // delta<-duration
		writer.write32(0);
		delta += duration;
	}
	writer.write32(size); // size
	// 0x01010000 => no-key => sample_depends_on YES | sample_is_difference_sample
	// 0X02000000 => key or audio => sample_depends_on NO
	// _sequence>10 => trick to do working firefox correctly in live video mode (fix the bufferization, TODO fix firefox?), _sequence>10 to get 2 seconds of bufferization (no impact on others browsers)
	if (frames.hasKey)
		writer.write32(isSync ? 0x02000000 : (_buffering ? 0x10000 : 0));
	if (frames.hasCompositionOffset)
		writer.write32(compositionOffset);
	return delta;
}



} // namespace Mona
