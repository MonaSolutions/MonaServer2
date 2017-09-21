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
#include "Mona/Subscription.h"
#include "Mona/ByteRate.h"
#include "Mona/LostRate.h"
#include "Mona/MediaFile.h"
#include "Mona/CCaption.h"
#include <set>

namespace Mona {

struct Publication : Media::Source, Media::Properties, virtual NullableObject {
	typedef Event<void(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet)>	ON(Audio);
	typedef Event<void(UInt8 track, const Media::Video::Tag& tag, const Packet& packet)>	ON(Video);
	typedef Event<void(UInt8 track, Media::Data::Type type, const Packet& packet)>			ON(Data);
	typedef Event<void()>																	ON(Flush);
	typedef Event<void(const Media::Properties&)>											ON(Properties);
	typedef Event<void(UInt8 track)>														ON(KeyFrame);
	typedef Event<void()>																	ON(End);


	template<typename TrackType>
	struct Tracks : std::deque<TrackType>, virtual Object {
		Tracks() : lostRate(byteRate) {}
		ByteRate byteRate;
		LostRate lostRate;
		void clear() { std::deque<TrackType>::clear(); byteRate = 0; lostRate = 0; }
		TrackType& operator[](UInt8 track) { if (track > std::deque<TrackType>::size()) std::deque<TrackType>::resize(track); return std::deque<TrackType>::operator[](track - 1); }
		const TrackType& operator[](UInt8 track) const { return std::deque<TrackType>::operator[](track - 1); }
	};

	struct Track : virtual Object {};
	struct DataTrack : Track, virtual Object {
		DataTrack() : textLang(NULL) {}
		const char* textLang;
	};
	struct AudioTrack : Track, virtual Object {
		AudioTrack() : lang(NULL) {}
		Media::Audio::Config	config;
		const char*				lang;
	};
	struct VideoTrack : Track, virtual Object {
		VideoTrack() : keyFrameTime(0), keyFrameInterval(0), waitKeyFrame(true) {}
		Media::Video::Config	config;
		CCaption				cc;
		bool					waitKeyFrame;
		UInt32					keyFrameTime;
		UInt32					keyFrameInterval;
	};


	Publication(const std::string& name);
	virtual ~Publication();

	explicit operator bool() const { return _publishing || !subscriptions.empty(); }

	const std::string&				name() const { return _name; }

	const Tracks<AudioTrack>&		audios;
	const Tracks<VideoTrack>&		videos;
	const Tracks<DataTrack>&		datas;

	UInt16							latency() const { return _latency; }
	UInt64							byteRate() const { return _byteRate; }
	double							lostRate() const { return _lostRate; }

	const std::set<Subscription*>	subscriptions;

	void							start(MediaFile::Writer* pRecorder=NULL, bool append = false);
	void							reset();
	bool							publishing() const { return _publishing; }
	void							stop();

	MediaFile::Writer*				recorder();
	bool							recording() const { return _pRecording && ((MediaFile::Writer&)_pRecording->target).running(); }

	void							reportLost(UInt32 lost) { reportLost(Media::TYPE_NONE, lost); }
	void							reportLost(Media::Type type, UInt32 lost, UInt8 track = 0);

/*! 
	Push audio packet, an empty audio "isConfig" packet is required by some protocol to signal "audio end".
	Good practice would be to send an audio empty "isConfig" packet for publishers which can stop "dynamically" just audio track.
	/!\ Audio timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void							writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { writeAudio(track, tag, packet); }
/*!
	Push video packet
	Video timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void							writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { writeVideo(track, tag, packet); }
/*!
	Push data packet */
	void							writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) { writeData(track, type, packet); }
	

	void							flush();
	void							flush(UInt16 ping);

private:
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet);
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet);
	void flushProperties();

	void startRecording(MediaFile::Writer& recorder, bool append);
	void stopRecording();

	void setProperties(UInt8 track, DataReader& reader) { Properties::setProperties(track, reader); }
	void onParamChange(const std::string& key, const std::string* pValue);
	void onParamClear();

	ByteRate						_byteRate;
	LostRate						_lostRate;

	Tracks<AudioTrack>				_audios;
	Tracks<VideoTrack>				_videos;
	Tracks<DataTrack>				_datas;

	UInt16							_latency;

	bool							_publishing;
	Time							_waitingFirstVideoSync;
	std::string						_name;

	bool							_new;
	bool							_newLost;
	bool							_newProperties;

	std::unique_ptr<Subscription>   _pRecording;
};


} // namespace Mona
