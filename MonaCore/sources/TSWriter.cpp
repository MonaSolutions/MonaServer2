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

#include "Mona/TSWriter.h"
#include "Mona/Crypto.h"
#include "Mona/ADTSWriter.h"
#include "Mona/NALNetWriter.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

#define FIRST_VIDEO_PID 33
#define FIRST_AUDIO_PID 256

// Spec requires PAT/PMT every 140ms, but for a network live way it consumes really too much bytes
// 1sec has been choosen to work correctly with WindowsMediaPlayer (especially its search feature which looks sensible for that)
#define PMT_PERIOD			1000

// An offset with PCR is necessary to make working forward/rewind feature on few player as VLC
#define PCR_OFFSET			200

static UInt8 _FF[188] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

TSWriter::Track& TSWriter::Track::operator=(MediaTrackWriter* pWriter) {
	if (_pWriter) {
		_pWriter->endMedia();
		delete _pWriter;
	}
	if ((_pWriter = pWriter))
		_pWriter->beginMedia();
	return *this;
}

bool TSWriter::Track::addLang(const char* textLang) {
	if (!textLang)
		return false;
	size_t size = strlen(textLang);
	if (size < 3) {
		DEBUG("TS Writer too short lang ", textLang, " ignored");
		return false;
	}
	_langs.emplace_back(textLang);
	return true;
}
bool TSWriter::Track::parseLang(UInt8 track, const Media::Properties& properties) {
	_langs.clear();
	if (type == Media::TYPE_AUDIO)
		return addLang(properties.getString(String(track, ".textLang")));
	// VIDEO
	bool changed = false;
	// 4 text langs available to describe by video track, no data description available in TS outside video track!
	for (UInt8 i = 1; i<5; ++i) {
		if (addLang(properties.getString(String(track + i, ".textLang"))))
			changed = true;
	}
	return changed;
}

void TSWriter::beginMedia(const OnWrite& onWrite) {
	_timePCR = 0;
	_toWrite = 0;
	_canWrite = 0;
	_firstPCR = true;
	_timePMT = 0;
	_changed = true;
	// Don't reset _version to allow to player to detect change
}

void TSWriter::endMedia(const OnWrite& onWrite) {
	if (_canWrite)
		ERROR("TS writer has miscalculated PES split and fill size");
	if (_toWrite)
		ERROR("A TS writer track has miscalculated its finalSize");
	for (auto& it : _audios) {
		if (it.second)
			it.second->endMedia();
	}
	for (auto& it : _videos)
		it.second->endMedia();
	_videos.clear();
	_audios.clear();
	_pids.clear();
}

void TSWriter::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	for (auto& it : _audios) {
		if (it.second.parseLang(it.first, properties))
			_changed = true; // update PMT!
	}
	for (auto& it : _videos) {
		if (it.second.parseLang(it.first, properties))
			_changed = true; // update PMT!
	}
}

void TSWriter::writePMT(BinaryWriter& writer, UInt32 time) {
	// Write just one PAT+PMT table on codec change to save bandwith and because programs can't change during TSWriter session (see http://www.etherguidesystems.com/help/sdos/mpeg/syntax/tablesections/pat.aspx)
	// Now write inside the both first 188 bytes packet (PAT + PMT) to allow recomposition stream on client side in a easy way (save the packet, and reuse it at stream beginning)
	// No splitable format for now, so maximum programs (tracks) for PMT is 33!
	if (_changed) {
		++_version;
		_changed = false;
	}

	UInt32 offset = writer.size();

	// PAT => 47 60 00 10   Pointer: 00   TableID: 00   Length: B0 0D   Fix: 00 01 C1 00 00   Program: 00 01 F0 00   CRC: 9D B0 81 9C 
	const auto& itPAT(_pids.emplace(0, 0).first);
	writer.write(EXPAND("\x47\x60\x00")).write8(0x10 | (itPAT->second++ % 0x10));
	writer.write(EXPAND("\x00\x00\xB0\x0D\x00\x01\xC1\x00\x00\x00\x01\xE0\x20"));
	// Write CRC
	writer.write32(Crypto::ComputeCRC32(writer.data() + offset + 5, (writer.size() - offset) - 5));
	// Fill with FF
	writer.write(_FF, 188 - (writer.size() - offset));


	offset = writer.size();

	// PMT (PID = 32) => 47 60 20 10   Pointer: 00   TableID: 02
	writer.write(EXPAND("\x47\x60\x20"));

	const auto& itPMT(_pids.emplace(0x20, 0).first);
	writer.write8(0x10 | (itPMT->second++ % 0x10)); // playload flag + counter
	writer.write16(2); // Pointer 00 + Table id 02, always 2 for PMT

	writer.next(2); // length

	writer.write16(1); // program number
	writer.write8(0xC1 | ((_version%32)<<1)); // version
	writer.write16(0); // table sequences (non used)

	_pidPCR = 0;

	for (const auto& it : _videos) {
		if (!_pidPCR) {
			writer.write16(0xE000 | (_pidPCR = (FIRST_VIDEO_PID+it.first)));
			writer.write16(0xF000);
		}
		writer.write8(it.second.codec==Media::Video::CODEC_HEVC ? 0x24 : 0x1b); // HEVC/H264 codec
		writer.write16(0xE000 | (FIRST_VIDEO_PID+it.first));
		
		UInt16 esiSize = UInt16(it.second.langs().size())*6;
		if (esiSize)
			esiSize += 3;
		writer.write16(0xF000 | esiSize);
		if (!esiSize)
			continue;
		writer.write(0xe0 | (esiSize - 2));
		for(UInt8 i = 0; i<4; ++i)
			writer.write(it.second.lang(i).data(), 3).write8(0xc0 | ((it.first*4)+i)).write16(0x3FFF);
	}
	for (const auto& it : _audios) {
		if (!_pidPCR) {
			writer.write16(0xE000 | (_pidPCR = (FIRST_AUDIO_PID+it.first)));
			writer.write16(0xF000);
		}

		if (it.second)
			writer.write8(0x0f);
		else
			writer.write8(0x03); // MP3 codec
		writer.write16(0xE000 | (FIRST_AUDIO_PID+it.first));

		UInt16 esiSize = 0;
		if (!it.second.langs().empty())
			esiSize += 6;
		writer.write16(0xF000 | esiSize);
		if (esiSize)
			writer.write8(0x0a).write8(4).write(it.second.lang().data(), 3).write8(0);
	}

	// Write len
	BinaryWriter(writer.buffer().data() + offset + 6, 2).write16(0xB000 | (((writer.size() - offset) -4)&0x3FF));

	// Write CRC
	writer.write32(Crypto::ComputeCRC32(writer.data() + offset + 5, (writer.size() - offset) - 5));
	// Fill with FF
	writer.write(_FF, 188 - (writer.size() - offset));

	_timePMT = time;
}

void TSWriter::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!onWrite)
		return;

	// decrements track to be zero based (and pass track 0 to default track 1)
	if (track)
		--track;

	auto it(_audios.lower_bound(track));
	if (it == _audios.end() || it->first != track) {
		if (tag.codec != Media::Audio::CODEC_AAC && tag.codec != Media::Audio::CODEC_MP3) {
			ERROR("Audio track ",track," ignored, TS format doesn't support ", Media::Audio::CodecToString(tag.codec), " codec");
			return;
		}
		if (_videos.size() + _audios.size() == 33) { // else require splitted PMT table!
			ERROR("Audio track ",track," ignored, current TSWriter implementation doesn't support more than 33 tracks");
			return;
		}
		
		// add the new track
		it = _audios.emplace_hint(it, SET, forward_as_tuple(track), forward_as_tuple(tag.codec, tag.codec == Media::Audio::CODEC_AAC ? new ADTSWriter() : NULL));
		_changed = true;
	} else if (tag.codec == Media::Audio::CODEC_AAC) {
		if (!it->second) {
			it->second = new ADTSWriter();
			_changed = true;
		}
	} else if (it->second) {
		it->second = NULL;
		_changed = true;
	}

	const auto& itPID(_pids.emplace(FIRST_AUDIO_PID + it->first, 0).first);

	shared<Buffer> pBuffer(SET);
	BinaryWriter writer(*pBuffer);
	if (it->second) {
		// AAC
		UInt32 finalSize;
		MediaTrackWriter::OnWrite onAudioWrite([this, &itPID, &tag, &writer, &finalSize](const Packet& packet) {
			writeES(writer, itPID->first, itPID->second, tag.time, 0, packet, finalSize);
		});
		it->second->writeAudio(tag, packet, onAudioWrite, finalSize);
	} else // MP3
		writeES(writer, itPID->first, itPID->second, tag.time, 0, packet, packet.size());
	onWrite(Packet(pBuffer));

//	if (_canWrite || _toWrite)
//		int breakPoint = 0;
}

void TSWriter::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!onWrite)
		return;

	// decrements track to be zero based (and pass track 0 to default track 1)
	if (track)
		--track;

	auto it(_videos.lower_bound(track));
	if (it == _videos.end() || it->first != track) {
		if (tag.codec != Media::Video::CODEC_H264 && tag.codec != Media::Video::CODEC_HEVC) {
			ERROR("Video track ",track," ignored, TS format doesn't support video ", Media::Video::CodecToString(tag.codec), " codec");
			return;
		}
		if (_videos.size() + _audios.size() == 33) { // else require splitted PMT table!
			ERROR("Video track ",track," ignored, current TSWriter implementation doesn't support more than 33 tracks");
			return;
		}
		
		// add the new track
		it = _videos.emplace_hint(it, SET, forward_as_tuple(track),forward_as_tuple(tag.codec, tag.codec == Media::Video::CODEC_H264 ? (MediaTrackWriter*)new NALNetWriter<AVC>() : (MediaTrackWriter*)new NALNetWriter<HEVC>()));
		_changed = true;
	}

	const auto& itPID(_pids.emplace(FIRST_VIDEO_PID + it->first, 0).first);

	shared<Buffer> pBuffer(SET);
	BinaryWriter writer(*pBuffer);
	UInt32 finalSize;
	MediaTrackWriter::OnWrite onVideoWrite([this, &itPID, &tag, &writer, &finalSize](const Packet& packet){
		writeES(writer, itPID->first, itPID->second, tag.time, tag.compositionOffset, packet, finalSize, tag.frame==Media::Video::FRAME_KEY);
	});
	it->second->writeVideo(tag, packet, onVideoWrite, finalSize);
	onWrite(Packet(pBuffer));
	//	if (_canWrite || _toWrite)
	//		int breakPoint = 0;
}

void TSWriter::writeES(BinaryWriter& writer, UInt16 pid, UInt8& counter, UInt32 time, UInt16 compositionOffset, const Packet& packet, UInt32 esSize, bool randomAccess) {
	Packet playload(packet);
	while (playload) {
		if (!_toWrite) {
			if (_canWrite) {
				ERROR("TS writer program ", pid, " has miscalculated PES split and fill size");
				writer.write(_FF, _canWrite);
			}
			_canWrite = writePES(writer, pid, counter, time, compositionOffset, randomAccess, _toWrite = esSize);
		} else if (!_canWrite)
			_canWrite = writePES(writer, pid, counter, time, randomAccess, _toWrite);
		UInt32 toWrite = playload.size() > _canWrite ? _canWrite : playload.size();
		writer.write(playload.data(), toWrite);
		playload += toWrite;
		_canWrite -= toWrite;
		if (toWrite>_toWrite) {
			ERROR("TS writer program ",pid," has miscalculated its finalSize");
			_toWrite = 0;
		} else
			_toWrite -= toWrite;
	}
}

UInt8 TSWriter::writePES(BinaryWriter& writer, UInt16 pid, UInt8& counter, UInt32 time, bool randomAccess, UInt32 size) {
	writer.write8(0x47).write16(pid);
	if (size >= 184) {
		writer.write8(0x10 | (counter++ % 0x10));
		return 184;
	}
	// fill
	writer.write8(0x30 | (counter++ % 0x10)); // adaptation flag
	writeAdaptiveHeader(writer, pid, time, randomAccess, 184 - size);
	return size;
}

UInt8 TSWriter::writePES(BinaryWriter& writer, UInt16 pid, UInt8& counter, UInt32 time, UInt16 compositionOffset, bool randomAccess, UInt32 size) {

	if (_changed || abs(Int32(time-_timePMT)) >= PMT_PERIOD)
		writePMT(writer, time); // send periodic PMT infos
	
	UInt64 pts(0), dts(0);
	UInt8 flags(0), length(0);
	UInt8 fillSize(175); // 188-13 (13 is the minimum header size)

	if (size) { // no playload data => no timestamp (usefull for PCR request, see below)
		if (compositionOffset) {
			flags = 0xC0;
			length = 10;

			dts = (time+PCR_OFFSET)*90;
			pts = (90 * compositionOffset) + dts;
			fillSize -= 10;
		} else {
			flags = 0x80;
			length = 5;
			pts = (time+PCR_OFFSET)*90;
			fillSize -= 5;
		}
	}

	if (size>=fillSize)
		fillSize = 0;
	else
		fillSize -= size;

	 // force available space for pcr!
	if (fillSize<8) {
		Int32 deltaTime(time-_timePCR);
		if (_firstPCR || deltaTime >= 100) {  // at less every 100ms for PCR track
			if (pid == _pidPCR)
				fillSize = 8;
			else if (deltaTime >= 500) { // 500ms without PCR track, pulse it now (urgent)
				// just PCR program (_pidPCR) can deliver the PCR timecode
				const auto& it(_pids.emplace(_pidPCR, 0).first);
				writer.write(_FF, writePES(writer, it->first, it->second, time, compositionOffset, false, 0));
			}
		}
	}

	UInt32 offset = writer.size();
	writer.write8(0x47); // syncword
	writer.write16(0x4000 | pid); // pid

	/// ADAPTIVE HEADER ////
	if (fillSize) {
		writer.write8(0x30 | (counter++ % 0x10)); // adaptation field followed by payload
		writeAdaptiveHeader(writer, pid, time, randomAccess, fillSize);
	} else
		writer.write8(0x10 | (counter++ % 0x10)); // no adaptation field, payload only

	/// PES HEADER ////

	if(!size) {
		// 0xFF => type private! To make more easy counter management! (playload always present => counter always incremented)
		// It's the case where just PCR is sent!
		writer.write32(0x1FF);
	} else
		writer.write32((pid<FIRST_AUDIO_PID) ? 0x1E0 : 0x1C0);

	size += (3 + length);
	writer.write16(size>0xFFFF ? 0 : size);  // size>0xFFFF just for video (optional for video)

	writer.write8(0x80); // 10 (fixed),	PES scrambling control,	data alignment indicator,	copyright,	original or copy
	writer.write8(flags).write8(length);

	if (pts)
		writer.write8(((pts>>29)&0x0E) | 0x21).write16(((pts >> 14) & 0xfffe) | 1).write16(((pts << 1) & 0xfffe) | 1);
	if (dts)
		writer.write8(((dts>>29)&0x0E) | 0x21).write16(((dts >> 14) & 0xfffe) | 1).write16(((dts << 1) & 0xfffe) | 1);

	return 188 - (writer.size()-offset);
}

void TSWriter::writeAdaptiveHeader(BinaryWriter& writer, UInt16 pid, UInt32 time, bool randomAccess, UInt8 fillSize) {
	UInt32 offset = writer.size();
	writer.write8(--fillSize); // size of Adaptive Field
	if (fillSize >= 7 && pid == _pidPCR && time>_timePCR) {
		// Add PCR if place available and if monothonic,
		// on wrap around, wait next PES header to fix PCR (PCR is optional here)
		writer.write8(randomAccess ? 0x50 : 0x10); // timecode flag + random access flag
		UInt64 pcr(time*90);
		writer.write32((pcr>>1) & 0xFFFFFFFF).write16((pcr&0x01)<<15);
		_timePCR = time;
		_firstPCR = false;
	} else if(fillSize)
		writer.write8(0); // flags
	// Fill with FF
	writer.write(_FF, ++fillSize-(writer.size()-offset));
}




} // namespace Mona
