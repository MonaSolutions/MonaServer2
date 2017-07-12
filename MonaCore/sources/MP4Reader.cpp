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
#include "Mona/MPEG4.h"

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
	"xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx", "xxx",
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
	_type = UNDEFINED;
	if (reader.available() < 8)
		return *this;
	_size = reader.read32();
	if (_size < 8) {
		ERROR("Bad box format without 4 bytes name");
		return operator=(reader);
	}
	_rest = _size-8;
	if (memcmp(reader.current(), EXPAND("moof")) == 0) {
		_type = MOOF;
	} else if (memcmp(reader.current(), EXPAND("traf")) == 0) {
		_type = TRAF;
	} else if (memcmp(reader.current(), EXPAND("tfhd")) == 0) {
		_type = TFHD;
	} else if (memcmp(reader.current(), EXPAND("trun")) == 0) {
		_type = TRUN;
	} else if (memcmp(reader.current(), EXPAND("moov")) == 0) {
		_type = MOOV;
	} else if (memcmp(reader.current(), EXPAND("mvex")) == 0) {
		_type = MVEX;
	} else if (memcmp(reader.current(), EXPAND("mdia")) == 0) {
		_type = MDIA;
	} else if (memcmp(reader.current(), EXPAND("minf")) == 0) {
		_type = MINF;
	} else if (memcmp(reader.current(), EXPAND("stbl")) == 0) {
		_type = STBL;
	} else if (memcmp(reader.current(), EXPAND("trak")) == 0) {
		_type = TRAK;
	} else if (memcmp(reader.current(), EXPAND("trex")) == 0) {
		_type = TREX;
	} else if (memcmp(reader.current(), EXPAND("stsc")) == 0) {
		_type = STSC;
	} else if (memcmp(reader.current(), EXPAND("mdhd")) == 0) {
		_type = MDHD;
	} else if (memcmp(reader.current(), EXPAND("tkhd")) == 0) {
		_type = TKHD;
	} else if (memcmp(reader.current(), EXPAND("elng")) == 0) {
		_type = ELNG;
	} else if (memcmp(reader.current(), EXPAND("stsd")) == 0) {
		_type = STSD;
	} else if (memcmp(reader.current(), EXPAND("dinf")) == 0) {
		_type = DINF;
	} else if (memcmp(reader.current(), EXPAND("stco")) == 0) {
		_type = STCO;
	} else if (memcmp(reader.current(), EXPAND("co64")) == 0) {
		_type = CO64;
	} else if (memcmp(reader.current(), EXPAND("stsz")) == 0) {
		_type = STSZ;
	} else if (memcmp(reader.current(), EXPAND("stts")) == 0) {
		_type = STTS;
	} else if (memcmp(reader.current(), EXPAND("ctts")) == 0) {
		_type = CTTS;
	} else if (memcmp(reader.current(), EXPAND("mdat")) == 0) {
		_type = MDAT;
	}// else
		//DEBUG("Undefined box ", string(STR reader.current(), 4), " (size=", _size, ")");
	reader.next(4);
	return _rest ? *this : operator=(reader); // if empty, continue to read!
}

MP4Reader::Box& MP4Reader::Box::operator-=(BinaryReader& reader) {
	if (reader.available() < _rest) {
		_rest -= reader.available();
		reader.next(reader.available());
	} else {
		reader.next(_rest);
		_rest = 0;
	}
	return *this;
}

MP4Reader::Box& MP4Reader::Box::operator-=(UInt32 readen) {
	if (readen >= _rest)
		_rest = 0;
	else
		_rest -= readen;
	return *this;
}

UInt32 MP4Reader::parse(const Packet& packet, Media::Source& source) {
	UInt32 rest = parseData(packet, source);
	_position += packet.size() - rest;
	return rest;
}

UInt32 MP4Reader::parseData(const Packet& packet, Media::Source& source) {

	BinaryReader reader(packet.data(), packet.size());

	do {

		if (!_boxes.back()) {
			if (!(_boxes.back() = reader))
				return reader.available();
			//DEBUG(string(_boxes.size() - 1, '\t'), _boxes.back().name());
		}

		Box::Type type = _boxes.back().type();
		switch (type) {
			case Box::TRAK:
				_tracks.emplace_back();
			case Box::TRAF:
				_pTrack = NULL;
			case Box::MOOV:
			case Box::MVEX:
			case Box::MDIA:
			case Box::MINF:
			case Box::STBL:
			case Box::DINF:
				_boxes.emplace_back();
				continue;
			case Box::MOOF:
				_offset = _position + reader.position() - 8;
				_chunks.clear();
				_boxes.emplace_back();
				continue;
			case Box::MDHD: {
				// Media Header
				if (_tracks.empty()) {
					ERROR("Media header box not through a Track box");
					break;
				}
				if (reader.available()<22)
					return reader.available();
				BinaryReader mdhd(reader.current(), 22);
				mdhd.next(12); // version + flags + creation time + modification time
				Track& track = _tracks.back();
				track.timeStep = mdhd.read32();
				if (track.timeStep)
					track.timeStep = 1000 / track.timeStep;
				mdhd.next(4); // duration
				// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap4/qtff4.html#//apple_ref/doc/uid/TP40000939-CH206-27005
				UInt16 lang = mdhd.read16();
				if (lang < 0x400) {
					if (lang >= 149)
						lang = 100; // => xxx
					memcpy(track.lang, _MacLangs[lang], 3);
				} else if (lang != 0x7FFF && lang != 0x55C4) { // 0x55C4 = "und"!
					track.lang[0] = ((lang >> 10) & 0x1F) + 0x60;
					track.lang[1] = ((lang >> 5) & 0x1F) + 0x60;
					track.lang[2] = (lang & 0x1F) + 0x60;
				} else
					track.lang[0] = 0;
				break;
			}
			case Box::ELNG: {
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
				memcpy(track.lang, reader.current() + 4, 2);
				track.lang[2] = 0;
				break;
			}
			case Box::CO64:
			case Box::STCO: {
				// Chunk Offset
				if (_tracks.empty()) {
					ERROR("Chunk offset box not through a Track box");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stco(reader.current(), _boxes.back());
				stco.next(4); // skip version + flags
				UInt32 count = stco.read32();
				while (count-- && stco.available())
					_chunks[type==Box::STCO ? stco.read32() : stco.read64()] = &track;
				break;
			}
			case Box::STSD: {
				// Sample description
				if (_tracks.empty()) {
					ERROR("Sample description box not through a Track box");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stsd(reader.current(), _boxes.back());
				stsd.next(4); // version + flags
				UInt32 count = stsd.read32();
				while (count-- && stsd.available()) {
					UInt32 size = stsd.read32();
					BinaryReader description(stsd.current(), stsd.available());
					if (description.shrink(stsd.next(size)) < 4)
						continue;
					const char* typeName = STR description.current();
					description.next(12);
					if (memcmp(typeName, EXPAND("avc1")) == 0) {
						track.types.emplace_back(Media::Video::CODEC_H264);
						// see https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html#//apple_ref/doc/uid/TP40000939-CH205-74522
						description.next(44);
						if (!description.read16()) {
							// Color table
							description.next(14); //header
							description.next((description.read16()+1)*8);
						}
					} else if (memcmp(typeName, EXPAND("mp4a")) == 0) {
						track.types.emplace_back(Media::Audio::CODEC_AAC);
						Track::Type& type = track.types.back();
						UInt16 version = description.read16();
						if (version==2) {
							description.next(22); // skip revision level, vendor, "always" values and sizeOfStructOnly
							type.audio.rate = (UInt32)round(description.readDouble());
							type.audio.channels = description.read32();
							description.next(20);
						} else {
							description.next(6); // skip revision level and vendor
							type.audio.channels = limit8(description.read16());
							description.next(6); // skip sample size, compression id and packet size
							type.audio.rate = description.read16();
							description.next(2);
							if (version) // version = 1
								description.next(16);
						}
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
							extension.next(8); // skip type + version
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
							extension.next(13); // skip decoder config descriptor
							if (extension.read8() != 5)  // Audio specific config = 5
								continue;
							value = extension.read8();
							if (value & 0x80) { // 3 bytes extended descriptor
								extension.next(2);
								value = extension.read8();
							}
							extension.shrink(value);
							Track::Type& type = track.types.back();
							type.config = Packet(extension.current(), extension.available());
							// Fix rate and channels with configs packet (more precise!)
							MPEG4::ReadAudioConfig(type.config.data(), type.config.size(), type.audio.rate, type.audio.channels);
						} else if (memcmp(extension.current(), EXPAND("avcC")) == 0) {
							// http://hsevi.ir/RI_Standard/File/8978
							// section 5.2.4.1.1
							extension.next(4);
							shared<Buffer> pBuffer(new Buffer());
							MPEG4::ReadVideoConfig(extension.current(), extension.size(), *pBuffer);
							track.types.back().config.set(pBuffer);
						}

					}
				}
				break;
			}
			case Box::STSC: {
				// Sample to Chunks
				if (_tracks.empty()) {
					ERROR("Sample to chunks box not through a Track box");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stsc(reader.current(), _boxes.back());
				stsc.next(4); // version + flags
				UInt32 count = stsc.read32();
				while (count-- && stsc.available()>=8) { // 8 => required at less "first chunk" and "sample count" field
					UInt64& value = track.changes[stsc.read32()];
					value = stsc.read32();
					value = value<<32 | max<UInt32>(stsc.read32(), 1);
				}
				break;
			}
			case Box::STSZ: {
				// Sample size
				if (_tracks.empty()) {
					ERROR("Sample size box not through a Track box");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stsz(reader.current(), _boxes.back());
				stsz.next(4); // version + flags
				if ((track.size = track.defaultSize = stsz.read32()))
					break;
				UInt32 count = stsz.read32();
				while (count-- && stsz.available() >= 4)
					track.sizes.emplace_back(stsz.read32());
				break;
			}
			case Box::STTS: {
				// Time to sample
				if (_tracks.empty()) {
					ERROR("Time to sample box not through a Track box");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader stts(reader.current(), _boxes.back());
				stts.next(4); // version + flags
				UInt32 count = stts.read32();
				while (count-- && stts.available() >= 8) {
					track.durations.emplace_back(stts.read32());
					track.durations.back().value = stts.read32();
				}
				break;
			}
			case Box::CTTS: {
				// Composition offset
				if (_tracks.empty()) {
					ERROR("Composition offset box not through a Track box");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				Track& track = _tracks.back();
				BinaryReader ctts(reader.current(), _boxes.back());
				ctts.next(4); // version + flags
				UInt32 count = ctts.read32();
				while (count-- && ctts.available() >= 8) {
					track.compositionOffsets.emplace_back(ctts.read32());
					track.compositionOffsets.back().value = ctts.read32(); // offset
				}
				break;
			}
			case Box::TKHD: {
				// Track Header
				if (_tracks.empty()) {
					ERROR("Track header box not through a Track box");
					break;
				}
				if (reader.available()<16)
					return reader.available();
				BinaryReader tkhd(reader.current(), 16);
				tkhd.next(12); // version + flags + creation time + modification time
				const auto& it = _ids.emplace(tkhd.read32(), &_tracks.back());
				if (!it.second)
					ERROR("Bad track header id, identification ", it.first->first, " already used");
				break;
			}
			case Box::TREX: {
				// Track extends box
				if (reader.available()< _boxes.back())
					return reader.available();
				BinaryReader trex(reader.current(), _boxes.back());
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
			case Box::TFHD: {
				// Track fragment Header
				if (reader.available()< _boxes.back())
					return reader.available();
				BinaryReader tfhd(reader.current(), _boxes.back());
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
			case Box::TRUN: {
				// Track fragment run box
				if (!_pTrack) {
					ERROR("Track fragment run box without valid track fragment box before");
					break;
				}
				if (reader.available()<_boxes.back())
					return reader.available();
				BinaryReader trun(reader.current(), _boxes.back());
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

				if (flags & 1)
					_chunks[_offset + trun.read32()] = _pTrack;
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
					_pTrack->durations.back().value = _fragment.defaultDuration;
				}

				UInt32 value;
				while (count-- && trun.available()) {
					if (flags & 0x100) {
						value = trun.read32();
						if (_pTrack->durations.empty() || _pTrack->durations.back().value != value) {
							_pTrack->durations.emplace_back();
							_pTrack->durations.back().value = value;
						} else
							++_pTrack->durations.back().count;
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
			case Box::MDAT: {
				// DATA
				while(!_failed && !_chunks.empty() && reader.available()) {
					if(_chunks.begin()->second->buffered)
						flushMedias(source);

					UInt32 value = reader.position() + _position;				
					if (value < _chunks.begin()->first)
						_boxes.back() -= reader.next(UInt32(_chunks.begin()->first - value));
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
						if (!size) {
							ERROR("No sample size matched");
							_failed = true;
							break;
						}
						if(reader.available()<size)
							return reader.available();

						// determine type
						value = it->second & 0xFFFFFFFF;
						if (!value || value > track.types.size()) {
							if(value)
								WARN("Bad type index indication");
							value = track.pType ? 0 : 1;
						}
						
						Track::Type& type = value ? track.types[value - 1] : *track.pType;
						type.setTime(track.time);

						switch (type) {
							case Media::TYPE_AUDIO:
								if (&type != track.pType) {
									// has changed!
									track.pType = &type;				
									if (_audios.size() < 0xFF) {
										_audios.emplace_back(&track);
										track = UInt8(_audios.size());
										if(type.config) {
											type.audio.isConfig = true;
											_medias.emplace(type.audio.time, new Media::Audio(type.audio, type.config, track));
											type.audio.isConfig = false;
										}
									} else {
										WARN("Audio track ", _audios.size(), " ignored because Mona doesn't accept more than 255 tracks");
										track = 0;
									}
								}
								if(track)
									_medias.emplace(type.audio.time, new Media::Audio(type.audio, Packet(packet, reader.current(), size), track));
								break;
							case Media::TYPE_VIDEO: {
								if (&type != track.pType) {
									// has changed!
									track.pType = &type;
									if (_videos.size() < 0xFF) {
										_videos.emplace_back(&track);
										track = UInt8(_videos.size());
										if(type.config) {
											type.video.frame = Media::Video::FRAME_CONFIG;
											_medias.emplace(type.video.time,new Media::Video(type.video, type.config, track));
										}
									} else {
										WARN("Audio track ", _audios.size(), " ignored because Mona doesn't accept more than 255 tracks");
										track = 0;
									}
								}
								if (!track)
									break; // ignored!

								// determine compositionOffset
								if(!track.compositionOffsets.empty()) {
									Repeat& repeat = track.compositionOffsets.front();
									type.video.compositionOffset = limit16(round(repeat.value*track.timeStep));
									if (!--repeat.count)
										track.compositionOffsets.pop_front();
								} else
									type.video.compositionOffset = 0;
								
								// Get the correct video.frame type!
								// + support SPS and PPS inner samples (required by specification)
								BinaryReader frames(reader.current(), size);
								type.video.frame = Media::Video::FRAME_UNSPECIFIED;
								const UInt8* frame=NULL;
								UInt32 frameSize;
								while (frames.available()>4) {
									UInt8 frameType = frames.current()[4] & 0x1F;
									if (type.video.frame == Media::Video::FRAME_CONFIG) {
										// the previous is a CONFIG frame
										UInt8 prevType = (frame[4] & 0x1F);
										if (frameType != prevType) {
											if (frameType == 7 || frameType == 8) {
												// complete config packet!
												frameSize += frames.next(frames.read32()) + 4;
												_medias.emplace(type.video.time, new Media::Video(type.video, type.config = Packet(frame, frameSize), track));
												frame = NULL;
												continue;
											} // else new frame is not a config part
											if (prevType == 7) // else ignore 8 alone packet
												_medias.emplace(type.video.time, new Media::Video(type.video, type.config = Packet(frame, frameSize), track));
										} // else erase duplicate config type
										frame = NULL;
									} else if (frameType == 7 || frameType == 8) {
										// flush what before config packet
										_medias.emplace(type.video.time, new Media::Video(type.video, Packet(packet, frame, frameSize), track));
										frame = NULL;
									}
									type.video.frame = MPEG4::UpdateFrame(frameType, type.video.frame);
									if (!frame) {
										frame = frames.current();
										frameSize = 0;
									}
									frameSize += frames.next(frames.read32()) + 4;
								}
								if (frame)
									_medias.emplace(type.video.time, new Media::Video(type.video, type.video.frame == Media::Video::FRAME_CONFIG ? (type.config = Packet(frame, frameSize)) : Packet(packet, frame, frameSize), track));
								break;
							}
							default:; // ignored!
						}
			
						// determine next time
						Repeat& repeat = track.durations.front();
						track.time += repeat.value*track.timeStep;
						if (track.durations.size() > 1 && !--repeat.count) // repeat the last duration if few duration entries are missing
							track.durations.pop_front();
			
						// consume!
						++track.sample;
						_boxes.back() -= reader.next(size);
					}

					_chunks.erase(_chunks.begin());
					if (++it != track.changes.end() && ++track.chunk >= it->first)
						track.changes.erase(track.changes.begin());
					track.samples += samples;
					track.sample = 0;
					track.buffered = true;
				}
				flushMedias(source);
				break;
			}
			default:; // unknown or ignored	
		}

		if ((_boxes.back() -= reader)) // consume
			continue;
		// pop last box
		UInt32 size;
		do {
			size = _boxes.back().size();
			_boxes.pop_back();
		} while (!_boxes.empty() && !(_boxes.back() -= size));

		_boxes.emplace_back(); // always at less one box!
		
	} while (reader.available());

	return 0;
}

void MP4Reader::flushMedias(Media::Source& source) {
	for (Track& track : _tracks)
		track.buffered = false;
	for (auto& it : _medias) {
		source.writeMedia(*it.second);
		delete it.second;
	}
	_medias.clear();
}

void MP4Reader::onFlush(const Packet& packet, Media::Source& source) {
	flushMedias(source);
	_boxes.resize(1);
	_boxes.back() = nullptr;
	_offset = _position = 0;
	_failed = false;
	_chunks.clear();
	_tracks.clear();
	_audios.clear();
	_videos.clear();
	_ids.clear();
	MediaReader::onFlush(packet, source);
}


} // namespace Mona
