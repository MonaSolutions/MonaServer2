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

#include "Mona/MP4Reader.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"

using namespace std;

namespace Mona {

static const char* _MacLangs[149]{
	"eng",
	"fre",
	"deu",
	"ita",
	"dut"
	"swe",
	"spa",
	"dan",
	"por",
	"nor",
	"heb",
	"jpn",
	"ara",
	"fin",
	"grk",
	"ice",
	"mlt",
	"tur",
	"hrv",
	"chi",
	"urd",
	"hin",
	"tha",
	"kor",
	"lit",
	"pol",
	"hun",
	"lav",
	"fiu",
	"fao",
	"per",
	"rus",
	"zho",
	"vls",
	"gle",
	"sqi",
	"ron",
	"cze",
	"slk",
	"slv",
	"yid",
	"srp",
	"mac",
	"bul",
	"ukr",
	"bel",
	"uzb",
	"kaz",
	"aze",
	"aze",
	"arm",
	"geo",
	"mol",
	"mol",
	"kir",
	"tgk",
	"tuk",
	"mon",
	"mon",
	"pus",
	"kur",
	"kas",
	"snd",
	"tib",
	"nep",
	"san",
	"mar",
	"ben",
	"asm",
	"guj",
	"pan",
	"ori",
	"mal",
	"kan",
	"tam",
	"tel",
	"sin",
	"bur",
	"khm",
	"lao",
	"vie",
	"ind",
	"tgl",
	"mal",
	"mal",
	"amh",
	"tir",
	"orm",
	"orm",
	"som",
	"swa",
	"kin",
	"run",
	"nya",
	"mlg",
	"epo", // 94
	"und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und", "und",
	"wel", // 128
	"baq",
	"cat",
	"lat",
	"que",
	"grn",
	"aym",
	"crh",
	"uig",
	"dzo",
	"jav",
	"sun", // 139
	"glg",
	"afr",
	"bre",
	"iku",
	"gla",
	"glv",
	"gle",
	"ton",
	"gre", // 148
};

MP4Reader::Box& MP4Reader::Box::operator=(BinaryReader& reader) {
	if (reader.available() < 8)
		return operator=(nullptr); // rest = 0!
	_size = reader.read32();
	if (_size < 8) {
		_size = 0;
		ERROR("Bad box format without 4 char name");
		return operator=(reader); // try to continue to read
	}
	reader.read(4, _name);
	_rest = _size-8;
	return _rest ? self : operator=(reader); // if empty, continue to read!
}

struct Lost : Media::Base, virtual Object {
	Lost(UInt32 lost) : lost(lost) {}
	const UInt32 lost;
};

MP4Reader::Box& MP4Reader::Box::operator-=(UInt32 readen) {
	if (readen >= _rest)
		_rest = 0;
	else
		_rest -= readen;
	return *this;
}

void MP4Reader::Track::writeProperties(Media::Properties& properties) {
	if (!lang[0])
		return;
	struct TrackReader : DataReader {
		TrackReader(const Track& track) : _track(track), DataReader(Packet::Null(), OTHER) {}
	private:
		bool readOne(UInt8 type, DataWriter& writer) {
			writer.beginObject();
			writer.writePropertyName(*_track.pType == Media::TYPE_AUDIO ? "audioLang" : "textLang");
			writer.writeString(_track.lang, strlen(_track.lang));
			writer.endObject();
			return true;
		}
		const Track& _track;
	} reader(self);
	properties.addProperties(_track, reader);
}


UInt32 MP4Reader::parse(Packet& buffer, Media::Source& source) {
	UInt32 rest = parseData(buffer, source);
	_position += buffer.size() - rest;
	return rest;
}

UInt32 MP4Reader::parseData(const Packet& packet, Media::Source& source) {

	BinaryReader reader(packet.data(), packet.size());

	do {

		Box& box = _boxes.back();
		if (!box) {
			if (!(box = reader))
				return reader.available();
			//DEBUG(string(_boxes.size() - 1, '\t'), box.name(), " (size=", box.size(), ")");
		}

		UInt32 code = box.code();
		switch (code) {
			case FOURCC('t', 'r', 'a', 'k'): // TRACK
				_tracks.emplace_back();
			case FOURCC('t', 'r', 'a', 'f'): // TRAF
				_pTrack = NULL;
			case FOURCC('m', 'v', 'e', 'x'): // MVEX
			case FOURCC('m', 'd', 'i', 'a'): // MDIA
			case FOURCC('m', 'i', 'n', 'f'): // MINF
			case FOURCC('s', 't', 'b', 'l'): // STBL
			case FOURCC('d', 'i', 'n', 'f'): // DINF
			case FOURCC('e', 'd', 't', 's'): // EDTS
				_boxes.emplace_back();
				continue;
			case FOURCC('m', 'o', 'o', 'v'): // MOOV
				// Reset resources =>
				_times.clear(); // force to flush all Medias!
				if (!_firstMoov) {
					flushMedias(source); // clear _medias
					source.reset();
				} else
					_firstMoov = false;
				_sequence = 0;
				_datas = _audios = _videos = 0;
				_pTrack = NULL;
				_tracks.clear();
				_chunks.clear();
				_ids.clear();
				_failed = false;
				
				_boxes.emplace_back();
				continue;
			case FOURCC('m', 'o', 'o', 'f'): // MOOF
				_offset = _position + reader.position() - 8;
				_chunks.clear();
				_boxes.emplace_back();
				continue;
			case FOURCC('m', 'd', 'h', 'd'): { // MDHD
				// Media Header
				if (_tracks.empty()) {
					ERROR("Media header box not through a Track box");
					break;
				}
				if (reader.available()<22)
					return reader.available();
				BinaryReader mdhd(reader.current(), 22);
				UInt8 version = mdhd.read8();
				mdhd.next(version ? 19 : 11); // version + flags + creation time + modification time
				Track& track = _tracks.back();
				track.timeStep = mdhd.read32();
				if (track.timeStep)
					track.timeStep = 1000 / track.timeStep;
				mdhd.next(version ? 8 : 4); // duration
				// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap4/qtff4.html#//apple_ref/doc/uid/TP40000939-CH206-27005
				UInt16 lang = mdhd.read16();
				if (lang < 0x400 || lang==0x7FFF) { // if lang == 0x7FFF means "unspecified lang code" => "und"
					if (lang >= 149)
						lang = 100; // => und
					memcpy(track.lang, _MacLangs[lang], sizeof(track.lang));
				} else if (lang != 0x55C4) { // 0x55C4 = no lang information
					track.lang[0] = ((lang >> 10) & 0x1F) + 0x60;
					track.lang[1] = ((lang >> 5) & 0x1F) + 0x60;
					track.lang[2] = (lang & 0x1F) + 0x60;
					track.lang[3] = 0;
				} else
					track.lang[0] = 0;
				break;
			}
			case FOURCC('m', 'f', 'h', 'd'): { // MFHD
				// Movie fragment header
				if (reader.available()<8)
					return reader.available();
				BinaryReader mfhd(reader.current(), 8);
				mfhd.next(4); // skip version + flags
				UInt32 sequence = mfhd.read32();
				if (++_sequence == sequence)
					break;
				_sequence = sequence;
				_medias.emplace_hint(_medias.end(),
					_times.empty() ? (_medias.empty() ? 0 : _medias.rbegin()->first) : _times.begin()->first,
					pair<Track*, Media::Base*>(NULL, new Lost(range<UInt32>(_offset - _position))) // lost approximation
				);
				break;
			}
			case FOURCC('e', 'l', 'n', 'g'): { // ELNG
				// Extended language
				if (_tracks.empty()) {
					ERROR("Extended language box not through a Track box");
					break;
				}
				if (reader.available()<6)
					return reader.available();
				BinaryReader elng(reader.current(), 6);
				elng.next(4); // version + flags
				Track& track = _tracks.back();
				memcpy(track.lang, reader.current() + 4, 2); // en-US => copy just 'en'
				track.lang[2] = 0;
				break;
			}
			case FOURCC('c', 'o', '6', '4'): // CO64
				code = 0;
			case FOURCC('s', 't', 'c', 'o'): { // STCO
				// Chunk Offset
				if (_tracks.empty()) {
					ERROR("Chunk offset box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stco(reader.current(), box);
				stco.next(4); // skip version + flags
				UInt32 count = stco.read32();
				while (count-- && stco.available())
					_chunks[code ? stco.read32() : stco.read64()] = &track;
				break;
			}
			case FOURCC('s', 't', 's', 'd'): { // STSD
				// Sample description
				if (_tracks.empty()) {
					ERROR("Sample description box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stsd(reader.current(), box);
				stsd.next(4); // version + flags
				UInt32 count = stsd.read32();
				while (count-- && stsd.available()) {
					UInt32 size = stsd.read32();
					BinaryReader description(stsd.current(), stsd.available());
					if (description.shrink(stsd.next(size)) < 4)
						continue;
					const char* typeName = STR description.current();
					description.next(12); // type name (4 bytes) + reserved (6 bytes) + data reference index (2 bytes)
					if (memcmp(typeName, EXPAND("avc1")) == 0) {
						track.types.emplace_back(Media::Video::CODEC_H264);
						// see https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html#//apple_ref/doc/uid/TP40000939-CH205-74522
						description.next(70); // skip version, revision level, vendor, quality, width, height, resolution, data size, frame count, compressor name, depth and color ID
					}
					else if (memcmp(typeName, EXPAND("hev1")) == 0) {
						track.types.emplace_back(Media::Video::CODEC_HEVC);
						description.next(70); // skip version, revision level, vendor, quality, width, height, resolution, data size, frame count, compressor name, depth and color ID

					} else if (memcmp(typeName, EXPAND("mp4a")) == 0 || memcmp(typeName, EXPAND(".mp3")) == 0) {
						track.types.emplace_back(*typeName=='.' ? Media::Audio::CODEC_MP3 : Media::Audio::CODEC_AAC);
						Track::Type& type = track.types.back();
						UInt16 version = description.read16();
						if (version==2) {
							description.next(22); // skip revision level, vendor, "always" values and sizeOfStructOnly
							type.audio.rate = (UInt32)round(description.readDouble());
							type.audio.channels = description.read32();
							description.next(20);
						} else {
							description.next(6); // skip revision level and vendor
							type.audio.channels = range<UInt8>(description.read16());
							description.next(6); // skip sample size, compression id and packet size
							type.audio.rate = description.read16();
							description.next(2);
							if (version) // version = 1
								description.next(16);
						}
					} 
					else if (memcmp(typeName, EXPAND("tx3g")) == 0) { // mov_text subtitles
						track.types.emplace_back(Media::Data::TYPE_TEXT);
						description.next(description.available()); // skip all (TODO: read styles)
					} else {
						if (memcmp(typeName, EXPAND("rtp")) != 0) // RTP hint track is a doublon, useless here, so display warn just for really unsupported type!
							WARN("Unsupported ", string(typeName, 4) , " media type");
						track.types.emplace_back();
						break;
					}
					// Read extension sample description box
					while (description.available()) {
						size = description.read32();
						if (size < 5)
							continue;
						BinaryReader extension(description.current(), description.available());
						extension.shrink(description.next(size));
						if (memcmp(extension.current(), EXPAND("esds")) == 0) {
							// http://xhelmboyx.tripod.com/formats/mp4-layout.txt
							// http://hsevi.ir/RI_Standard/File/8955
							// section 7.2.6.5
							// section 7.2.6.6.1 
							// AudioSpecificConfig => https://csclub.uwaterloo.ca/~pbarfuss/ISO14496-3-2009.pdf
							extension.next(8); // skip name + version
							if (extension.read8() != 3)  // ES descriptor type = 3
								continue;
							UInt8 value = extension.read8();
							if (value & 0x80) { // 3 bytes extended descriptor
								extension.next(2);
								value = extension.read8();
							}
							extension.shrink(value);
							extension.next(2); // ES ID
							value = extension.read8();
							if (value & 0x80) // streamDependenceFlag
								extension.next(2); // dependsOn_ES_ID
							if (value & 0x40) // URL_Flag
								extension.next(extension.read8()); // skip url
							if (value & 0x20) // OCRstreamFlag
								extension.next(2); // OCR_ES_Id
							if (extension.read8() != 4)  // Audio descriptor type = 4
								continue;
							value = extension.read8();
							if (value & 0x80) { // 3 bytes extended descriptor
								extension.next(2);
								value = extension.read8();
							}
							extension.shrink(value);
							Track::Type& type = track.types.back();
							UInt8 codec = extension.read8();
							UInt8 config[2];
							switch (codec) {
								case 64: // AAC
									break;
								case 102: // MPEG-4 ADTS main
									type.config = Packet(MPEG4::WriteAudioConfig(1, type.audio.rate, type.audio.channels, config), 2);
									break;
								case 103: // MPEG-4 ADTS Low Complexity
									type.config = Packet(MPEG4::WriteAudioConfig(2, type.audio.rate, type.audio.channels, config), 2);
									break;
								case 104: // MPEG-4 ADTS Scalable Sampling Rate
									type.config = Packet(MPEG4::WriteAudioConfig(3, type.audio.rate, type.audio.channels, config), 2);
									break;
								case 105: // MPEG-2 ADTS
									type.audio.codec = Media::Audio::CODEC_MP3;
									break;
								default:
									if (type == Media::TYPE_AUDIO)
										WARN("Audio track with unsupported ", codec, " codec")
									else
										WARN("Video track with unsupported ", codec, " codec")
									type = nullptr;
									break;
							}
							if(!type)
								break;
							extension.next(12); // skip decoder config descriptor (buffer size + max bitrate + average bitrate)
							if (extension.read8() != 5)  // Audio specific config = 5
								continue;
							value = extension.read8();
							if (value & 0x80) { // 3 bytes extended descriptor
								extension.next(2);
								value = extension.read8();
							}
							extension.shrink(value);
							type.config = Packet(extension.current(), extension.available());
							// Fix rate and channels with configs packet (more precise!)
							MPEG4::ReadAudioConfig(type.config.data(), type.config.size(), type.audio.rate, type.audio.channels);
						} else if (memcmp(extension.current(), EXPAND("avcC")) == 0) {
							// http://hsevi.ir/RI_Standard/File/8978
							// section 5.2.4.1.1
							extension.next(4);
							shared<Buffer> pBuffer(SET);
							AVC::ReadVideoConfig(extension.current(), extension.available(), *pBuffer);
							track.types.back().config.set(pBuffer);
						}
						else if (memcmp(extension.current(), EXPAND("hvcC")) == 0) {
							// https://stackoverflow.com/questions/32697608/where-can-i-find-hevc-h-265-specs
							extension.next(4);
							shared<Buffer> pBuffer(SET);						
							HEVC::ReadVideoConfig(extension.current(), extension.available(), *pBuffer);
							track.types.back().config.set(pBuffer);
						}

					}
				}
				break;
			}
			case FOURCC('s', 't', 's', 'c'): { // STSC
				// Sample to Chunks
				if (_tracks.empty()) {
					ERROR("Sample to chunks box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stsc(reader.current(), box);
				stsc.next(4); // version + flags
				UInt32 count = stsc.read32();
				while (count-- && stsc.available()>=8) { // 8 => required at less "first chunk" and "sample count" field
					UInt64& value = track.changes[stsc.read32()];
					value = stsc.read32();
					value = value<<32 | max(stsc.read32(), 1u);
				}
				break;
			}
			case FOURCC('s', 't', 's', 'z'): { // STSZ
				// Sample size
				if (_tracks.empty()) {
					ERROR("Sample size box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stsz(reader.current(), box);
				stsz.next(4); // version + flags
				if ((track.size = track.defaultSize = stsz.read32()))
					break;
				UInt32 count = stsz.read32();
				while (count-- && stsz.available() >= 4)
					track.sizes.emplace_back(stsz.read32());
				break;
			}
			case FOURCC('e', 'l', 's', 't'): { // ELST
				// Edit list box
				if (_tracks.empty()) {
					ERROR("Edit list box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader elst(reader.current(), box);
				UInt8 version = elst.read8();
				elst.next(3); // flags
				UInt32 count = elst.read32();
				
				track.durations.time = track.time = 0;

				while (count--) {
					UInt64 duration;
					if (version) {
						duration = elst.read64();
						if ((elst.read64() & 0x80000000)==0) // if postive value
							break;
					} else {
						duration = elst.read32();
						if ((elst.read32() & 0x80000000)==0)  // if postive value
							break;
					}
					// add silence on beginning!
					track.durations.time += duration;
					track.time += duration;
					elst.next(4); // media_rate_integer + media_rate_fraction
				}
				break;
			}
			case FOURCC('s', 't', 't', 's'): { // STTS
				// Time to sample
				if (_tracks.empty()) {
					ERROR("Time to sample box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stts(reader.current(), box);
				stts.next(4); // version + flags
				UInt32 count = stts.read32();
				UInt32 time = UInt32(round(track.durations.time));
				auto itTime = _times.lower_bound(time);
				while (count-- && stts.available() >= 8) {
					track.durations.emplace_back(stts.read32());
					Repeat& repeat = track.durations.back();
					repeat.value = stts.read32();
					for (UInt32 i = 0; i < repeat.count; ++i) {
						++(itTime = _times.emplace_hint(itTime, time, 0))->second;
						time = UInt32(round(track.durations.time += repeat.value * track.timeStep));
					}
				}
				break;
			}
			case FOURCC('c', 't', 't', 's'): { // CTTS
				// Composition offset
				if (_tracks.empty()) {
					ERROR("Composition offset box not through a Track box");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader ctts(reader.current(), box);
				ctts.next(4); // version + flags
				UInt32 count = ctts.read32();
				while (count-- && ctts.available() >= 8) {
					track.compositionOffsets.emplace_back(ctts.read32());
					track.compositionOffsets.back().value = ctts.read32(); // offset
				}
				break;
			}
			case FOURCC('t', 'k', 'h', 'd'): { // TKHD
				// Track Header
				if (_tracks.empty()) {
					ERROR("Track header box not through a Track box");
					break;
				}
				if (reader.available()<16)
					return reader.available();
				BinaryReader tkhd(reader.current(), 16);
				tkhd.next(tkhd.read8() ? 19 : 11); // version + flags + creation time + modification time
				const auto& it = _ids.emplace(tkhd.read32(), &_tracks.back());
				if (!it.second)
					ERROR("Bad track header id, identification ", it.first->first, " already used");
				break;
			}
			case FOURCC('t', 'r', 'e', 'x'): { // TREX
				// Track extends box
				if (reader.available()< box)
					return reader.available();
				BinaryReader trex(reader.current(), box);
				trex.next(4); // version + flags
				UInt32 id = trex.read32();
				const auto& it = _ids.find(id);
				if (it == _ids.end()) {
					ERROR("Impossible to find track with ", id, " as identification");
					break;
				}
				Track& track(*it->second);
				track.typeIndex = trex.read32();
				track.defaultDuration = trex.read32();
				track.defaultSize = trex.read32();
				break;
			}
			case FOURCC('t', 'f', 'h', 'd'): { // TFHD
				// Track fragment Header
				if (reader.available()< box)
					return reader.available();
				BinaryReader tfhd(reader.current(), box);
				tfhd.next(); // version
				UInt32 flags = tfhd.read24(); // flags
				UInt32 id = tfhd.read32();
				const auto& it = _ids.find(id);
				if (it == _ids.end()) {
					ERROR("Impossible to find track with ", id, " as identification");
					_pTrack = NULL;
					break;
				}
				_pTrack = it->second;
				_pTrack->chunk = 0;
				_pTrack->sample = 0;
				_pTrack->samples = 0;
				_pTrack->changes.clear();
				_pTrack->size = 0;
				_pTrack->sizes.clear();
				_pTrack->compositionOffsets.clear();
				_pTrack->durations.clear();

				if (flags & 1)
					_offset = tfhd.read64();
				_fragment.typeIndex = (flags & 2) ? tfhd.read32() : _pTrack->typeIndex;
				_fragment.defaultDuration = (flags & 8) ? tfhd.read32() : _pTrack->defaultDuration;
				_fragment.defaultSize = (flags & 0x10) ? tfhd.read32() : _pTrack->defaultSize;
				break;
			}
			case FOURCC('t', 'r', 'u', 'n'): { // TRUN
				// Track fragment run box
				if (!_pTrack) {
					ERROR("Track fragment run box without valid track fragment box before");
					break;
				}
				if (reader.available()<box)
					return reader.available();
				BinaryReader trun(reader.current(), box);
				trun.next(); // version
				UInt32 flags = trun.read24(); // flags
				UInt32 count = trun.read32();
				if (!count) {
					WARN("Track fragment run box describes 0 samples");
					break; // nothing to do!
				}

				UInt64& change = _pTrack->changes[0];
				change = count;
				change = (change << 32) | _fragment.typeIndex;
				_pTrack->size = (flags & 0x200) ? 0 : _fragment.defaultSize;

				UInt32 value = flags & 1 ? trun.read32() : 0; // To fix a bug with ffmpeg and omit_tfhd_offset flags (flags stays set but value is 0!)
				if(value)
					_chunks[_offset + value] = _pTrack;
				else if(!_chunks.empty())
					_chunks[_chunks.rbegin()->first+1] = _pTrack;
				else
					_chunks[_offset] = _pTrack;
				if (flags & 4)
					trun.next(4); // first_sammple_flags

				if (!(flags & 0x100)) {
					if (!_fragment.defaultDuration) {
						ERROR("No duration information in track fragment box");
						break;
					}
					_pTrack->durations.emplace_back(count);
					Repeat& repeat = _pTrack->durations.back();
					repeat.value = _fragment.defaultDuration;
					UInt32 time = UInt32(round(_pTrack->durations.time));
					auto itTime = _times.lower_bound(time);
					for (UInt32 i = 0; i < repeat.count; ++i) {
						++(itTime = _times.emplace_hint(itTime, time, 0))->second;
						time = UInt32(round(_pTrack->durations.time += repeat.value * _pTrack->timeStep));
					}
				}

				UInt32 time = UInt32(round(_pTrack->durations.time));
				auto itTime = _times.lower_bound(time);
				while (count-- && trun.available()) {
					if (flags & 0x100) {
						value = trun.read32();
						if (_pTrack->durations.empty() || _pTrack->durations.back().value != value) {
							_pTrack->durations.emplace_back();
							_pTrack->durations.back().value = value;
						} else
							++_pTrack->durations.back().count;

						++(itTime = _times.emplace_hint(itTime, time, 0))->second;
						time = UInt32(round(_pTrack->durations.time += value * _pTrack->timeStep));
					}
					if (flags & 0x200)
						_pTrack->sizes.emplace_back(trun.read32());
					if (flags & 0x400)
						trun.next(4); // sample_flags
					if (flags & 0x800) {
						value = trun.read32();
						if (_pTrack->compositionOffsets.empty() || _pTrack->compositionOffsets.back().value != value) {
							_pTrack->compositionOffsets.emplace_back();
							_pTrack->compositionOffsets.back().value = value;
						} else
							++_pTrack->compositionOffsets.back().count;
					}
				}

				break;
			}
			case FOURCC('m', 'd', 'a', 't'): { // MDAT
				// DATA
				while(!_failed && reader.available()) {
					if (_tracks.empty()) {
						ERROR("No tracks information before mdat (No support of mdat box before moov box)");
						_failed = true;
						break;
					}
					if (_chunks.empty())
						break;

					// consume
					UInt32 value = reader.position() + _position;				
					if (value < _chunks.begin()->first)
						box -= reader.next(UInt32(_chunks.begin()->first - value));

					Track& track = *_chunks.begin()->second;
					if (!track.timeStep) {
						ERROR("Data box without mdhd box with valid timeScale field before");
						_failed = true;
						break;
					}
					if (track.durations.empty()) {
						ERROR("Data box without valid Time to sample box before");
						_failed = true;
						break;
					}
					if (track.changes.empty()) {
						ERROR("Data box without valid Sample to chunk box before");
						_failed = true;
						break;
					}
					if (track.types.empty()) {
						ERROR("Data box without valid Sample description box before");
						_failed = true;
						break;
					}
					auto it = track.changes.begin();
					if (track.chunk < it->first)
						track.chunk = it->first;
					UInt32 samples = it->second >> 32;
					while (track.sample<samples) {
						
						// determine size
						value = track.sample + track.samples;
						UInt32 size = value < track.sizes.size() ? track.sizes[value] : track.size;
						if (reader.available() < size) {
							flushMedias(source); // flush when all read buffer is readen
							return reader.available();
						}

						// determine type
						value = it->second & 0xFFFFFFFF;
						if (!value || value > track.types.size()) {
							if(value)
								WARN("Bad type index indication");
							value = track.pType ? 0 : 1;
						}
						
						Track::Type& type = value ? track.types[value - 1] : *track.pType;
						UInt32 time = UInt32(round(track.time));
						UInt32 mediaSizes = _medias.size();
						switch (type) {
							case Media::TYPE_AUDIO:
								type.audio.time = time;
								if (&type != track.pType) {
									// has changed!
									track.pType = &type;				
									if (_audios < 0xFF) {
										track = ++_audios;
										if(type.config) {
											type.audio.isConfig = true;
											_medias.emplace(time, pair<Track*, Media::Base*>(&track, new Media::Audio(type.audio, type.config, track)));
											++_times[time]; // to match with times synchro
											type.audio.isConfig = false;
										}
									} else {
										WARN("Audio track ", _audios, " ignored because Mona doesn't accept more than 255 tracks");
										track = 0;
									}
								}
						  //	DEBUG("Audio ", type.audio.time);
								if (track && size) // if size == 0 => silence!
									_medias.emplace(time, pair<Track*, Media::Base*>(&track, new Media::Audio(type.audio, Packet(packet, reader.current(), size), track)));
								break;
							case Media::TYPE_VIDEO: {
								type.video.time = time;
								if (&type != track.pType) {
									// has changed!
									track.pType = &type;
									if (_videos < 0xFF) {
										track = ++_videos;
										if(type.config) {
											type.video.frame = Media::Video::FRAME_CONFIG;
											_medias.emplace(time, pair<Track*, Media::Base*>(&track, new Media::Video(type.video, type.config, track)));
											++_times[time]; // to match with times synchro
										}
									} else {
										WARN("Video track ", _videos, " ignored because Mona doesn't accept more than 255 tracks");
										track = 0;
									}
								}
								if (!track || !size) // no size => silence!
									break; // ignored!

								// determine compositionOffset
								if(!track.compositionOffsets.empty()) {
									Repeat& repeat = track.compositionOffsets.front();
									type.video.compositionOffset = range<UInt16>(round(repeat.value*track.timeStep));
									if (!--repeat.count)
										track.compositionOffsets.pop_front();
								} else
									type.video.compositionOffset = 0;

								//	DEBUG("Video ", time);
								
								// Get the correct video.frame type!
								// + support SPS and PPS inner samples (required by specification)
								if (type.video.codec == Media::Video::CODEC_H264)
									frameToMedias<AVC>(track, time, Packet(packet, reader.current(), size));
								else
									frameToMedias<HEVC>(track, time, Packet(packet, reader.current(), size));
								break;
							}
							case Media::TYPE_DATA: {
								if (&type != track.pType) {
									// has changed!
									track.pType = &type;
									if (_datas < 0xFF)
										track = ++_datas;
									else {
										WARN("Data track ", _datas, " ignored because Mona doesn't accept more than 255 tracks");
										track = 0;
									}
								}
								if (!track || !size) // no size => silence!
									break; // ignored!

								BinaryReader mov_text(reader.current(), size);
								UInt16 length = mov_text.read16();
								if (length && length <= mov_text.available()) // TODO: Use a reference to "packet" like in Media::Audio above
									_medias.emplace(time, pair<Track*, Media::Base*>(&track, new Media::Data(type.data, Packet(mov_text.current(), length), 1)));
								// TODO: Extract "styl" and others boxes types (hlit, hclr, twrp)
							}
							default:; // ignored!
						}

						// Add a media to match times reference if no media added!
						if (mediaSizes == _medias.size())
							_medias.emplace(time, pair<Track*, Media::Base*>(NULL, NULL));

						// determine next time
						Repeat& repeat = track.durations.front();
						track.time += repeat.value*track.timeStep;
						if (track.durations.size() > 1 && !--repeat.count) // repeat the last duration if few duration entries are missing
							track.durations.pop_front();
						
						// consume!
						++track.sample;
						box -= reader.next(size);
					}

					_chunks.erase(_chunks.begin());
					if (++it != track.changes.end() && ++track.chunk >= it->first)
						track.changes.erase(track.changes.begin());
					track.samples += samples;
					track.sample = 0;
				}
				flushMedias(source); // flush when all read buffer is readen
				break;
			}
			default: // unknown or ignored
				if(box == box.contentSize())
					TRACE("Undefined box ", box.name(), " (size=", box.size(), ")");
		}

		if ((box -= reader.next(box))) // consume
			continue;
		// pop last box
		UInt32 size;
		do {
			size = _boxes.back().size();
			_boxes.pop_back();
		} while (!_boxes.empty() && !(_boxes.back() -= size)); // remove parent box if empty one time box children removed!

		_boxes.emplace_back(); // always at less one box!
		
	} while (reader.available());

	return 0;
}


void MP4Reader::flushMedias(Media::Source& source) {
	Media::Properties properties;
	auto it = _medias.begin();
	for (; it != _medias.end(); ++it) {
		if (!_times.empty()) {
			if (it->first > _times.begin()->first)
				break;
			if(it->first== _times.begin()->first && !--_times.begin()->second)
				_times.erase(_times.begin());
		}
		if (!it->second.second)
			continue;
		Media::Base& media = *it->second.second;
		if (media.type) {
			it->second.first->writeProperties(properties);
			source.writeMedia(media);
		} else
			source.reportLost(media.type, ((Lost&)media).lost);
	}
	_medias.erase(_medias.begin(), it);
	if (properties.count())
		source.addProperties(properties);
}

void MP4Reader::onFlush(Packet& buffer, Media::Source& source) {
	// release resources
	_times.clear(); // to force media flush (and clear _medias)
	flushMedias(source);
	_boxes.resize(1);
	_boxes.back() = nullptr;
	_chunks.clear();
	_tracks.clear();
	_ids.clear();
	
	_offset = _position = 0;
	_firstMoov = true;

	MediaReader::onFlush(buffer, source);
}

template <class VideoType>
void MP4Reader::frameToMedias(Track& track, UInt32 time, const Packet& packet) {
	BinaryReader frames(packet.data(), packet.size());
	Media::Video::Tag& tag = track.pType->video;
	tag.frame = Media::Video::FRAME_UNSPECIFIED;
	const UInt8* frame = NULL;
	UInt32 frameSize;
	Media::Video* pLastVideo = NULL;
	while (frames.available()>4) {

		UInt8 frameType = VideoType::NalType(frames.current()[4]);
		if (frame) {
			if (tag.frame == Media::Video::FRAME_CONFIG) {
				// the previous is a CONFIG frame
				UInt8 prevType = VideoType::NalType(frame[4]);
				if (frameType != prevType) {
					if (VideoType::Frames[frameType] == Media::Video::FRAME_CONFIG) {
						// complete config packet!
						frameSize += frames.next(frames.read32()) + 4;
						if (pLastVideo)
							++_times[time]; // to match with times synchro
						_medias.emplace(time, pair<Track*, Media::Base*>(&track, pLastVideo = new Media::Video(tag, track.pType->config = Packet(frame, frameSize), track)));
						frame = NULL;
						continue;
					} // else new frame is not a config part
					if (prevType == VideoType::NAL_SPS) {
						if (pLastVideo)
							++_times[time]; // to match with times synchro
						_medias.emplace(time, pair<Track*, Media::Base*>(&track, pLastVideo = new Media::Video(tag, track.pType->config = Packet(frame, frameSize), track)));
					} // else ignore 8 alone packet
				} // else erase duplicate config type
				frame = NULL;
			}
			else if (VideoType::Frames[frameType] == Media::Video::FRAME_CONFIG) {
				// flush what before config packet
				if (pLastVideo)
					++_times[time]; // to match with times synchro
				_medias.emplace(time, pair<Track*, Media::Base*>(&track, pLastVideo = new Media::Video(tag, Packet(packet, frame, frameSize), track)));
				frame = NULL;
			}
		}
		tag.frame = VideoType::UpdateFrame(frameType, frame ? tag.frame : Media::Video::FRAME_UNSPECIFIED);

		if (!frame) {
			frame = frames.current();
			frameSize = 0;
		}
		frameSize += frames.next(frames.read32()) + 4;
	}

	if (!frame)
		return;
	if (pLastVideo)
		++_times[time]; // to match with times synchro
	if (tag.frame == Media::Video::FRAME_CONFIG)
		_medias.emplace(time, pair<Track*, Media::Base*>(&track, new Media::Video(tag, track.pType->config.set(frame, frameSize), track)));
	else
		_medias.emplace(time, pair<Track*, Media::Base*>(&track, new Media::Video(tag, Packet(packet, frame, frameSize), track)));
}

} // namespace Mona
