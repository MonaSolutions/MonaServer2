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

#include "Mona/TSReader.h"
#include "Mona/NALNetReader.h"
#include "Mona/HEVC.h"
#include "Mona/AVC.h"
#include "Mona/ADTSReader.h"
#include "Mona/MP3Reader.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

TSReader::Program& TSReader::Program::reset(Media::Source& source) {
	// don't reset "type" to know the preivous type in the goal of not increase audio/video track if next type is unchanged
	// don't clear parameters to flush just on change!
	if (_pReader) {
		_pReader->flush(source);
		delete _pReader;
		_pReader = NULL;
	}
	return *this;
}


UInt32 TSReader::parse(Packet& buffer, Media::Source& source) {

	BinaryReader input(buffer.data(), buffer.size());

	do {

		if(!_syncFound) {
			while (input.read8()!=0x47) {
				if (!_syncError) {
					WARN("TSReader 47 signature not found");
					_syncError = true;
				}
				if (!input.available())
					return 0;
			}
			_syncFound = true;
			_syncError = false;
		}

		if (input.available() < 187)
			return input.available();

		BinaryReader reader(input.current(), 187);
		//Logs::Dump(reader.data(), reader.size());
		input.next(187);

		_syncFound = false;
			
		// Now 187 bytes after 0x47
			
		// top of second byte
		UInt8 byte(reader.read8());
	//	bool tei(byte&0x80 ? true : false); // error indicator
		bool hasHeader(byte &0x40 ? true : false); // payload unit start indication
//		bool tpri(byte&0x20 ? true : false); // transport priority indication
			
		// program ID = bottom of second byte and all of third
		UInt16 pid(((byte & 0x1f)<<8) | reader.read8());
			
		// fourth byte
		byte = reader.read8();

		//	UInt8 scramblingControl((value >> 6) & 0x03); // scrambling control for DVB-CSA, TODO?
		bool hasContent(byte & 0x10 ? true : false);	// has payload data
		// technically hasPD without hasAF is an error, see spec
			
		if (byte & 0x20) { // has adaptation field
			 // process adaptation field and PCR
			UInt8 length(reader.read8());
			if (length >= 7) {
				if (reader.read8() & 0x10)
					length -= reader.next(6);
				/*if (reader.read8() & 0x10) {
					length -= 6;
					pcr = reader.read32();
					pcr <<= 1;	
					UInt16 extension(reader.read16());
					pcr |= (extension>>15)&1;
					pcr *= 300;
					pcr += (extension&0x1FF);
				}*/
				--length;
			}
			reader.next(length);
		}

		if(!pid) {
			// PAT
			if (hasHeader && hasContent) // assume that PAT table can't be split (pusi==true) + useless if no playload
				parsePAT(reader.current(), reader.available(), source);
			continue;
		}
		if (pid == 0x1FFF)
			continue; // null packet, used for fixed bandwidth padding
		const auto& it(_programs.find(pid));
		if (it == _programs.end()) {
			if (!hasHeader || !hasContent)
				continue;
			const auto& itPMT(_pmts.find(pid));
			if (itPMT != _pmts.end()) // assume that PMT table can't be split (pusi==true) + useless if no playload
				parsePSI(reader.current(), reader.available(), itPMT->second, source);
			continue;
		}

		if (!it->second)
			continue;  // ignore unsupported track!

		// Program known!

		UInt8 sequence(byte & 0x0f);
		UInt32 lost = sequence - it->second.sequence;
		if (hasContent)
			--lost; // continuity counter is incremented just on playload, else it says same!
		lost = (lost & 0x0F) * 184; // 184 is an approximation (impossible to know if missing packet had playload header or adaptation field)
		// On lost data, wait next header!
		if (lost) {
			if (!it->second.waitHeader) {
				it->second.waitHeader = true;
				it->second->flush(source); // flush to reset the state!
			}
			if (it->second.sequence != 0xFF) // if sequence==0xFF it's the first time, no real lost, juste wait header!
				source.reportLost(it->second.type, lost, it->second->track);
		}
		it->second.sequence = sequence;
	
		if (hasHeader)
			readPESHeader(reader, it->second);

		if (hasContent) {
			if (it->second.waitHeader)
				lost += reader.available();
			else
				it->second->read(Packet(buffer, reader.current(), reader.available()), source);
		}
		
	} while (input.available());

	return 0;
}

void TSReader::parsePAT(const UInt8* data, UInt32 size, Media::Source& source) {
	BinaryReader reader(data, size);
	reader.next(reader.read8()); // ignore pointer field
	if (reader.read8()) { // table ID
		WARN("PID = 0 pointes a non PAT table");
		return; // don't try to parse it
	}

	map<UInt16, UInt8> pmts;
	size = reader.read16() & 0x03ff;
	if (reader.shrink(size) < size)
		WARN("PAT splitted table not supported, maximum supported PMT is 42");
	reader.next(5); // skip stream identifier + reserved bits + version/cni + sec# + last sec#

	while (reader.available() > 4) {
		reader.next(2); // skip program number
		pmts.emplace(reader.read16() & 0x1fff, 0xFF);  // 13 bits => PMT pids
	}
	// Use CRC bytes as stream identifier (if PAT change, stream has changed!)
	UInt32 crc = reader.read32();
	if (_crcPAT == crc)
		return;
	if (_crcPAT) {
		// Stream has changed => reset programs!
		for (auto& it : _programs) {
			if (it.second)
				it.second->flush(source);
		}
		_programs.clear();
		_properties.clear();
		_timeProperties = _properties.timeChanged();
		_startTime = -1;
		_audioTrack = 0; _videoTrack = 0;
		source.reset();
	}
	_pmts = move(pmts);
	_crcPAT = crc;
}

void TSReader::parsePSI(const UInt8* data, UInt32 size, UInt8& version, Media::Source& source) {
	if (!size--)
		return;
	// ignore pointer field
	UInt8 skip = *data++;
	if (skip >= size)
		return;
	data += skip;
	size -= skip;
	// https://en.wikipedia.org/wiki/Program-specific_information#Table_Identifiers
	switch (*data) {
		case 0x02:
			return parsePMT(++data, --size, version, source);
		default:;
	}
}
		
void TSReader::parsePMT(const UInt8* data, UInt32 size, UInt8& version, Media::Source& source) {
	BinaryReader reader(data, size);
	size = reader.read16() & 0x03ff;
	if (reader.shrink(size) < size)
		WARN("PMT splitted table not supported, maximum supported programs is 33");
	if (reader.available() < 9) {
		WARN("Invalid too shorter PMT table");
		return;
	}
	reader.next(2); // skip program number
	UInt8 value = (reader.read8()>>1)&0x1F;
	reader.next(2); // skip table sequences

	if (value == version)
		return; // no change!

	// Change doesn't reset a source.reset, because it's just an update (new program, or change codec, etc..),
	// but timestamp and state stays unchanged!
	version = value;

	reader.next(2); // pcrPID
	reader.next(reader.read16() & 0x0fff); // skip program info

	while(reader.available() > 4) {
		UInt8 value(reader.read8());
		UInt16 pid(reader.read16() & 0x1fff);

		Program& program = _programs[pid];
		Media::Type oldType = program.type;
	
		switch(value) {
			case 0x1b: { // H.264 video
				program.set<NALNetReader<AVC>>(Media::TYPE_VIDEO, source);
				break;
			}
			case 0x24: { // H.265 video
				program.set<NALNetReader<HEVC>>(Media::TYPE_VIDEO, source);
				break;
			}
			case 0x0f: { // AAC Audio / ADTS
				program.set<ADTSReader>(Media::TYPE_AUDIO, source);
				break;
			}
			case 0x03: // ISO/IEC 11172-3 (MPEG-1 audio)
			case 0x04: { // ISO/IEC 13818-3 (MPEG-2 halved sample rate audio)
				program.set<MP3Reader>(Media::TYPE_AUDIO, source);
				break;
			}
			default:
				WARN("unsupported type ",String::Format<UInt8>("%.2x", value)," in PMT");
				program.reset(source);
				reader.next(reader.read16() & 0x0fff); // ignore ESI
				continue;
		}

		if (oldType != program.type) {
			// added or type changed!
			if (program.type == Media::TYPE_AUDIO)
				program->track = ++_audioTrack;
			else if (program.type == Media::TYPE_VIDEO)
				program->track = ++_videoTrack;
		}
		readESI(reader, program);
	}
	// ignore 4 CRC bytes

	// Flush PROPERTIES if changed!
	if (_timeProperties < _properties.timeChanged()) {
		_timeProperties = _properties.timeChanged();
		source.addProperties(_properties);
	}
}

void TSReader::readESI(BinaryReader& reader, Program& program) {
	// Elmentary Stream Info
	// https://en.wikipedia.org/wiki/Program-specific_information#Table_Identifiers
	UInt16 available(reader.read16() & 0x0fff);
	while (available-- && reader.available()) {
		UInt8 type = reader.read8();
		if (type == 0xFF || !available--)
			continue; // null padding
		UInt8 size = reader.read8();
		const UInt8* data = reader.current();
		BinaryReader desc(data, size = reader.next(size));
		available -= size;
		switch (type) {
			case 0x0A: // ISO 639 language + Audio option
				data = desc.current();
				size = desc.next(3);
				while (size && !data[0]) {
					++data; // remove 0 possible prefix!
					--size;
				}
				if(size)
					_properties.setString(String(program->track, ".audioLang"), STR data, size);
				break;
			case 0x86: // Caption service descriptor http://atsc.org/wp-content/uploads/2015/03/Program-System-Information-Protocol-for-Terrestrial-Broadcast-and-Cable.pdf
				UInt8 count = desc.read8() & 0x1F;
				while (count--) {
					data = desc.current();
					size = desc.next(3);
					UInt8 channel = desc.read8();
					if ((channel & 0x80) && (channel&=0x3F) && channel<5) // channel must be superior to 0 (see spec.) and is CC extractable just if inferior or equal to 4
						_properties.setString(String((program->track - 1) * 4 + channel, ".textLang"), STR data, size);
					desc.next(2);
				}
				break;
		}
		
	}
}

void TSReader::readPESHeader(BinaryReader& reader, Program& pProgram) {
	// prefix 00 00 01 + stream id byte (http://dvd.sourceforge.net/dvdinfo/pes-hdr.html)
	UInt32 value(reader.read32());
	bool isVideo(value&0x20 ? true : false);
	if ((value & 0xFFC0)!=0x1C0) {
		WARN("PES start code not found or not a ",isVideo ? "video" : "audio"," packet");
		return;
	}
	pProgram.waitHeader = false;
	reader.next(3); // Ignore packet length and marker bits.

	// Need PTS
	UInt8 flags((reader.read8() & 0xc0) >> 6);

	// Check PES header length
	UInt8 length(reader.read8());

	if (flags&0x02) {

		// PTS
		double pts = double(((UInt64(reader.read8()) & 0x0e) << 29) | ((reader.read16() & 0xfffe) << 14) | ((reader.read16() & 0xfffe) >> 1));
		length -= 5;
		pts /= 90;

		double dts;

		if(flags == 0x03) {
			// DTS
			dts = double(((UInt64(reader.read8()) & 0x0e) << 29) | ((reader.read16() & 0xfffe) << 14) | ((reader.read16() & 0xfffe) >> 1));
			length -= 5;
			dts /= 90;

			if (pts < dts) {
				WARN("Decoding time ", dts, " superior to presentation time ", pts);
				dts = pts;
			}

		} else
			dts = pts;

		// To decrease time value on long session (TS can starts with very huge timestamp)
		if (_startTime > dts) {
			WARN("Time ",dts," inferior to start time ", _startTime);
			_startTime = dts;
		} else if(_startTime<0)
			_startTime = dts - min(200, dts); // + 200ms which is a minimum required offset with PCR (see TSWriter.cpp)
		pProgram->time = UInt32(round(dts) - _startTime);
		pProgram->compositionOffset = range<UInt16>(round(pts - dts));

		//DEBUG(isVideo ? "video " : "audio ", pProgram->track, " => ", pProgram->time, " ", pProgram->compositionOffset);
	}
	
	// skip other header data.
	if (length<0)
		WARN("Bad ",isVideo ? "video" : "audio", " PES length")
	else
		reader.next(length);
}

void TSReader::onFlush(Packet& buffer, Media::Source& source) {
	for (auto& it : _programs) {
		if (it.second)
			it.second->flush(source);
	}
	_programs.clear();
	_properties.clear();
	_timeProperties = _properties.timeChanged();
	_audioTrack = 0; _videoTrack = 0;
	_pmts.clear();
	_syncFound = false;
	_syncError = false;
	_crcPAT = 0;
	_startTime = -1;
	MediaReader::onFlush(buffer, source);
}

} // namespace Mona
