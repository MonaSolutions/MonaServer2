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
#include "Mona/H264NALReader.h"
#include "Mona/ADTSReader.h"
#include "Mona/MP3Reader.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

UInt32 TSReader::parse(const Packet& packet, Media::Source& source) {

	BinaryReader input(packet.data(), packet.size());

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
	//	UInt8 ccount(value & 0x0f);		// continuty count
			
		// technically hasPD without hasAF is an error, see spec
			
		UInt64 pcr(0);
		if (byte & 0x20) { // has adaptation field
			 // process adaptation field and PCR
			UInt8 length(reader.read8());
			if (length >= 7) {
				if (reader.read8() & 0x10) {
					length -= 6;
					pcr = reader.read32();
					pcr <<= 1;	
					UInt16 extension(reader.read16());
					pcr |= (extension>>15)&1;
					pcr *= 300;
					pcr += (extension&0x1FF);
				}
				--length;
			}
			reader.next(length);
		}

		if(!pid) {
			// PAT
			if (hasHeader && hasContent) // assume that PAT table can't be split (pusi==true) + useless if no playload
				parsePAT(reader, source);
			continue;
		}

		const auto& it(_programs.find(pid));
		if (it == _programs.end())
			continue; // ignore unsupported track!

		if (!it->second) {
			// pid pointes a PSI table
			if (hasHeader && hasContent) // assume that PMT table can't be split (pusi==true) + useless if no playload
				parsePSI(reader, source);
			continue;
		}

		// Program known!

		if (it->second.firstTime) {
			// To decrease time value to support more easy long session use the first pts-pcr delta as reference time
			const auto& itPCR(_programs.find(it->second.pcrPID));
			if (itPCR != _programs.end() && !itPCR->second.firstTime) {
				// if has PCR program bound
				it->second.startTime = itPCR->second.startTime;
				it->second->time = itPCR->second->time; // in the case where pusi is not indicated, take the PCR time
			} else
				it->second.startTime = UInt32(round(pcr / 27000.0));
			it->second.firstTime = false;
		}

		UInt8 sequence(byte & 0x0f);
		UInt32 lost = ((sequence - it->second.sequence - 1) & 0x0F) * 184; // 184 is an approximation (impossible to know if missing packet had playload header or adaptation field)
		// On lost data, wait next header!
		if (lost && !it->second.waitHeader) {
			it->second.waitHeader = true;
			it->second->flush(source); // flush to reset the state!
		}

		if (hasHeader)
			parsePESHeader(reader, it->second);

		if (hasContent) {
			if (it->second.waitHeader) {
				lost += reader.available();
			} else
				it->second->read(Packet(packet, reader.current(), reader.available()), source);
		}
		
		if (lost && it->second.sequence != 0xFF) // if sequence==0xFF it's the first time, no real lost, juste wait header!
			source.reportLost(it->second.type, it->second->track, lost);
		it->second.sequence = sequence;
			
	} while (input.available());

	return 0;
}

void TSReader::parsePAT(BinaryReader& reader, Media::Source& source) {
	reader.next(reader.read8()); // ignore pointer field
	if (reader.read8()) { // table ID
		WARN("PID = 0 pointes a non PAT table");
		return; // don't try to parse it
	}

	UInt16 available(reader.read16() & 0x03ff); // ignoring unused and reserved bits
	UInt16 streamId = reader.read16(); // stream identifier number
	if (streamId != _streamId) {
		if(_streamId) {
			// Stream has changed => reset programs!
			for (auto& it : _programs) {
				if (it.second)
					it.second->flush(source);
			}
			_programs.clear();
			source.reset();
		}
		_streamId = streamId;
	}
	reader.next(3); // skip reserved bits + version/cni + sec# + last sec#
	available -= 5;

	if (available > reader.available()) {
		WARN("PAT splitted table not supported, maximum supported PMT is 42");
		available = reader.available()+1; // +1 to read until >= 4
	}

	while (available > 4) {
		reader.next(2); // skip program number
		_programs.emplace(reader.read16() & 0x1fff, nullptr); // 13 bits => PMT pids
		available -= 4;
	}
	// ignore 4 CRC bytes
}

void TSReader::parsePSI(BinaryReader& reader, Media::Source& source) {
	reader.next(reader.read8()); // ignore pointer field
	// https://en.wikipedia.org/wiki/Program-specific_information#Table_Identifiers
	switch (reader.read8()) {
		case 0x02:
			return parsePMT(reader, source);
		default: break;
	}
}
		
void TSReader::parsePMT(BinaryReader& reader, Media::Source& source) {
	UInt16 available(reader.read16() & 0x03ff); // ignoring unused and reserved bits
	reader.next(5); // skip tableid extension + reserved bits + version/cni + sec# + last sec#
	available -= 5;

	UInt16 pcrPID(reader.read16() & 0x1FFF);
	available -= 2;
			
	UInt16 piLen(reader.read16() & 0x0fff);
	available -= 2;
			
	reader.next(piLen); // skip program info
	available -= piLen;

	if (available > reader.available()) {
		WARN("PMT splitted table not supported, maximum supported programs is 33");
		available = reader.available()+1; // +1 to read until >= 4
	}
			
	while(available > 4) {
		UInt8 type(reader.read8());
		UInt16 pid(reader.read16() & 0x1fff);
		UInt16 esiLen(reader.read16() & 0x0fff);
		available -= 5;
				
		reader.next(esiLen);
		available -= esiLen;
	
		switch(type) {
			case 0x1b: { // H.264 video
				createProgram<H264NALReader, Media::TYPE_VIDEO>(pid, pcrPID, source);
				break;
			}
			case 0x0f: { // AAC Audio / ADTS
				createProgram<ADTSReader, Media::TYPE_AUDIO>(pid, pcrPID, source);
				break;
			}
			case 0x03: // ISO/IEC 11172-3 (MPEG-1 audio)
			case 0x04: { // ISO/IEC 13818-3 (MPEG-2 halved sample rate audio)
				createProgram<MP3Reader, Media::TYPE_AUDIO>(pid, pcrPID, source);
				break;
			}
			default:
				WARN("unsupported type ",String::Format<UInt8>("%.2x",type)," in PMT");

				const auto& it(_programs.lower_bound(pid));
				if (it == _programs.end() || it->first != pid)
					break;
				if (it->second)
					it->second->flush(source);
				_programs.erase(it);
				break;
		}
	}
			
	// ignore 4 CRC bytes
}

void TSReader::parsePESHeader(BinaryReader& reader, Program& pProgram) {
	// prefix 00 00 01 + stream id byte (http://dvd.sourceforge.net/dvdinfo/pes-hdr.html)
	UInt32 value(reader.read32());
	bool isVideo(value&0x20 ? true : false);
	if ((value & 0xFFC0)!=0x1C0) {
		WARN("PES start code not found or not a ",isVideo ? "video" : "audio"," packet");
		return;
	}
	pProgram.waitHeader = false;
	if (isVideo)
		pProgram->track = value&0x0F;
	else
		pProgram->track = value&0x1F;
	reader.next(3); // Ignore packet length and marker bits.

	// Need PTS
	UInt8 flags((reader.read8() & 0xc0) >> 6);

	// Check PES header length
	UInt8 length(reader.read8());

	if (flags&0x02) {

		// PTS
		double pts = double(((UInt64(reader.read8()) & 0x0e) << 29) | ((reader.read16() & 0xfffe) << 14) | ((reader.read16() & 0xfffe) >> 1));
		length -= 5;

		if(flags == 0x03) {
			// DTS
			double dts = double(((UInt64(reader.read8()) & 0x0e) << 29) | ((reader.read16() & 0xfffe) << 14) | ((reader.read16() & 0xfffe) >> 1));
			length -= 5;

			dts /= 90;

			pProgram->time = UInt32(round(dts)-pProgram.startTime);
			pProgram->compositionOffset =  UInt32(round(pts/90 - dts));

			//trace("pts "+pts+" dts "+dts+" comp "+_compositionTime +" stamp "+_timestamp +" total "+(_compositionTime+_timestamp));
			
		} else {
			pts /= 90;

			pProgram->time = UInt32(round(pts)-pProgram.startTime);
			pProgram->compositionOffset = 0;
		}

	}
	
	// skip other header data.
	if (length<0)
		WARN("Bad ",isVideo ? "video" : "audio", " PES length")
	else
		reader.next(length);
}

void TSReader::onFlush(const Packet& packet, Media::Source& source) {
	for (auto& it : _programs) {
		if (it.second)
			it.second->flush(source);
	}
	_programs.clear();
	_syncFound = false;
	_syncError = false;
	_streamId = 0;
	MediaReader::onFlush(packet, source);
}

} // namespace Mona
