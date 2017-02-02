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
#include <set>

namespace Mona {


struct Publication : Media::Source, Media::Properties, virtual Object {
	typedef Event<void(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet)>	ON(Audio);
	typedef Event<void(UInt16 track, const Media::Video::Tag& tag, const Packet& packet)>	ON(Video);
	typedef Event<void(UInt16 track, Media::Data::Type type, const Packet& packet)>			ON(Data);
	typedef Event<void()>																	ON(Flush);
	typedef Event<void(const Media::Properties&)>											ON(Properties);
	typedef Event<void(UInt16 track)>														ON(KeyFrame);
	typedef Event<void()>																	ON(End);


	template<typename TrackType>
	struct Tracks : std::map<UInt16,TrackType>, virtual Object {
		Tracks() : lostRate(byteRate) {}
		ByteRate byteRate;
		LostRate lostRate;
		void clear() { std::map<UInt16, TrackType>::clear(); byteRate = 0; lostRate = 0; }
	};

	struct Track : virtual Object { Track() {} };
	struct AudioTrack : Track, virtual Object {
		AudioTrack() {}
		Media::Audio::Config	config;
	};
	struct VideoTrack : Track, virtual Object {
		VideoTrack() {}
		Media::Video::Config	config;
		bool					waitKeyFrame;
	};


	Publication(const std::string& name);
	virtual ~Publication();

	const std::string&				name() const { return _name; }

	const Tracks<AudioTrack>&		audios;
	const Tracks<VideoTrack>&		videos;
	const Tracks<Track>&			datas;


	UInt16							latency() const { return _latency; }
	UInt64							byteRate() const { return _byteRate; }
	double							lostRate() const { _lostRate; }

	const std::set<Subscription*>	subscriptions;

	void							start(MediaFile::Writer* pRecorder=NULL, bool append = false);
	void							reset();
	const Time&						idleSince() const { return _idleSince; }
	bool							running() const { return _running; }
	void							stop();

	MediaFile::Writer*				recorder();
	bool							recording() const { return _pRecording && ((MediaFile::Writer&)_pRecording->target).running(); }

	void							reportLost(UInt32 lost);
	void							reportLost(Media::Type type, UInt32 lost);
	void							reportLost(Media::Type type, UInt16 track, UInt32 lost);

	void							writeProperties(UInt16 track, DataReader& reader) { writeProperties(track, reader, 0); }
	void							writeProperties(UInt16 track, DataReader& reader, UInt16 ping);

/*! 
	Push audio packet, an empty audio "isConfig" packet is required by some protocol to signal "audio end".
	Good practice would be to send an audio empty "isConfig" packet for publishers which can stop "dynamically" just audio track.
	/!\ Audio timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void							writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) { writeAudio(track, tag, packet, 0); }
	void							writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, UInt16 ping);
/*!
	Push video packet
	Video timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void							writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) { writeVideo(track, tag, packet, 0); }
	void							writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, UInt16 ping);
/*!
	Push data packet */
	void							writeData(UInt16 track, Media::Data::Type type, const Packet& packet) { writeData(track, type, packet, 0); }
	void							writeData(UInt16 track, Media::Data::Type type, const Packet& packet, UInt16 ping);

	void							flush();

private:
	void startRecording(MediaFile::Writer& recorder, bool append);
	void stopRecording();

	void onParamChange(const std::string& key, const std::string* pValue) { _newProperties = true; Media::Properties::onParamChange(key, pValue); }
	void onParamClear() { _newProperties = true; Media::Properties::onParamClear(); }

	ByteRate						_byteRate;
	LostRate						_lostRate;

	Tracks<AudioTrack>				_audios;
	Tracks<VideoTrack>				_videos;
	Tracks<Track>					_datas;

	UInt16							_latency;

	bool							_running;
	std::string						_name;
	Time							_idleSince;

	bool							_new;
	bool							_newLost;
	bool							_newProperties;

	std::unique_ptr<Subscription>    _pRecording;
};


} // namespace Mona
