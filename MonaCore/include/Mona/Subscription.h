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
	typedef Event<void(const std::set<std::string>& streams, bool down)>	ON(MBR);
	typedef Event<void(Publication& publication)>							ON(Next);

	enum EJECTED {
		EJECTED_NONE = 0,
		EJECTED_BANDWITDH,
		EJECTED_ERROR
	};

	struct Track : virtual Object {};
	struct MediaTrack : Track, virtual Object {
		MediaTrack() : _started(false), lastTime(0) {}
		const UInt32 lastTime;
		bool setLastTime(UInt32 time);
	private:
		bool _started;
	};
	struct VideoTrack : MediaTrack, virtual Object {
		VideoTrack() : waitKeyFrame(1) {}
		UInt8 waitKeyFrame;  // 0 = no wait, 1 = wait, 2 = wait logged
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
		MediaTracks(bool multiTracks) : Tracks<TrackType>(multiTracks), lastTime(0) {}
		bool selected(UInt8 track) const { return Tracks<TrackType>::pSelection ? (*Tracks<TrackType>::pSelection == track) : (Tracks<TrackType>::multiTracks || track <= 1); }
		const UInt32 lastTime;
		bool setLastTime(UInt8 track, UInt32 time) {
			if (track && !self[track].setLastTime(time))
				return false;
			(UInt32&)lastTime = time;
			return true;
		}
	};

	/*!
	Create subscription */
	Subscription(Media::Target& target);
	Subscription(Media::TrackTarget& target);
	~Subscription();

	/*!
	Subscription ejected (insufficient bandwidth, timeout, target error, ...) 
	Call periodically this method! */
	EJECTED				ejected();
	
	
	const MediaTracks<MediaTrack>&	audios;
	const MediaTracks<VideoTrack>&	videos;
	const Tracks<Track>&			datas;

	Publication*					pPublication;
	Publication*					setNext(Publication* publication);

	bool							subscribed(const std::string& stream) const;
	bool							subscribed(const Publication& publication) const { return pPublication == &publication || _pNextPublication == &publication; }
	const std::string&				name() const;

	UInt32							currentTime() const;
	UInt32							lastTime() const;

	void							setFormat(const char* format);

	const Time& streaming() const { return _streaming; }

	/*!
	Push audio packet, an empty audio "isConfig" packet is required by some protocol to signal "audio end".
	Good practice would be to send an audio empty "isConfig" packet for publishers which can stop "dynamically" just audio track.
	/!\ Audio timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1);
	/*!
	Push video packet, an empty video "config" packet can serve to keep alive a data stream (SRT/VTT subtitle stream for example)
	Video timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1);
	void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0);
	void writeProperties(const Media::Properties& properties);
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0);
	void flush();

	void reset();
	

	template<typename TargetType=Media::Target>
	TargetType& target() const { return (TargetType&)_target; }
private:
	void parseTime(const char* time);
	void parseFromTime(const char* time);
	bool insideDuration(UInt32 time);

	void clear(); // block father Parameters:clear call, and usefull in private!
	void release(); // reset the full subscription as if was just created

	// Media::Properties overrides
	void addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) { Properties::addProperties(track, type, packet); }
	void onParamChange(const std::string& key, const std::string* pValue);
	void onParamClear();

	bool start();
	bool start(UInt32 time);
	bool start(UInt8 track, Media::Data::Type type, const Packet& packet);
	bool start(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet);
	bool start(UInt8 track, const Media::Video::Tag& tag, const Packet& packet);

	bool next();

	UInt32 scaleTime(UInt32 time, bool isConfig = true);

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
			if (tracks.pSelection && *tracks.pSelection == track)
				return; // no change!
			tracks.pSelection.set(track);
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

	struct Medias : virtual Object, private Media::Target, private std::deque<unique<Media::Base>> {
		Medias(Subscription& subscription) : _aligning(false), _nextTimeout(0), _nextSize(0), _subscription(subscription), _flushing(false) {}

		UInt32 count() const { return std::deque<unique<Media::Base>>::size() - _nextSize; }

		
		void clear();
		void clear(UInt32 untilTime);
		void flush(Media::Source& source);
		void setNext(Publication* pPublication);

		bool synchronizing() const;
		
		bool add(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track) { return add<Media::Audio>(tag, packet, track); }
		bool add(const Media::Video::Tag& tag, const Packet& packet, UInt8 track) { return add<Media::Video>(tag, packet, track); }
		bool add(Media::Data::Type type, const Packet& packet, UInt8 track) { return add<Media::Data>(type, packet, track); }
	private:
		template<typename MediaType>
		bool add(const typename MediaType::Tag& tag, const Packet& packet, UInt8 track) {
			if (_flushing)
				return false; // return false to play it now!
			if (_nextSize && !_nextTimeout)
				return true; // already joined OR must be buffered as previous (no time progress, same behavior than prev media)
			return add(std::make_unique<MediaType>(tag, packet, track));;
		}
		bool add(unique<Media::Base>&& pMedia);


		bool beginMedia(const std::string& name) { return true; }
		bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
			return tag.isConfig && !_nextSize ? true : writeMedia(std::make_unique<Media::Audio>(tag, packet, track)); // first config packet is useless because will be given on switch
		}
		bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
			return tag.frame == Media::Video::FRAME_CONFIG && !_nextSize ? true : writeMedia(std::make_unique<Media::Video>(tag, packet, track)); // first config packet is useless because will be given on switch
		} 
		bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) {
			return _nextSize ? writeMedia(std::make_unique<Media::Data>(type, packet, track)) : true;  // ignore data packet as first to get a timestamp explicit on _nextSize
		}
		bool endMedia() { return _nextSize ? writeMedia(std::make_unique<Media::Base>()) : true; }
		// bool writeProperties(const Media::Properties& properties); ignore properties (metadata) during MBR switch, will be write on end of switch!
		bool writeMedia(unique<Media::Base>&& pMedia);
		void flush() {}; // to overrides Media::Target flush (remove a clang warning)

		Subscription&		 _subscription;
		unique<Subscription> _pNextSubscription;
		bool				 _aligning;
		Time				 _nextTimeout;
		UInt32				 _nextSize; // number of next media  at the end of collection
		bool				 _flushing;
	}	_medias;
	enum {
		MBR_NONE = 0,
		MBR_DOWN,
		MBR_UP
	}									_mbr;
	std::set<std::string>				_streams; // publication streams alternative = MBR!
	Publication*						_pNextPublication;


	Media::Target&			_target;
	UInt32					_flushable;
	Int8					_updating; // 0 = nothing (after flush), 1 = writen (before flush), -1 = MBR_UP, -2 MBR_DOWN

	MediaTracks<MediaTrack>	_audios;
	MediaTracks<VideoTrack>	_videos;
	Tracks<Track>			_datas;

	UInt32					_seekTime;
	bool					_firstTime;
	UInt32					_startTime;

	unique<UInt32>			_pFromTime;
	UInt32					_duration;

	Time					_streaming;
	Time					_timeProperties;

	// When the subscription starts on a publication running wit video we have to wait the first key video frame
	// We can bufferize in this case audio gotten between since the last interval frame
	Time					_waitingFirstVideoSync;

	Time					_queueing;
	Congestion				_congestion;
	UInt32					_timeoutMBRUP;

	UInt32					_timeout;
	mutable EJECTED			_ejected;

	// For "format" parameter
	MediaWriter::OnWrite	_onMediaWrite;
	unique<MediaWriter>		_pMediaWriter;
};


} // namespace Mona
