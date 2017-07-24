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
#include "Mona/H264NALWriter.h"

namespace Mona {


struct TSWriter : MediaWriter, virtual Object {
	// https://en.wikipedia.org/wiki/MPEG_transport_stream
	// https://en.wikipedia.org/wiki/Packetized_elementary_stream
	// http://cmm.khu.ac.kr/korean/files/03.mpeg2ts2_timemodel_update_.pdf
	// max delay A/V (interlace) => https://en.wikipedia.org/wiki/Audio_to_video_synchronization
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
	void    writePMT(UInt32 time, const OnWrite& onWrite);

	void   writeES(UInt16 pid, UInt8& counter, UInt32 time, UInt16 compositionOffset, const Packet& packet, UInt32 esSize, const OnWrite& onWrite, bool randomAccess=true);
	
	UInt8  writePES(UInt16 pid, UInt8& counter, UInt32 time, bool randomAccess, UInt32 size, const OnWrite& onWrite);
	UInt8  writePES(UInt16 pid, UInt8& counter, UInt32 time, UInt16 compositionOffset, bool randomAccess, UInt32 size, const OnWrite& onWrite);

	UInt8  writeAdaptiveHeader(UInt16 pid, UInt32 time, bool randomAccess, UInt8 fillSize, BinaryWriter& writer);

	struct Track : virtual NullableObject {
		Track(MediaTrackWriter* pWriter) : _pWriter(pWriter) { _pWriter->beginMedia();  }
		~Track() { if (_pWriter) delete _pWriter; }

		operator bool() const { return _pWriter ? true : false; }
		Track& operator=(MediaTrackWriter* pWriter);
		MediaTrackWriter* operator->() { return _pWriter; }
		std::vector<std::string> langs;
	private:
		MediaTrackWriter* _pWriter;
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

	UInt8						_buffer[188];
	UInt8						_canWrite;
	UInt32						_toWrite;
};



} // namespace Mona
