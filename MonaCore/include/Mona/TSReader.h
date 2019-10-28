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

struct TSReader : virtual Object, MediaReader {
	// http://www.iptvdictionary.com/iptv_dictionary_MPEG_Transport_Stream_TS_definition.html
	// http://apple-http-osmf.googlecode.com/svn/trunk/src/at/matthew/httpstreaming/HTTPStreamingMP2PESVideo.as
	// TS complete description => http://www.gwg.nga.mil/misb/docs/standards/ST1402.pdf
	// https://en.wikipedia.org/wiki/Packetized_elementary_stream
	// https://en.wikipedia.org/wiki/Program-specific_information
	// http://dvbsnoop.sourceforge.net/examples/example-pat.html
	// http://dvd.sourceforge.net/dvdinfo/pes-hdr.html

	TSReader() : _syncFound(false), _syncError(false), _crcPAT(0), _audioTrack(0), _videoTrack(0), _startTime(-1), _timeProperties(_properties.timeChanged()) {}
	
private:

	struct Program : virtual Object {
		NULLABLE(!_pReader)
		Program() : type(Media::TYPE_NONE), _pReader(NULL), waitHeader(true), sequence(0xFF) {}
		~Program() { if (_pReader) delete _pReader; }

		template<typename TrackReaderType>
		Program& set(Media::Type type, Media::Source& source) {
			// don't clear parameters to flush just on change!
			if (_pReader && typeid(*_pReader) == typeid(TrackReaderType))
				return *this;
			this->type = type;
			if (_pReader)
				_pReader->flush(source);
			_pReader = new TrackReaderType();
			return *this;
		}
		Program& reset(Media::Source& source);
		MediaTrackReader* operator->() { return _pReader; }
		MediaTrackReader& operator*() { return *_pReader; }

		Media::Type	 type;
		bool		 waitHeader;
		UInt8		 sequence;

	private:
		MediaTrackReader* _pReader;
	};

	UInt32  parse(Packet& buffer, Media::Source& source);

	void	parsePAT(const UInt8* data, UInt32 size, Media::Source& source);
	void	parsePSI(const UInt8* data, UInt32 size, UInt8& version, Media::Source& source);
	void	parsePMT(const UInt8* data, UInt32 size, UInt8& version, Media::Source& source);

	void	readESI(BinaryReader& reader, Program& program);
	void	readPESHeader(BinaryReader& reader, Program& pProgram);

	void    onFlush(Packet& buffer, Media::Source& source);

	std::map<UInt16, Program>					_programs;
	Media::Properties							_properties;
	Time										_timeProperties;
	UInt8										_audioTrack;
	UInt8										_videoTrack;
	std::map<UInt16, UInt8>						_pmts;
	UInt32										_crcPAT;
	bool										_syncFound;
	bool										_syncError;
	double										_startTime;
};


} // namespace Mona
