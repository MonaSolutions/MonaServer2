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

/*!
/!\ Don't support mp4 file with mdat before moov, used -movflags faststart with ffmpeg to avoid it (or a fragmented mp4 file)*/
struct MP4Reader : virtual Object, MediaReader {
	// http://atomicparsley.sourceforge.net/mpeg-4files.html
	// https://www.adobe.com/content/dam/Adobe/en/devnet/flv/pdfs/video_file_format_spec_v10.pdf
	// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html
	// https://w3c.github.io/media-source/isobmff-byte-stream-format.html
	// With fragments box = > http://l.web.umkc.edu/lizhu/teaching/2016sp.video-communication/ref/mp4.pdf
	MP4Reader() : _boxes(1), _position(0), _failed(false), _offset(0), _videos(0), _audios(0), _datas(0), _firstMoov(true), _sequence(0) {}

private:
	UInt32  parse(Packet& buffer, Media::Source& source);
	void	onFlush(Packet& buffer, Media::Source& source);

	UInt32  parseData(const Packet& packet, Media::Source& source);
	void	flushMedias(Media::Source& source);

	struct Box : virtual Object {
		Box() { operator=(nullptr); }
		const char* name() const { return _size ? _name : "undefined"; }

		UInt32 code() const { return _size ? FOURCC(_name[0], _name[1], _name[2], _name[3]) : 0; }
		operator UInt32() const { return _rest; }
		UInt32 contentSize() const { return _size-8; }
		UInt32 size() const { return _size; }
		Box& operator=(BinaryReader& reader); // assign _name, _rest and _size
		Box& operator=(std::nullptr_t) { _size = _rest = 0; return self; }
		Box& operator-=(UInt32 readen);
	private:
		char	_name[4];
		UInt32	_size;
		UInt32  _rest;
	};


	struct Fragment : virtual Object {
		Fragment() : typeIndex(1), defaultDuration(0), defaultSize(0) {}
		UInt32 typeIndex;
		UInt32 defaultDuration;
		UInt32 defaultSize;
	};
	struct Repeat : virtual Object {
		Repeat(UInt32 count=1) : count(count ? count : 1) {}
		UInt32 count;
		UInt32 value;
	};
	struct Durations : std::deque<Repeat>, virtual Object {
		Durations() : time(0) {}
		double time;
	};
	struct Track : Fragment, virtual Object {
		struct Type : virtual Object {
			Type() : _type(Media::TYPE_NONE) {}
			Type(Media::Audio::Codec codec) : _type(Media::TYPE_AUDIO), audio(codec) {}
			Type(Media::Video::Codec codec) : _type(Media::TYPE_VIDEO), video(codec) {}
			Type(Media::Data::Type type) : _type(Media::TYPE_DATA), data(type) {}
			~Type() {}
			operator Media::Type() const { return _type; }
			Type& operator=(std::nullptr_t) { _type = Media::TYPE_NONE; return *this; }
			Packet config;
			union {
				Media::Audio::Tag audio;
				Media::Video::Tag video;
				Media::Data::Tag data;
			};
		private:
			Media::Type _type;
		};

		Track() : _track(0), size(0), sample(0), samples(0), chunk(0), time(0), timeStep(0), pType(NULL) { lang[0] = 0; }

		operator UInt8() const { return _track; }
		Track& operator=(UInt8 track) { _track = track; return *this; }

		void writeProperties(Media::Properties& properties);

		char						lang[4]; // if lang[0]==0 => undefined!

		double						time;
		double						timeStep;
		Type*						pType;
		std::deque<Type>			types;

		// Following attributes have to be reseted on every fragment (moof)
		UInt32						chunk;
		UInt32						sample;
		UInt32						samples;
		
		std::map<UInt32, UInt64>	changes; // stsc => key = first chunk, value = 4 bytes for "sample count" and 4 bytes for "type index"
		UInt32						size;
		std::vector<UInt32>			sizes;   // stsz
		Durations					durations; // stts
		std::deque<Repeat>			compositionOffsets; // ctts

	private:
		UInt8 _track;
	};

	template <class VideoType>
	void	frameToMedias(Track& track, UInt32 time, const Packet& packet);

	UInt32															_sequence;
	UInt32															_position;
	UInt64															_offset;
	bool															_failed;
	UInt8															_audios;
	UInt8															_videos;
	UInt8															_datas;

	std::map<UInt64, Track*>										_chunks; // stco
	std::deque<Box>													_boxes;
	std::deque<Track>												_tracks;
	std::map<UInt32,Track*>											_ids;
	std::map<UInt32, UInt32>										_times;
	std::multimap<UInt32, std::pair<Track*, unique<Media::Base>>>	_medias;

	Track*															_pTrack;
	Fragment														_fragment;
	bool															_firstMoov;
};


} // namespace Mona
