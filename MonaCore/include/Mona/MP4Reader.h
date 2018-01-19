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
	MP4Reader() : _boxes(1), _position(0), _failed(false), _offset(0), _videos(0), _audios(0), _firstMoov(true) {}

private:
	UInt32  parse(Packet& buffer, Media::Source& source);
	void	onFlush(Packet& buffer, Media::Source& source);

	UInt32  parseData(const Packet& packet, Media::Source& source);
	void	flushMedias(Media::Source& source);

	struct Box : virtual Object {
		enum Type {
			UNDEFINED = 0,

			MOOV,
			MOOF,
			MVEX,
			TRAK,
			TRAF,
			TREX,
			MDIA,
			MINF,
			STBL,
			DINF,
			EDTS,

			ELST,
			MDHD,
			TKHD,
			TFHD,
			STSC,
			ELNG,
			STSD,
			STSZ,
			STCO,
			CO64,
			STTS,
			CTTS,
			TRUN,

			MDAT
		};
		Box() { operator=(nullptr); }
		const char* name() {
			static const char* Names[] = {"UNDEFINED", "MOOV", "MOOF", "MVEX", "TRAK", "TRAF", "TREX", "MDIA", "MINF", "STBL", "DINF", "EDTS", "ELST", "MDHD", "TKHD", "TFHD", "STSC", "ELNG", "STSD", "STSZ", "STCO", "CO64", "STTS", "CTTS", "TRUN", "MDAT" };
			return Names[UInt8(_type)];
		}

		Type type() const { return _type; }
		operator UInt32() const { return _rest; }
		UInt32 size() const { return _size; }
		Box& operator=(BinaryReader& reader); // assign _type, _rest and _size
		Box& operator=(nullptr_t) { _type = UNDEFINED; _rest = 0; return *this; }
		Box& operator-=(UInt32 readen);
		Box& operator-=(BinaryReader& reader);
	private:
		Type	_type;
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
			~Type() {}
			operator Media::Type() const { return _type; }
			Type& operator=(nullptr_t) { _type = Media::TYPE_NONE; return *this; }
			Packet config;
			union {
				Media::Audio::Tag audio;
				Media::Video::Tag video;
			};
		private:
			Media::Type _type;
		};

		Track() : _id(0), size(0), sample(0), samples(0), chunk(0), time(0), timeStep(0), pType(NULL), propertiesFlushed(false) { lang[0] = 0; }

		operator UInt8() const { return _id; }
		Track& operator=(UInt8 id) { _id = id; return *this; }

		char						lang[3]; // if lang[0]==0 => undefined!
		bool						propertiesFlushed;

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
		UInt8 _id;
	};

	template <class VideoType>
	void	frameToMedias(const Packet& packet, UInt8 track, Track::Type& type, UInt32 time);

	UInt32								_position;
	UInt64								_offset;
	bool								_failed;
	UInt8								_audios;
	UInt8								_videos;

	std::map<UInt64, Track*>			_chunks; // stco
	std::deque<Box>						_boxes;
	std::deque<Track>					_tracks;
	std::map<UInt32,Track*>				_ids;
	std::map<UInt32, UInt32>			_times;
	std::multimap<UInt32, Media::Base*> _medias;

	Track*								_pTrack;
	Fragment							_fragment;
	bool								_firstMoov;
};


} // namespace Mona
