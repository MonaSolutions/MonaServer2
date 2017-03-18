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

Subscription::Subscription(Media::Target& target) : pPublication(NULL), _congested(0), pNextPublication(NULL), target(target),
	audios(_audios), videos(_videos), datas(_datas), _streaming(0),
	_firstTime(true), _seekTime(0), _timeout(-1),_ejected(EJECTED_NONE),
	_startTime(0),_lastTime(0) {
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
	const auto& it = _videos.find(track);
	if(it!=_videos.end())
		it->second.waitKeyFrame = true;
}

const string& Subscription::name() const {
	return pPublication ? pPublication->name() : typeof(target);
}

Subscription::EJECTED Subscription::ejected() const {
	if (_ejected)
		return _ejected;
	if (_streaming && _timeout > 0 && _streaming.isElapsed(_timeout)) {
		INFO(name(), " timeout, idle since ", _timeout / 1000, " seconds");
		((Subscription&)*this).endMedia();
	}
	return EJECTED_NONE;
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
			_timeout = -1;
		else if (String::ToNumber(*pValue, _timeout))
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
	_timeout = -1;
	_audios.enable();
	_videos.enable();
	_datas.enable();
}

void Subscription::beginMedia() {
	if (_streaming)
		endMedia();

	if (pPublication && !pPublication->publishing())
		return; // wait publication running to start subscription

	if (!target.beginMedia(name(), *this)) {
		_ejected = EJECTED_ERROR;
		return;
	}
	_streaming.update();

	if (!pPublication) {
		if(*this)
			writeProperties(*this);
		return;
	}

	if(pPublication->count())
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
	_ejected = EJECTED_NONE; // Reset => maybe the next publication will solve ejection
	if (!_streaming)
		return;

	target.endMedia(name());
	target.flush();

	_streaming = 0;
	_seekTime = _lastTime;
	_audios.clear();
	_videos.clear();
	_datas.clear();
	_firstTime = true;
	_startTime = 0;
	_congested = 0;
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
	} else
		_streaming.update();
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
	else
		_streaming.update();
	if (_ejected)
		return;
	if (!_datas.enabled(track))
		return;

	// Create track!
	_datas[track];

	if (congested()) {
		if (_datas.reliable || _congestion(target.queueing(), Net::RTO_MAX)) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Data timeout, insufficient bandwidth to play ", name());
			return;
		}
		// if data is unreliable, drop the packet
		++_datas.dropped;
		WARN("Data packet dropped, insufficient bandwidth to play ", name());
		return;
	}

	if(!target.writeData(track, type, packet, _datas.reliable))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	else
		_streaming.update();
	if (_ejected)
		return;
	if (!_audios.enabled(track) && !tag.isConfig)
		return;

	// Create track!
	_audios[track];

	if (congested()) {
		if (_audios.reliable || _congestion(target.queueing(), Net::RTO_MAX)) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Audio timeout, insufficient bandwidth to play ", name());
			return;
		}
		if (!tag.isConfig) {
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
	else
		_streaming.update();
	if (_ejected)
		return;
	bool isConfig(tag.frame == Media::Video::FRAME_CONFIG);
	if (!_videos.enabled(track) && !isConfig) {
		const auto& it = _videos.find(track);
		if(it!=_videos.end())
			it->second.waitKeyFrame = true;
		return;
	}

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

	if (congested()) {
		if (_videos.reliable || _congestion(target.queueing(), Net::RTO_MAX)) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Video timeout, insufficient bandwidth to play ", name());
			return;
		}
		if(!isConfig) {
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
	TRACE("Video time => ", video.time, "\t", String::Hex(packet.data(), 5));
	if (!target.writeVideo(track, fixTag(isConfig, tag, video), packet, isConfig || _videos.reliable))
		_ejected = EJECTED_ERROR;
}

bool Subscription::congested() {
	if (!_congested)
		_congested = 0xF0 | (_congestion(target.queueing()) ? 1 : 0);
	return _congested & 1;
}

void Subscription::flush() {
	if (_ejected)
		return;
	target.flush();
	_congested = 0;
}


} // namespace Mona
