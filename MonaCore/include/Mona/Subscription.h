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
#include "Mona/Congestion.h"
#include "Mona/MediaWriter.h"

namespace Mona {

/*!
Subscription to Publication
Params:
	audioReliable|videoReliable|dataReliable=true|false
	format=SubMime
	timeout=UInt32 (0 = no timeout)
	time=Int32 (set current time, if +Int32 or -Int32 it sets a time relative to source, and "time=source" let time of source unchanged)
	audio|video|data=false|0|UInt8|all|true (disable|disable|track selected|all selected|allselected)
	*/
struct Publication;
struct Subscription : Media::Source, Media::Properties, virtual Object {
	enum EJECTED {
		EJECTED_NONE = 0,
		EJECTED_BANDWITDH,
		EJECTED_ERROR
	};

	template<typename TrackType>
	struct Tracks : std::deque<TrackType> {
		Tracks() : dropped(0) {}
		UInt32 	dropped;
		void clear() { std::deque<TrackType>::clear(); dropped = 0; }
		TrackType& operator[](UInt8 track) { if (track > std::deque<TrackType>::size()) std::deque<TrackType>::resize(track); return std::deque<TrackType>::operator[](track - 1); }
		const TrackType& operator[](UInt8 track) const { return std::deque<TrackType>::operator[](track - 1); }
	};

	struct Track : virtual Object {};
	struct VideoTrack : Track, virtual Object {
		VideoTrack() : waitKeyFrame(true) {}
		bool waitKeyFrame;
	};

	/*!
	Create subscription */
	Subscription(Media::Target& target);
	~Subscription();

	/*!
	Subscription ejected (insufficient bandwidth, timeout, target error, ...) */
	EJECTED				ejected() const;
	
	Media::Target&				target;
	const Tracks<Track>&		audios;
	const Tracks<VideoTrack>&	videos;
	const Tracks<Track>&		datas;

	Publication*  pPublication;
	Publication*  pNextPublication;

	const std::string& name() const;

	bool streaming(UInt32 timeout) const { return _streaming ? !_streaming.isElapsed(timeout) : false; }
	const Time& streaming() const { return _streaming; }

	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet);
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet);
	void writeProperties(const Media::Properties& properties);
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0);
	void flush();
	void reset() { endMedia(); }

private:
	void beginMedia();
	void endMedia();

	void setAudioTrack(Int16 track);
	void setVideoTrack(Int16 track);

	bool congested();

	void setFormat(const char* format);

	template<typename TagType>
	TagType& fixTag(bool isConfig, const TagType& tagIn, TagType& tagOut) {
		if (isConfig)
			tagOut.time = _lastTime;
		else if (_firstTime) {
			_firstTime = false;
			_startTime = tagIn.time;
			_lastTime = tagOut.time = _seekTime;
		} else if (_startTime > tagIn.time) {
			tagOut.time = _seekTime;
			WARN(typeid(TagType) == typeid(Media::Audio::Tag) ? "Audio" : "Video", " time too late on ", name());
		} else
			_lastTime = tagOut.time = tagIn.time - _startTime + _seekTime;
		tagOut.codec = tagIn.codec;
		return tagOut;
	}

	// Properties overrides
	void setProperties(UInt8 track, DataReader& reader) { Properties::setProperties(track, reader); }
	void onParamChange(const std::string& key, const std::string* pValue);
	void onParamClear();

	Tracks<Track>			_audios;
	Tracks<VideoTrack>		_videos;
	Tracks<Track>			_datas;

	UInt32					_seekTime;
	bool					_firstTime;
	UInt32					_startTime;
	UInt32					_lastTime;

	Time					_streaming;
	Time					_waitingFirstVideoSync;

	UInt8					_congested;
	Congestion				_congestion;

	UInt32					_timeout;
	mutable EJECTED			_ejected;

	// For "format" parameter
	MediaWriter::OnWrite	_onMediaWrite;
	MediaWriter*			_pMediaWriter;
};


} // namespace Mona
