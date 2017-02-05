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
#include "Mona/Media.h"
#include "Mona/Logs.h"
#include "Mona/Congestion.h"

namespace Mona {


/*!
Subscription to Publication */
struct Publication;
struct Subscription : Media::Source, Media::Properties, virtual Object {
	enum EJECTED {
		EJECTED_NONE = 0,
		EJECTED_BANDWITDH,
		EJECTED_TIMEOUT,
		EJECTED_ERROR
	};

	template<typename TrackType>
	struct Tracks : std::map<UInt16, TrackType> {
		Tracks() : reliable(true), dropped(0), _enabled(1) {}
		bool	reliable;
		UInt32 	dropped;

		bool enabled(UInt16 track) const { return !_enabled ? false : (_enabled>0 || _track == track); }
		void enable() const { _enabled = 1; } // enable all tracks!
		void enable(UInt16 track) const { _enabled = -1; _track = track; }
		void disable() const { _enabled = 0; }
		void clear() { std::map<UInt16, TrackType>::clear(); dropped = 0; }
	private:
		mutable UInt16 _track;
		mutable Int8   _enabled;
	};

	struct Track : virtual Object {};
	struct VideoTrack : Track, virtual Object {
		VideoTrack() : waitKeyFrame(true) {}
		bool waitKeyFrame;
	};

	Subscription(Media::Target& target, bool timeoutOnEnd=true);
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
	const UInt32 timeout() const { return _timeout; }
	/*!
	Allow to reset the reference live timestamp */
	void seek(UInt32 time);

	bool streaming() const { return _streaming; }
	void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet);
	void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet);
	void writeData(UInt16 track, Media::Data::Type type, const Packet& packet);
	void reportLost(Media::Type type, UInt32 lost);
	void reportLost(Media::Type type, UInt16 track, UInt32 lost);
	void flush();
	void reset() { endMedia(); }

	void writeProperties(const Media::Properties& properties);

private:
	void beginMedia();
	void endMedia();

	/*!
	For Subscription usage as Source! */
	void writeProperties(UInt16 track, DataReader& reader);

	bool congested();

	template<typename TagType>
	TagType& fixTag(bool isConfig, const TagType& tagIn, TagType& tagOut) {
		if (_firstTime && !isConfig) {
			_firstTime = false;
			_startTime = tagIn.time;
		}
		if (isConfig)
			tagOut.time = _lastTime;
		else if (_firstTime) {
			_firstTime = false;
			_startTime = tagIn.time;
			_lastTime = _seekTime;
		} else if (_startTime > tagIn.time) {
			tagOut.time = _seekTime;
			WARN(typeid(TagType) == typeid(Media::Audio::Tag) ? "Audio" : "Video", " time too late on ", name());
		} else
			_lastTime = tagOut.time = tagIn.time - _startTime + _seekTime;
		tagOut.codec = tagIn.codec;
		return tagOut;
	}

	// Parameters overrides
	void onParamChange(const std::string& key, const std::string* pValue);
	void onParamClear();

	Tracks<Track>			_audios;
	Tracks<VideoTrack>		_videos;
	Tracks<Track>			_datas;

	UInt32					_seekTime;
	bool					_firstTime;
	UInt32					_startTime;
	UInt32					_lastTime;

	bool					_timeoutOnEnd;
	Time					_idleSince;
	bool					_streaming;

	UInt8					_congested;
	Congestion				_congestion;

	UInt32					_timeout; // for congestion and publication without data
	mutable EJECTED			_ejected;
};


} // namespace Mona
