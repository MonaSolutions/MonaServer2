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

#pragma once

#include "Mona/Mona.h"
#include "MediaWriter.h"

namespace Mona {


struct TSWriter : MediaWriter, virtual Object {
	// https://en.wikipedia.org/wiki/MPEG_transport_stream
	// https://en.wikipedia.org/wiki/Packetized_elementary_stream
	// https://en.wikipedia.org/wiki/Program-specific_information
	// http://cmm.khu.ac.kr/korean/files/03.mpeg2ts2_timemodel_update_.pdf
	// max delay A/V (interlace) => https://en.wikipedia.org/wiki/Audio_to_video_synchronization
	// language = https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.03.01_40/en_300468v010301o.pdf
	// CRC => http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
	/*
		Acceptable error when using with MediaSocket in UDP =>
		ts error: libdvbpsi error (PSI decoder): TS discontinuity (received 0, expected 6) for PID 0 => nevermind, packet is processed, and save PID between sessions could limit the maximum number of PIDs usable on long session
		ts error: libdvbpsi error (PSI decoder): TS discontinuity (received 0, expected 6) for PID 32 => nevermind, packet is processed, and save PID between sessions could limit the maximum number of PIDs usable on long session
		core error: ES_OUT_SET_(GROUP_)PCR is called too late (pts_delay increased to 1725 ms)
		core error: ES_OUT_RESET_PCR called */

	TSWriter() : _version(0) {}

	void beginMedia(const OnWrite& onWrite);
	void writeProperties(const Media::Properties& properties, const OnWrite& onWrite);
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void endMedia(const OnWrite& onWrite);

private:
	void    writePMT(BinaryWriter& writer, UInt32 time);

	void   writeES(BinaryWriter& writer, UInt16 pid, UInt8& counter, UInt32 time, UInt16 compositionOffset, const Packet& packet, UInt32 esSize, bool randomAccess=true);
	
	UInt8  writePES(BinaryWriter& writer, UInt16 pid, UInt8& counter, UInt32 time, bool randomAccess, UInt32 size);
	UInt8  writePES(BinaryWriter& writer, UInt16 pid, UInt8& counter, UInt32 time, UInt16 compositionOffset, bool randomAccess, UInt32 size);

	void  writeAdaptiveHeader(BinaryWriter& writer, UInt16 pid, UInt32 time, bool randomAccess, UInt8 fillSize);

	struct Track : virtual Object {
		NULLABLE(!_pWriter)
		Track(Media::Audio::Codec codec, MediaTrackWriter* pWriter) : type(Media::TYPE_AUDIO), _pWriter(pWriter), codec(codec) {
			if (pWriter)
				pWriter->beginMedia();
		}
		Track(Media::Video::Codec codec, MediaTrackWriter* pWriter) : type(Media::TYPE_VIDEO), _pWriter(pWriter), codec(codec) {
			if(pWriter)
				pWriter->beginMedia(); 
		}
		~Track() { if (_pWriter) delete _pWriter; }

		const Media::Type type;
		const UInt8 codec;

		Track& operator=(MediaTrackWriter* pWriter);
		MediaTrackWriter* operator->() { return _pWriter; }
		MediaTrackWriter& operator*() { return *_pWriter; }
		const std::vector<std::string>& langs() const { return _langs; }
		const std::string& lang(UInt8 i=0) const { return _langs[i]; }
		bool parseLang(UInt8 track, const Media::Properties& properties);
	private:
		bool addLang(const char* textLang);
		MediaTrackWriter*			_pWriter;
		std::vector<std::string>	_langs;  // max size is 4: match CC channels max option + don't exceed PAT table of 188 bytes!
	};

	std::map<UInt8, Track>		_videos;
	std::map<UInt8, Track>		_audios;

	UInt8						_version;
	bool						_changed;
	UInt32						_timePMT;

	std::map<UInt16, UInt8>		_pids;
	UInt16						_pidPCR;
	bool						_firstPCR;
	UInt32						_timePCR;

	UInt8						_canWrite;
	UInt32						_toWrite;
};



} // namespace Mona
