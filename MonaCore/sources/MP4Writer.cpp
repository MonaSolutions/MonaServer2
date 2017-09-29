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

void MP4Writer::Frames::emplace_back(const Media::Audio::Tag& tag, const Packet& packet) {
	if (!_firstRate)
		_firstRate = tag.rate;

	// sort by times!
	auto it = end();
	while (it != begin()) {
		if (tag.time < (--it)->time())
			continue;
		++it;
		break;
	}

	// attach config packet!
	if (tag.isConfig) {
		if (it != end() && ++it!=end())
			it->config = move(packet);
		else
			config = move(packet);
		return;
	}

	if (it == end() && config) {
		deque<Frame>::emplace_back(tag, packet);
		back().config = move(config);
		config = nullptr;
	} else
		deque<Frame>::emplace(it, tag, packet);
}

void MP4Writer::Frames::emplace_back(const Media::Video::Tag& tag, const Packet& packet) {
	// sort by times!
	auto it = end();
	while (it != begin()) {
		if (tag.time < (--it)->time())
			continue;
		++it;
		break;
	}
	// attach config packet!
	if (tag.frame == Media::Video::FRAME_CONFIG) {
		if (it != end() && ++it != end())
			it->config = move(packet);
		else
			config = move(packet);
		return;
	}
	if (tag.compositionOffset)
		hasCompositionOffset = true;

	if (it == end() && config) {
		deque<Frame>::emplace_back(tag, packet);
		back().config = move(config);
		config = nullptr;
	} else
		deque<Frame>::emplace(it, tag, packet);
}

void MP4Writer::Frames::clear() {
	hasCompositionOffset = false;
	deque<Frame>::clear();
}

void MP4Writer::beginMedia(const OnWrite& onWrite) {
	_audioOutOfRange = _videoOutOfRange = false;
	_sequence = 0;
	_errors = 0;
	_timeFront = _timeBack = 0;
	_firstTime = true;
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
		if (!(_errors & 4)) {
			_errors |= 4;
			WARN("Audio codec ",Media::Audio::CodecToString(tag.codec)," ignored, Web MP4 supports currently just AAC and MP3 codecs");
		}
		return;
	}
	if (!track)
		++track; // 1 based!

	// INFO("Audio ", tag.time);

	if (track > _audios.size()) {
		if (_sequence) {
			if (!(_errors&1)) {
				_errors |= 1;
				WARN("Audio track superior to ", _audios.size()," are ignored, MP4 doesn't support dynamic track addition");
			}
			return;
		}
		_audios.emplace_back(tag.codec);
	}
	Frames& audios = _audios[track - 1];

	if (audios.codec != tag.codec) {
		if (!(_errors & 16)) {
			_errors |= 16;
			WARN("MP4 doesn't support dynamic codec change");
		}
		return;
	}

	if (!tag.isConfig) { // config packet has not a safe time information
		if (tag.time >= _timeBack) {
			_timeBack = tag.time;
			flush(onWrite); // flush before emplace_back
		}  // else _timeBack unchanged, no flush possible
	} else {
		if (!packet) {
			WARN("MP4 current implementation doesn't support dynamic noise suppression");
			return; // ignore empty config packet (silence signal)
		}
		if (!_sequence)
			WARN("Audio dynamic configuration change is not supported by MP4 format")
	}

	if(tag.time<_timeFront) {
		WARN("Timestamp already played, audio ignored");
		return;
	}
	audios.emplace_back(tag, packet);
}

void MP4Writer::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (tag.codec != Media::Video::CODEC_H264) {
		if (!(_errors & 8)) {
			_errors |= 8;
			WARN("Video codec ", Media::Video::CodecToString(tag.codec), " ignored, Web MP4 supports just H264 codec");
		}
		return;
	}
	if (!track)
		++track; // 1 based!

	// NOTE("Video ", tag.time);

	if (track > _videos.size()) {
		if (_sequence) {
			if (!(_errors & 2)) {
				_errors |= 2;
				WARN("Video track superior to ", _videos.size(), " are ignored, MP4 doesn't support dynamic track addition");
			}
			return;
		}
		_videos.emplace_back(tag.codec);
	}
	Frames& videos = _videos[track - 1];

	if (videos.codec != tag.codec) {
		if (!(_errors & 16)) {
			_errors |= 16;
			WARN("MP4 doesn't support dynamic codec change");
		}
		return;
	}

	if (tag.frame != Media::Video::FRAME_CONFIG) { // config packet has not a safe time information
		if(tag.time >= _timeBack) {
			_timeBack = tag.time;
			flush(onWrite); // flush before emplace_back
		} // else _timeBack unchanged, no flush possible
	} else if (!packet)
		return; // ignore empty config packet (silence signal)

	if (tag.time<_timeFront) {
		WARN("Timestamp already played, video ignored");
		return;
	}
	videos.emplace_back(tag, packet);
}

void MP4Writer::flush(const OnWrite& onWrite) {
	if (_firstTime) {
		_timeFront = _timeBack;
		_firstTime = false;
		return;
	}

	// flush after 1 seconds on loading, and 100ms then
	if ((_timeBack - _timeFront) < (_sequence ? 100u : 1000u)) // Wait one second
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

							// avc1/avc2 => get config packet in moov
							// avc3/avc4 => allow config packet dynamically in the stream itself
							// The sample entry name ‘avc1’ or 'avc3' may only be used when the stream to which this sample entry applies is a compliant and usable AVC stream as viewed by an AVC decoder operating under the configuration (including profile and level) given in the AVCConfigurationBox. The file format specific structures that resemble NAL units (see Annex A) may be present but must not be used to access the AVC base data; that is, the AVC data must not be contained in Aggregators (though they may be included within the bytes referenced by the additional_bytes field) nor referenced by Extractors.
							// The sample entry name ‘avc2’ or 'avc4' may only be used when Extractors or Aggregators (Annex A) are required to be supported, and an appropriate Toolset is required (for example, as indicated by the file-type brands). This sample entry type indicates that, in order to form the intended AVC stream, Extractors must be replaced with the data they are referencing, and Aggregators must be examined for contained NAL Units. Tier grouping may be present.			

							{ // avc1 TODO => switch to avc3 when players are ready? (test video config packet inband!)	
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

								Packet sps, pps, &config(videos.empty() ? videos.config : videos.front().config);
								if (config && MPEG4::ParseVideoConfig(config, sps, pps)) {
									config = nullptr;
									// file:///C:/Users/mathieu/Downloads/standard8978%20(1).pdf => 5.2.1.1
									UInt32 sizePos = writer.size();
									BinaryWriter(pBuffer->data() + sizePos, 4).write32(MPEG4::WriteVideoConfig(sps, pps, writer.next(4).write(EXPAND("avcC"))).size() - sizePos);
								} else
									WARN("No avcC configuration");
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
					Packet config, &packet(audios.empty() ? audios.config : audios.front().config);
					if (packet) {
						if (packet.size() <= 0xFF) {
							config = move(packet);
							size += config.size() + 2;
							packet = nullptr;
						} else
							WARN("Audio config with size of ", packet.size(), " too large for mp4a/esds box")
					} else if (audios.codec == Media::Audio::CODEC_AAC)
							WARN("Audio AAC track without any configuration packet prealoaded");

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
					writer.write16(audios.empty() ? 0 : range<UInt16>(audios.firstRate())).write16(0); // just a introduction indication, config packet is more precise!
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


	//////////// MOOF /////////////
	UInt32 sizeMoof = 24;
	
	for (Frames& videos : _videos) {
		if (!videos.empty())
			sizeMoof += videos.sizeTraf = 64 + (videos.size() * (videos.hasCompositionOffset ? 12 : 8));
	}
	for (Frames& audios : _audios) {
		if (!audios.empty())
			sizeMoof += audios.sizeTraf = 64 + (audios.size() * 8);
	}

	writer.write32(sizeMoof);
	writer.write(EXPAND("moof"));
	{	// mfhd
		writer.write(EXPAND("\x00\x00\x00\x10""mfhd\x00\x00\x00\x00"));
		writer.write32(_sequence++);
	}

	UInt32 dataOffset(sizeMoof+8); // 8 for [size]mdat
	for (Frames& videos : _videos)
		writeTrack(++track, videos, dataOffset, *pBuffer);
	for (Frames& audios : _audios)
		writeTrack(++track, audios, dataOffset, *pBuffer);


	// MDAT
	writer.write32(dataOffset - sizeMoof);
	writer.write(EXPAND("mdat"));

	if(onWrite)
		onWrite(Packet(pBuffer));
	for (Frames& videos : _videos) {
		if (onWrite) {
			for (const Frame& video : videos) {
				if(video.config)
					onWrite(video.config);
				onWrite(video.data());
			}
		}
		videos.clear();
	}
	for (Frames& audios : _audios) {
		if (onWrite) {
			for (const Frame& audio : audios) {
				if (audio.config)
					onWrite(audio.config);
				onWrite(audio.data());
			}
		}
		audios.clear();
	}
	_timeFront = _timeBack;
}

void MP4Writer::writeTrack(UInt32 track, Frames& frames, UInt32& dataOffset, Buffer& buffer) {
	if (frames.empty())
		return;
	BinaryWriter writer(buffer);
	writer.write32(frames.sizeTraf); // skip size!
	writer.write(EXPAND("traf"));
	{ // tfhd
		writer.write(EXPAND("\x00\x00\x00\x14""tfhd\x00\x02\x00\x20")); // 020020 => default_base_is_moof + default sample flags present
		writer.write32(track);
		writer.write32(_sequence>10 ? 0 : 0x10000); // default sample flags => trick to do working firefox correctly (fix the bufferization, TODO fix firefox?), _sequence>10 to get 2 seconds of bufferization (no impact on others browsers)
	}
	{ // tfdt => required by https://w3c.github.io/media-source/isobmff-byte-stream-format.html
	  // http://www.etsi.org/deliver/etsi_ts/126200_126299/126244/10.00.00_60/ts_126244v100000p.pdf
	  // 13.5
		writer.write(EXPAND("\x00\x00\x00\x10""tfdt\x00\x00\x00\x00"));
		writer.write32(frames.front().time());
	}
	{ // trun
		writer.write32(frames.sizeTraf-44);
		writer.write(EXPAND("trun"));
		UInt32 flags = 0x00000301; // flags = sample duration + sample size + data-offset
		if (frames.hasCompositionOffset)
			flags |= 0x00000800;
		writer.write32(flags); // flags
		writer.write32(frames.size()); // array length
		writer.write32(dataOffset); // data-offset

		const Frame* pFrame = NULL;
		for (const Frame& nextFrame : frames) {
			if (!pFrame) {
				pFrame = &nextFrame;
				continue;
			}
			// medias is already a list sorted by time, so useless to check here if pMedia->tag.time inferior to pNext->time()
			writer.write32(nextFrame.time() - pFrame->time()); // duration
			writer.write32(pFrame->size()); // size
			if (frames.hasCompositionOffset)
				writer.write32(pFrame->compositionOffset());
			dataOffset += pFrame->size();
			pFrame = &nextFrame;
		}
		if (pFrame) {
			writer.write32(_timeBack - pFrame->time()); // duration
			writer.write32(pFrame->size()); // size
			if (frames.hasCompositionOffset)
				writer.write32(pFrame->compositionOffset());
			dataOffset += pFrame->size();
		}
	}
}



} // namespace Mona
