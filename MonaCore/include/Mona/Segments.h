/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/MediaWriter.h"
#include "Mona/Playlist.h"

namespace Mona {


struct Segments : virtual Object, Media::Target, private MediaWriter {
	NULLABLE(!_maxSegments) // no real sense to use in writing/reading if _maxSegments==0

	enum : UInt8 {
		DEFAULT_SEGMENTS = 4
	};

	/*!
	Init segments and fill Playlist. Playlist properti */
	static bool Init(Exception& ex, IOFile& io, Playlist& playlist, bool append = false);
	/*!
	Clear files not include in playlist  */
	static Playlist& Erase(UInt32 count, Playlist& playlist, IOFile& io);
	
	struct Writer : MediaWriter, virtual Object {
		typedef Event<void(UInt16 duration)> ON(Segment);

		Writer(MediaWriter& writer, UInt8 sequences = 1) : _writer(writer), sequences(sequences) {}
	
		UInt8 sequences;
	
		const char*	format() const { return _writer.format(); }
		virtual MIME::Type	mime() const { return _writer.mime(); }
		virtual const char* subMime() const { return _writer.subMime(); }

		void beginMedia(const OnWrite& onWrite);
		void writeProperties(const Media::Properties& properties, const OnWrite& onWrite);
		void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
		void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
		void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) { _writer.writeData(track, type, packet, onWrite); }
		void endMedia(const OnWrite& onWrite);

	private:
		bool	newSegment(UInt32 time, const OnWrite& onWrite);
	
		MediaWriter&							_writer;
		unique<Media::Data>						_pProperties;
		std::map<UInt8, Media::Audio::Config>	_audioConfigs;
		std::map<UInt8, Media::Video::Config>	_videoConfigs;
		bool									_keying;
		UInt8									_sequences;

		bool									_first;
		UInt32									_segTime;
		UInt32									_lastTime;
	};

	/*!
	Not allow easly to configure "sequences" (setter are available for) because in live-memory the best is to have always 1 keyframe by sequence */
	Segments(UInt8 maxSegments = DEFAULT_SEGMENTS);

	UInt8&		sequences;
	
	UInt32		count() const { return _segments.size(); }

	const UInt8	maxSegments() const { return _maxSegments; }
	UInt8		setMaxSegments(UInt8 maxSegments);
	UInt32		maxDuration() const { return _maxDuration; }

	Segments&	operator=(std::nullptr_t) { setMaxSegments(0);  return self; }
	/*!
	Get segment by its sequence number, or by a relative end index if negative */
	const Segment& operator()(Int32 sequence) const;
	Playlist&	   to(Playlist& playlist) const;
	
	typedef std::deque<Segment>::const_iterator const_iterator;
	// iterate just on segments with duration information!
	const_iterator	begin() const { return _segments.begin(); }
	const_iterator	end() const { return _segments.end(); }

	// Methods to write Segments
	/*!
	beginMedia, can be called multiple time (MBR) */
	bool beginMedia(const std::string& name) override;
	bool writeProperties(const Media::Properties& properties) override { return write(properties, nullptr); }
	bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable = true) override { return write(track, tag, packet, nullptr); }
	bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable = true) override { return write(track, tag, packet, nullptr); }
	bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable = true) override { return write(track, type, packet, nullptr); }
	bool endMedia() override;

private:
	void flush() {} // private to mark it explicitly as useless

	template<typename ...Args>
	bool write(Args&&... args) {
		DEBUG_ASSERT(_started);
		if(!_started)
			return false; // Stream not begin!
		_writer.writeMedia(std::forward<Args>(args)...);
		return true;
	}

	// Method to write current segment
	void writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
		Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
		const Packet& packet = properties.data(type);
		DEBUG_CHECK(_segment.add<Media::Data>(type, packet, 0, true));
	}
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite) {
		DEBUG_CHECK(_segment.add<Media::Data>(type, packet, track));
	}
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
		DEBUG_CHECK(_segment.add<Media::Audio>(tag, packet, track));
	}
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
		DEBUG_CHECK(_segment.add<Media::Video>(tag, packet, track));
	}

	std::deque<Segment>	_segments;
	Segment				_segment;
	UInt32				_sequence;
	UInt8				_maxSegments;
	UInt32				_maxDuration;
	Writer				_writer;
	bool				_started;
};

} // namespace Mona

