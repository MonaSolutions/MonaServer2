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
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

#define FIRST_VIDEO_PID 33
#define TRACK_MAX		4076
#define FIRST_AUDIO_PID 4110

// Spec requires PAT/PMT every 140ms, but for a network live way it consumes really too much bytes
// 1sec has been choosen to work correctly with WindowsMediaPlayer (especially its search feature which looks sensible for that)
#define PMT_PERIOD			1000

// An offset with PCR is necessary to make working forward/rewind feature on few player as VLC
#define PCR_OFFSET			200

static UInt8 _FF[182] = {
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
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const Packet _Fill(_FF,sizeof(_FF));

TSWriter::~TSWriter() {
	for (const auto& it : _audios) {
		if (it.second)
			delete it.second;
	}
}

void TSWriter::beginMedia(const OnWrite& onWrite) {
	_timePCR = 0;
	_toWrite = 0;
	_canWrite = 0;
	_firstPCR = true;
	_timePMT = 0;
	// Don't reset _version to make compatible with long session/instance of TSWriter
}

void TSWriter::endMedia(const OnWrite& onWrite) {
	if (_canWrite)
		ERROR("TS writer has miscalculated PES split and fill size");
	if (_toWrite)
		ERROR("A TS writer track has miscalculated its finalSize");
	for (const auto& it : _audios) {
		if (it.second) {
			it.second->endMedia();
			delete it.second;
		}
	}
	for (auto& it : _videos)
		it.second.endMedia();
	_videos.clear();
	_audios.clear();
	_pids.clear();
}

void TSWriter::updatePMT(UInt32 time, const OnWrite& onWrite) {
	++_version;
	writePMT(time, onWrite);
}

void TSWriter::writePMT(UInt32 time, const OnWrite& onWrite) {
	// Write just one PAT+PMT table on codec change to save bandwith and because programs can't change during TSWriter session (see http://www.etherguidesystems.com/help/sdos/mpeg/syntax/tablesections/pat.aspx)
	// Now write inside the both first 188 bytes packet (PAT + PMT) to allow recomposition stream on client side in a easy way (save the packet, and reuse it at stream beginning)
	// No splitable format for now, so maximum programs (tracks) for PMT is 33!


	// PAT => 47 60 00 10   Pointer: 00   TableID: 00   Length: B0 0D   Fix: 00 01 C1 00 00   Program: 00 01 F0 00   CRC: 9D B0 81 9C 
	BinaryWriter writer(_buffer, 188);
	const auto& itPAT(_pids.emplace(0, 0).first);
	writer.write(EXPAND("\x47\x60\x00")).write8(0x10 | (itPAT->second++ % 0x10));
	writer.write(EXPAND("\x00\x00\xB0\x0D\x00\x01\xC1\x00\x00\x00\x01\xE0\x20"));
	// Write CRC
	writer.write32(Crypto::ComputeCRC32(_buffer + 5, writer.size() - 5));
	onWrite(writer);
	// Fill with FF
	onWrite(Packet(_Fill, _Fill.data(), 188 - writer.size()));


	// PMT (PID = 8188) => 47 60 20 10   Pointer: 00   TableID: 02
	writer.clear();
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
		writer.write8(0x1b); // H264 codec
		writer.write16(0xE000 | (FIRST_VIDEO_PID+it.first));
		writer.write16(0xF000);
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
		writer.write16(0xF000);
	}

	// Write len
	BinaryWriter(_buffer + 6, 2).write16(0xB000 | ((writer.size()-4)&0x3FF));

	// Write CRC
	writer.write32(Crypto::ComputeCRC32(_buffer + 5, writer.size() - 5));

	onWrite(writer);
	// Fill with FF
	onWrite(Packet(_Fill, _Fill.data(), 188 - writer.size()));

	_timePMT = time;
}

void TSWriter::writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!onWrite)
		return;

	auto it(_audios.lower_bound(track));
	bool newTrack(false);
	if (it == _audios.end() || it->first != track) {
		if (tag.codec != Media::Audio::CODEC_AAC && tag.codec != Media::Audio::CODEC_MP3 && tag.codec != Media::Audio::CODEC_MP3_8K) {
			ERROR("Audio track ",track," ignored, TS format doesn't support ", Media::Audio::CodecToString(tag.codec), " codec");
			return;
		}
		if (track>TRACK_MAX) {
			ERROR("Audio track ",track," ignored, current TSWriter implementation doesn't support a track superior to ",TRACK_MAX);
			return;
		}
		if (_audios.size()==32) {
			ERROR("Audio track ",track," ignored, current TSWriter implementation doesn't support more than 32 audio tracks");
			return;
		}
		if (_videos.size() + _audios.size() == 33) {
			ERROR("Audio track ",track," ignored, current TSWriter implementation doesn't support more than 33 tracks");
			return;
		}
		
		// add the new track
		it = _audios.emplace_hint(it, track, tag.codec == Media::Audio::CODEC_AAC ? new ADTSWriter() : NULL);
		updatePMT(tag.time, onWrite);
		newTrack = true;
	} else if (tag.codec == Media::Audio::CODEC_AAC) {
		if (!it->second) {
			it->second = new ADTSWriter();
			updatePMT(tag.time, onWrite); // change from MP3 to AAC
			newTrack = true;
		}
	} else if (it->second) {
		it->second->endMedia();
		delete it->second;
		it->second = NULL;
		updatePMT(tag.time, onWrite); // change from AAC to MP3
	}

	UInt8 streamId(distance(_audios.begin(),it));
	const auto& itPID(_pids.emplace(FIRST_AUDIO_PID + it->first, 0).first);

	if (!it->second) // MP3
		return writeES(itPID->first, itPID->second, streamId, tag.time, 0, packet, packet.size(), onWrite);

	// AAC
	if (newTrack)
		it->second->beginMedia();
	UInt32 finalSize;
	TrackWriter::OnWrite onAudioWrite([this, &itPID, streamId, &tag, &onWrite, &finalSize](const Packet& packet){
		writeES(itPID->first, itPID->second, streamId, tag.time, 0, packet, finalSize, onWrite);
	});
	it->second->writeAudio(tag, packet, onAudioWrite, finalSize);
//	if (_canWrite || _toWrite)
//		int breakPoint = 0;
}

void TSWriter::writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!onWrite)
		return;

	auto it(_videos.lower_bound(track));
	bool newTrack(false);
	if (it == _videos.end() || it->first != track) {
		if (tag.codec != Media::Video::CODEC_H264) {
			ERROR("Video track ",track," ignored, TS format doesn't support video ", Media::Video::CodecToString(tag.codec), " codec");
			return;
		}
		if (track>TRACK_MAX) {
			ERROR("Video track ",track," ignored, current TSWriter implementation doesn't support a track superior to ",TRACK_MAX);
			return;
		}
		if (_videos.size()==16) {
			ERROR("Video track ",track," ignored, current TSWriter implementation doesn't support more than 16 video tracks");
			return;
		}
		if (_videos.size() + _audios.size() == 33) {
			ERROR("Video track ",track," ignored, current TSWriter implementation doesn't support more than 33 tracks");
			return;
		}
		
		// add the new track
		it = _videos.emplace_hint(it, piecewise_construct, forward_as_tuple(track),forward_as_tuple());
		updatePMT(tag.time, onWrite);
		newTrack = true;
	}

	UInt8 streamId(distance(_videos.begin(),it));
	const auto& itPID(_pids.emplace(FIRST_VIDEO_PID + it->first, 0).first);
	if (newTrack)
		it->second.beginMedia();
	UInt32 finalSize;
	TrackWriter::OnWrite onVideoWrite([this, &itPID, streamId, &tag, &onWrite, &finalSize](const Packet& packet){
		writeES(itPID->first, itPID->second, streamId, tag.time, tag.compositionOffset, packet, finalSize, onWrite, tag.frame==Media::Video::FRAME_KEY);
	});
	it->second.writeVideo(tag, packet, onVideoWrite, finalSize);
//	if (_canWrite || _toWrite)
//		int breakPoint = 0;
}

void TSWriter::writeES(UInt16 pid, UInt8& counter, UInt8 streamId, UInt32 time, UInt32 compositionOffset, const Packet& packet, UInt32 esSize, const OnWrite& onWrite, bool randomAccess) {
	
	Packet data(packet);

	while (data) {
		if (!_toWrite) {
			if (_canWrite) {
				ERROR("TS writer program ", pid, " has miscalculated PES split and fill size");
				onWrite(Packet(_Fill, _Fill.data(), _canWrite));
			}
			_canWrite = writePES(pid, counter, streamId, time, compositionOffset, randomAccess, _toWrite = esSize, onWrite);
		} else if (!_canWrite)
			_canWrite = writePES(pid, counter, time, randomAccess, _toWrite, onWrite);
		Packet fragment(data, data.data(), data.size() > _canWrite ? _canWrite : data.size());
		onWrite(fragment);
		data += fragment.size();
		_canWrite -= fragment.size();
		if (fragment.size()>_toWrite) {
			ERROR("TS writer program ",pid," has miscalculated its finalSize");
			_toWrite = 0;
		} else
			_toWrite -= fragment.size();
	}
}

UInt8 TSWriter::writePES(UInt16 pid, UInt8& counter, UInt32 time, bool randomAccess, UInt32 size, const OnWrite& onWrite) {

	BinaryWriter writer(_buffer, 13);
	writer.write8(0x47).write16(pid);
	if (size >= 184) {
		writer.write8(0x10 | (counter++ % 0x10));
		onWrite(writer);
		return 184;
	}
	// fill
	UInt8 fillSize(184-size);
	writer.write8(0x30 | (counter++ % 0x10)); // adaptation flag
	fillSize -= writeAdaptiveHeader(pid, time, randomAccess, fillSize, writer);

	onWrite(writer);
	if (fillSize)
		onWrite(Packet(_Fill, _Fill.data(), fillSize));
	return size;
}

UInt8 TSWriter::writePES(UInt16 pid, UInt8& counter, UInt8 streamId, UInt32 time, UInt32 compositionOffset, bool randomAccess, UInt32 size, const OnWrite& onWrite) {

	if (Int32(time-_timePMT) >= PMT_PERIOD)
		writePMT(time, onWrite); // send periodic PMT infos
	
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
				onWrite(Packet(_Fill, _Fill.data(), writePES(it->first, it->second, streamId, time, compositionOffset, false, 0, onWrite)));
			}
		}
	}

	BinaryWriter writer(_buffer,188);
	writer.write8(0x47); // syncword
	writer.write16(0x4000 | pid); // pid

	/// ADATPVIE HEADER ////
	if (fillSize) {
		writer.write8(0x30 | (counter++ % 0x10)); // adaptation flag
		fillSize -= writeAdaptiveHeader(pid, time, randomAccess, fillSize, writer);
	} else
		writer.write8(0x10 | (counter++ % 0x10)); // no adaptation flag
	
	UInt32 pusiPos(writer.size());

	/// PES HEADER ////

	if (size)
		writer.write32(((pid<FIRST_AUDIO_PID) ? 0x1E0 : 0x1C0) | streamId);
	else {
		// 0xFF => type private! To make more easy counter management! (playload always present => counter always incremented)
		// It's the case where just PCR is sent!
		writer.write32(0x1FF);
	}

	size += (3 + length);
	writer.write16(size>0xFFFF ? 0 : size);  // size>0xFFFF just for video (optional for video)

	writer.write8(0x80); // 10 (fixed),	PES scrambling control,	data alignment indicator,	copyright,	original or copy
	writer.write8(flags).write8(length);

	if (pts)
		writer.write8(((pts>>29)&0x0E) | 0x21).write16(((pts >> 14) & 0xfffe) | 1).write16(((pts << 1) & 0xfffe) | 1);
	if (dts)
		writer.write8(((dts>>29)&0x0E) | 0x21).write16(((dts >> 14) & 0xfffe) | 1).write16(((dts << 1) & 0xfffe) | 1);

	if (fillSize) {
		onWrite(Packet(writer.data(),pusiPos));
		onWrite(Packet(_Fill, _Fill.data(), fillSize));
		onWrite(Packet(writer.data()+pusiPos,writer.size()-pusiPos));
	} else
		onWrite(writer);
	return 188 - writer.size()-fillSize;
}

UInt8 TSWriter::writeAdaptiveHeader(UInt16 pid, UInt32 time, bool randomAccess, UInt8 fillSize, BinaryWriter& writer) {
	writer.write8(--fillSize); // size of Adaptive Field
	if (fillSize >= 7 && pid == _pidPCR && time>_timePCR) {
		// Add PCR if place available and if monothonic,
		// on wrap around, wait next PES header to fix PCR (PCR is optional here)
		writer.write8(randomAccess ? 0x50 : 0x10); // timecode flag + random access flag
		UInt64 pcr(time*90);
		writer.write32((pcr>>1) & 0xFFFFFFFF).write16((pcr&0x01)<<15);
		_timePCR = time;
		_firstPCR = false;
	} else if (fillSize)
		writer.write8(0); // flags
	return writer.size()-4;
}




} // namespace Mona
