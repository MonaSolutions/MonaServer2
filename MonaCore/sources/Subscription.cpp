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

#include "Mona/Subscription.h"
#include "Mona/Publication.h"
#include "Mona/MapWriter.h"
#include "Mona/Util.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

Subscription::Subscription(Media::Target& target, bool timeoutOnEnd) : pPublication(NULL), pNextPublication(NULL), target(target),
	audios(_audios), videos(_videos), datas(_datas), _streaming(false), _timeoutOnEnd(timeoutOnEnd),
	_firstTime(true), _seekTime(0), _timeout(7000),_ejected(EJECTED_NONE),
	_startTime(0),_lastTime(0) {
	if (!_timeoutOnEnd)
		_idleSince = 0;
}

Subscription::~Subscription() {
	// No endMedia() call here! stop "means" @unpublishing, it's not the case here! (publication may be always running)
	if (pPublication)
		CRITIC("Unsafe ", pPublication->name(), " subscription deletion");
	if (pNextPublication)
		CRITIC("Unsafe ", pNextPublication->name(), " subscription deletion");
}

void Subscription::reportLost(Media::Type type, UInt32 lost) {
	if (!lost || (type && type!= Media::TYPE_VIDEO)) return;
	for (auto& it : _videos)
		it.second.waitKeyFrame = true;
}
void Subscription::reportLost(Media::Type type, UInt16 track, UInt32 lost) {
	if (!lost || (type && type != Media::TYPE_VIDEO)) return;
	auto& it = _videos.find(track);
	if(it!=_videos.end())
		it->second.waitKeyFrame = true;
}

const string& Subscription::name() const {
	return pPublication ? pPublication->name() : typeof(target);
}

Subscription::EJECTED Subscription::ejected() const {
	if (_idleSince && _timeout && !_ejected && _idleSince.isElapsed(_timeout)) {
		INFO(name(), " subscription timeout, idle since ", _timeout/1000, " seconds");
		_ejected = EJECTED_TIMEOUT;
	}
	return _ejected;
}

template<typename TracksType>
static void Enable(TracksType& tracks, const string* pValue) {
	if(pValue) {
		UInt16 track;
		if (String::ToNumber(*pValue, track))
			return tracks.enable(track);
		if (String::IsFalse(*pValue))
			return tracks.disable();
	}
	tracks.enable();
}

void Subscription::onParamChange(const string& key, const string* pValue) {
	if (String::ICompare(key, EXPAND("audioReliable")) == 0)
		_audios.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("videoReliable")) == 0)
		_videos.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("dataReliable")) == 0)
		_datas.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("timeout")) == 0) {
		if (!pValue)
			_timeout = 7000;
		else if (String::ToNumber<UInt32>(*pValue, _timeout))
			_timeout *= 1000;
	} else if (String::ICompare(key, EXPAND("audioEnable")) == 0)
		Enable(_audios, pValue);
	else if (String::ICompare(key, EXPAND("videoEnable")) == 0)
		Enable(_videos, pValue);
	else if (String::ICompare(key, EXPAND("dataEnable")) == 0)
		Enable(_datas, pValue);
	Media::Properties::onParamChange(key, pValue);
}
void Subscription::onParamClear() {
	_audios.reliable = _videos.reliable = _datas.reliable = true;
	_timeout = 7000;
	_audios.enable();
	_videos.enable();
	_datas.enable();
}

void Subscription::beginMedia() {
	if (_streaming)
		endMedia();

	// update idleSince to timeout from subscription start
	_idleSince.update();
	// here can just be TIMEOUT, on start reset timeout!
	_ejected = EJECTED_NONE;
	
	if (pPublication && !pPublication->running())
		return; // wait publication running to start subscription

	if (!target.beginMedia(name(), *this)) {
		_ejected = EJECTED_ERROR;
		return;
	}
	_streaming = true;

	if (!pPublication) {
		if(*this)
			writeProperties(*this);
		return;
	}

	if(*pPublication)
		writeProperties(*pPublication);

	// Send codecs settings on start! Time is 0 (like following first packet)
	UInt16 track(0);
	for (const auto& it : pPublication->audios) {
		if (it.second.config) {
			if(!target.writeAudio(track, it.second.config, it.second.config, true)) {
				_ejected = EJECTED_ERROR;
				return;
			}
		}
		++track;
	}
	track = 0;
	for (const auto& it : pPublication->videos) {
		if (it.second.config) {
			if (!target.writeVideo(track, it.second.config, it.second.config, true)) {
				_ejected = EJECTED_ERROR;
				return;
			}
		}
		++track;
	}
}

void Subscription::endMedia() {
	if (!_timeoutOnEnd)
		_idleSince = 0;
	_ejected = EJECTED_NONE; // Reset => maybe the next publication will solve ejection
	
	if (!_streaming)
		return;

	target.endMedia(name());
	target.flush();

	_streaming = false;
	_seekTime = _lastTime;
	_audios.clear();
	_videos.clear();
	_datas.clear();
	_firstTime = true;
	_startTime = 0;
}

void Subscription::seek(UInt32 time) {
	// Doesn't considerate "_ejected" because can be assinged on stop to change _seekTime
	// To force the time to be as requested, but the stream continue normally (not reseted _codecInfosSent and _firstMedia)
	_firstTime = true;
	_lastTime = _startTime = 0;
	_seekTime = time;
}

void Subscription::writeProperties(const Media::Properties& properties) {
	if (!_streaming) {
		beginMedia();
		if (pPublication == &properties)
			return; // already done in beginMedia!
	}
	if (_ejected)
		return;
	INFO("Properties sent to one ",name()," subscription")
	if(!target.writeProperties(properties))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeProperties(UInt16 track, DataReader& reader) {
	// Just for Subscription as Source
	MapWriter<Parameters> writer(*this);
	writer.beginObject();
	writer.writePropertyName(to_string(track).c_str());
	reader.read(writer);
	writer.endObject();
	writeProperties(*this);
}

void Subscription::writeData(UInt16 track, Media::Data::Type type, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	if (_ejected)
		return;
	if (!_datas.enabled(track))
		return;
	_idleSince.update();

	// Create track!
	_datas[track];

	UInt32 congested(target.congestion());
	if (congested) {
		if (_timeout && congested>_timeout) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Data timeout, insufficient bandwidth to play ", name());
			return;
		}
		if (!_datas.reliable) {
			 // if data is unreliable, drop the packet
			++_datas.dropped;
			WARN("Data packet dropped, insufficient bandwidth to play ",  name());
			return;
		}
	}

	if(!target.writeData(track, type, packet, _datas.reliable))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	if (_ejected)
		return;
	if (!_audios.enabled(track) && !tag.isConfig)
		return;
	_idleSince.update();

	// Create track!
	_audios[track];

	UInt32 congested(target.congestion());
	if (congested) {
		if (_timeout && congested>_timeout) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Audio timeout, insufficient bandwidth to play ", name());
			return;
		}
		if (!tag.isConfig && !_audios.reliable) {
			 // if it's not config packet and audio is unreliable, drop the packet
			++_audios.dropped;
			WARN("Audio packet dropped, insufficient bandwidth to play ", name());
			return;
		}
	}

	Media::Audio::Tag audio;
	audio.channels = tag.channels;
	audio.rate = tag.rate;
	audio.isConfig = tag.isConfig;
	if(!target.writeAudio(track, fixTag(tag.isConfig, tag, audio), packet, tag.isConfig || _audios.reliable))
		_ejected = EJECTED_ERROR;
	TRACE("Audio time => ", audio.time);
}

void Subscription::writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	if (_ejected)
		return;
	bool isConfig(tag.frame == Media::Video::FRAME_CONFIG);
	if (!_videos.enabled(track) && !isConfig) {
		auto& it = _videos.find(track);
		if(it!=_videos.end())
			it->second.waitKeyFrame = true;
		return;
	}
	_idleSince.update();


	// Create track!
	VideoTrack& videoTrack = _videos[track];

	if (!isConfig && videoTrack.waitKeyFrame) {
		if (tag.frame != Media::Video::FRAME_KEY) {
			++_videos.dropped;
			DEBUG("Video frame dropped, waits a key frame from ", name());
			return;
		}
		videoTrack.waitKeyFrame = false;
	}

	UInt32 congested(target.congestion());
	if (congested) {
		if (_timeout && congested>_timeout) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Video timeout, insufficient bandwidth to play ", name());
			return;
		}
		if (!isConfig && !_videos.reliable) {
			// if it's not config packet and video is unreliable, drop the packet
			++_videos.dropped;
			WARN("Video frame dropped, insufficient bandwidth to play ", name());
			videoTrack.waitKeyFrame = true;
			return;
		}
	}

	Media::Video::Tag video;
	video.compositionOffset = tag.compositionOffset;
	video.frame = tag.frame;
	TRACE("Video time => ", video.time, "\t", Util::FormatHex(packet.data(), 5, string()));
	if (!target.writeVideo(track, fixTag(isConfig, tag, video), packet, isConfig || _videos.reliable))
		_ejected = EJECTED_ERROR;
}


} // namespace Mona
