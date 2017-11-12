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
		Tracks(bool multiTracks) : multiTracks(multiTracks), dropped(0), reliable(true) {}
		
		bool			reliable;
		UInt32			dropped;
		bool			multiTracks;
		unique<UInt8>	pSelection;
		bool			selected(UInt8 track) const { return pSelection ? (*pSelection == track) : (multiTracks || !track); }

		TrackType&		 operator[](UInt8 track) { if (track > std::deque<TrackType>::size()) std::deque<TrackType>::resize(track); return std::deque<TrackType>::operator[](track - 1); }
		const TrackType& operator[](UInt8 track) const { return std::deque<TrackType>::operator[](track - 1); }
		void			 clear() { std::deque<TrackType>::clear(); dropped = 0; }
	};
	template<typename TrackType>
	struct MediaTracks : Tracks<TrackType> {
		MediaTracks(bool multiTracks) : Tracks<TrackType>(multiTracks) {}
		bool selected(UInt8 track) const { return Tracks<TrackType>::pSelection ? (*Tracks<TrackType>::pSelection == track) : (Tracks<TrackType>::multiTracks || track <= 1); }
	};

	struct Track : virtual Object {};
	struct VideoTrack : Track, virtual Object {
		VideoTrack() : waitKeyFrame(1) {}
		UInt8 waitKeyFrame;  // 0 = no wait, 1 = wait, 2 = wait logged
	};

	/*!
	Create subscription */
	Subscription(Media::Target& target);
	Subscription(Media::TrackTarget& target);
	~Subscription();

	/*!
	Subscription ejected (insufficient bandwidth, timeout, target error, ...) */
	EJECTED				ejected() const;
	
	
	const MediaTracks<Track>&		audios;
	const MediaTracks<VideoTrack>&	videos;
	const Tracks<Track>&			datas;

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

	template<typename TargetType>
	TargetType& target() const { return (TargetType&)_target; }
private:
	void beginMedia();
	void endMedia();

	bool congested();

	void setFormat(const char* format);

	// Properties overrides
	void setProperties(UInt8 track, DataReader& reader) { Properties::setProperties(track, reader); }
	void onParamChange(const std::string& key, const std::string* pValue);
	void onParamClear();


	template<typename TagType>
	TagType& fixTag(bool isConfig, const TagType& tagIn, TagType& tagOut) {
		if (isConfig)
			tagOut.time = _lastTime;
		else if (_firstTime) {
			_firstTime = false;
			_startTime = tagIn.time;
			_lastTime = tagOut.time = _seekTime;
		} else if (_startTime > tagIn.time) {
			tagOut.time = _seekTime; // give tagOut.time on startTime!
			WARN(typeid(TagType) == typeid(Media::Audio::Tag) ? "Audio" : "Video", " time too late on ", name());
		} else
			_lastTime = tagOut.time = tagIn.time - _startTime + _seekTime;
		tagOut.codec = tagIn.codec;
		return tagOut;
	}

	template<typename TracksType, typename TagType>
	bool writeToTarget(const TracksType& tracks, UInt8 track, const TagType& tag, const Packet& packet, bool isConfig = false) {
		if (!_target.writeMedia(track, tag, packet, tracks.reliable || isConfig))
			return false;
		if (_flushable >= Net::MTU_RELIABLE_SIZE) {
			// not call flush() for not update congestion now!
			_flushable = 0;
			_target.flush();
		}
		_flushable += packet.size();
		return true;
	}

	template<typename TracksType1, typename TracksType2>
	void setMediaSelection(const TracksType1* pPublicationTracks, const std::string* pValue, TracksType2& tracks) {
		UInt8 track = 0;
		if (!pValue || (!String::ToNumber(*pValue, track) && !String::IsFalse(*pValue))) {
			if (!tracks.pSelection)
				return; // no change!
			tracks.pSelection.reset();
		} else {
			if (*tracks.pSelection == track)
				return; // no change!
			tracks.pSelection.reset(new UInt8(track));
			if (!track)
				return; // track disabled, following is useless
		}
		if (!pPublicationTracks || !_streaming || _ejected)
			return; // following is useless if no streaming or ejected
		// change => redistribute config packets
		track = 1;
		for (const auto& media : *pPublicationTracks) {
			if (!media.config)
				continue;
			writeMedia(UInt8(track), media.config, media.config);
			if (_ejected)
				return;
			++track;
		}
	}

	Media::Target&			_target;
	UInt32					_flushable;

	MediaTracks<Track>		_audios;
	MediaTracks<VideoTrack>	_videos;
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
	unique<MediaWriter>		_pMediaWriter;
};


} // namespace Mona
