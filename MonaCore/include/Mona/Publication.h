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

struct Publication : Media::Source, Media::Properties, virtual Object {
	NULLABLE(!_publishing && subscriptions.empty())

	template<typename TrackType>
	struct Tracks : std::deque<TrackType>, virtual Object {
		Tracks() : lostRate(byteRate) {}
		ByteRate byteRate;
		LostRate lostRate;
		void clear() { std::deque<TrackType>::clear(); byteRate = 0; lostRate = 0; }
		TrackType& operator[](UInt8 track) { if (track > std::deque<TrackType>::size()) std::deque<TrackType>::resize(track); return std::deque<TrackType>::operator[](track - 1); }
		const TrackType& operator[](UInt8 track) const { return std::deque<TrackType>::operator[](track - 1); }
	};
	template<typename TrackType>
	struct MediaTracks : Tracks<TrackType>, virtual Object {
		MediaTracks() : lastTime(0) {}
		UInt32		lastTime;
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
		VideoTrack() : waitKeyFrame(true) {}
		Media::Video::Config	config;
		CCaption				cc;
		bool					waitKeyFrame;
	};


	Publication(const std::string& name);
	virtual ~Publication();

	const std::string&				name() const { return _name; }

	const MediaTracks<AudioTrack>&	audios;
	const MediaTracks<VideoTrack>&  videos;
	const Tracks<DataTrack>&		datas;

	UInt16							latency() const { return _latency; }
	UInt64							byteRate() const { return _byteRate; }
	double							lostRate() const { return _lostRate; }
	UInt32							currentTime() const;
	UInt32							lastTime() const;

	const std::set<Subscription*>	subscriptions;

	void							start(unique<MediaFile::Writer>&& pRecorder = nullptr, bool append = false);
	void							reset();
	void							stop();
	bool							publishing() const { return _publishing ? true : false; }

	MediaFile::Writer*				recorder();
	bool							recording() const { return _pRecording && _pRecording->target<MediaFile::Writer>().state()>0; }

	void							reportLost(Media::Type type, UInt32 lost, UInt8 track = 0);

	/*! 
	Push audio packet, an empty audio "isConfig" packet is required by some protocol to signal "audio end".
	Good practice would be to send an audio empty "isConfig" packet for publishers which can stop "dynamically" just audio track.
	/!\ Audio timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void							writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1);
	/*!
	Push video packet, an empty video "config" packet can serve to keep alive a data stream (SRT/VTT subtitle stream for example)
	Video timestamp should be monotonic (>=), but intern code should try to ignore it and let's pass packet such given */
	void							writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1);
	/*!
	Push data packet */
	void							writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0);
	
	/*!
	Set properties, prefer the direct publication object access to change properties when done by final user */
	void							addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) { Properties::addProperties(track, type, packet); }
	UInt32							addProperties(UInt8 track, DataReader& reader) { return Properties::addProperties(track, reader); }

	void							flush();
	void							flush(UInt16 ping);

private:
	void flushProperties();

	void startRecording(unique<MediaFile::Writer>&& pRecorder, bool append);
	void stopRecording();

	// Media::Properties overrides
	void onParamChange(const std::string& key, const std::string* pValue);
	void onParamClear();

	ByteRate						_byteRate;
	LostRate						_lostRate;

	MediaTracks<AudioTrack>			_audios;
	MediaTracks<VideoTrack>			_videos;
	Tracks<DataTrack>				_datas;

	UInt16							_latency;

	Int8							_publishing;
	std::string						_name;

	bool							_new;
	bool							_newLost;
	Time							_timeProperties;

	unique<Subscription>			_pRecording;
};


} // namespace Mona
