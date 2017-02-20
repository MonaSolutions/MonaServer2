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
#include "Mona/MediaReader.h"

namespace Mona {

class TSReader : public virtual Object, public MediaReader {
	// http://www.iptvdictionary.com/iptv_dictionary_MPEG_Transport_Stream_TS_definition.html
	// http://apple-http-osmf.googlecode.com/svn/trunk/src/at/matthew/httpstreaming/HTTPStreamingMP2PESVideo.as
	// TS complete description => http://www.gwg.nga.mil/misb/docs/standards/ST1402.pdf
	// https://en.wikipedia.org/wiki/Packetized_elementary_stream
	// https://en.wikipedia.org/wiki/Program-specific_information
	// http://dvbsnoop.sourceforge.net/examples/example-pat.html
	// http://dvd.sourceforge.net/dvdinfo/pes-hdr.html

public:
	TSReader() : _syncFound(false), _syncError(false), _streamId(0) {}
	
private:

	struct Program : virtual NullableObject {
		Program(TrackReader* pReader) : type(Media::TYPE_NONE), _pReader(pReader), pcrPID(0), firstTime(true),startTime(0), waitHeader(true), sequence(0xFF) {}
		~Program() { if (_pReader) delete _pReader; }

		operator bool() const { return _pReader ? true : false; }

		Program&			operator=(TrackReader* pReader) { if (_pReader) delete _pReader; _pReader = pReader; return *this; }
		TrackReader* operator->() { return _pReader; }
		TrackReader& operator*() { return *_pReader; }

		Media::Type	 type;
		bool		 waitHeader;
		UInt8		 sequence;

		bool		 firstTime;
		UInt32		 startTime;
		UInt16		 pcrPID;
	private:
		TrackReader* _pReader;
	};

	UInt32 parse(const Packet& packet, Media::Source& source);

	void	parsePAT(BinaryReader& reader, Media::Source& source);
	void	parsePSI(BinaryReader& reader, Media::Source& source);
	void	parsePMT(BinaryReader& reader, Media::Source& source);
	
	void	parsePESHeader(BinaryReader& reader, Program& pProgram);

	void    onFlush(const Packet& packet, Media::Source& source);

	template<typename TrackReaderType, Media::Type type>
	void createProgram(UInt16 pid, UInt16 pcrPID, Media::Source& source) {
		auto it(_programs.lower_bound(pid));
		if (it != _programs.end() && it->first == pid) {
			if (!it->second || typeid(*it->second) != typeid(TrackReaderType)) {
				if (it->second)
					it->second->flush(source);
				it->second = new TrackReaderType(); // new codec
			}
		} else
			it = _programs.emplace_hint(it, std::piecewise_construct, std::forward_as_tuple(pid), std::forward_as_tuple(new TrackReaderType()));
		it->second.type = type;
		it->second.pcrPID = pcrPID;
	}


	std::map<UInt16, Program>   _programs;
	bool						_syncFound;
	bool						_syncError;
	UInt16						_streamId;
};


} // namespace Mona
