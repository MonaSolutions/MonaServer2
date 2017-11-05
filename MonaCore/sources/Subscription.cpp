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
#include "Mona/Util.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

Subscription::Subscription(Media::Target& target) : pPublication(NULL), _congested(0), pNextPublication(NULL), target(target), _ejected(EJECTED_NONE),
	audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _seekTime(0), _timeout(0), _startTime(0), _lastTime(0),
	_waitingFirstVideoSync(0), _onMediaWrite([&target](const Packet& packet) {
		target.writeData(0, Media::Data::TYPE_MEDIA, packet);
	}) {
}

Subscription::~Subscription() {
	// No endMedia() call here! stop "means" @unpublishing, it's not the case here! (publication may be always running)
	if (pPublication)
		CRITIC("Unsafe ", pPublication->name(), " subscription deletion");
	if (pNextPublication)
		CRITIC("Unsafe ", pNextPublication->name(), " subscription deletion");
}

void Subscription::reportLost(Media::Type type, UInt32 lost, UInt8 track) {
	if (!lost || (type && type!= Media::TYPE_VIDEO))
		return;
	if (!track) {
		for (VideoTrack& video : _videos)
			video.waitKeyFrame = true;
		return;
	}
	if (track <= _videos.size())
		_videos[track].waitKeyFrame = true;
}

const string& Subscription::name() const {
	return pPublication ? pPublication->name() : typeof(target);
}

Subscription::EJECTED Subscription::ejected() const {
	if (_ejected)
		return _ejected;
	if (_streaming && _timeout && _streaming.isElapsed(_timeout)) {
		INFO(name(), " timeout, idle since ", _timeout / 1000, " seconds");
		((Subscription&)*this).endMedia();
	}
	return EJECTED_NONE;
}

void Subscription::onParamChange(const string& key, const string* pValue) {
	if (String::ICompare(key, EXPAND("audioReliable")) == 0)
		target.audioReliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("videoReliable")) == 0)
		target.videoReliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("dataReliable")) == 0)
		target.dataReliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("format")) == 0)
		setFormat(pValue ? pValue->data() : NULL);
	else if (String::ICompare(key, EXPAND("timeout")) == 0) {
		_timeout = 0;
		if (pValue && String::ToNumber(*pValue, _timeout))
			_timeout *= 1000;
	} else if (String::ICompare(key, EXPAND("time")) == 0) {
		if (pValue) {
			switch (*pValue->c_str()) {
				case '-':
				case '+':
				case 'r': // to allow "time=real"
					// time relative to publication time (source time)
					_firstTime = false;
					/// _lastTime = tagIn.time - _startTime + _seekTime;
					/// tagIn.time = _lastTime + _startTime - _seekTime;
					_lastTime = _lastTime + _startTime - _seekTime;
					String::ToNumber(*pValue, _startTime = 0);
					_lastTime += _startTime;
					_seekTime = 0;
					break;
				default:
					// currentTime!
					_firstTime = true;
					String::ToNumber(*pValue, _seekTime = 0);
					_lastTime = _seekTime;
					_startTime = 0;
			}
		} // not change on remove to keep time progressive
	} else {
		Int16 track = -1;
		if (String::ICompare(key, EXPAND("audio")) == 0) {
			setAudioTrack((!pValue || String::ToNumber(*pValue, track) || !String::IsFalse(*pValue)) ? track : 0);
		} else if (String::ICompare(key, EXPAND("video")) == 0)
			setVideoTrack((!pValue || String::ToNumber(*pValue, track) || !String::IsFalse(*pValue)) ? track : 0);
		else if (String::ICompare(key, EXPAND("data")) == 0)
			target.dataTrack = (!pValue || String::ToNumber(*pValue, track) || !String::IsFalse(*pValue)) ? track : 0;
	}
	Media::Properties::onParamChange(key, pValue);
	if(_streaming)
		target.setMediaParams(*this);
}
void Subscription::onParamClear() {
	target.audioReliable = target.videoReliable = target.dataReliable = true;
	_timeout = 0;
	setAudioTrack(-1);
	setVideoTrack(-1);
	target.dataTrack = -1;
	setFormat(NULL);
	Media::Properties::onParamClear();
	if (_streaming)
		target.setMediaParams(*this);
}

void Subscription::setAudioTrack(Int16 track) {
	if (track == target.audioTrack)
		return;
	target.audioTrack = track;
	if (!track || !_streaming || _ejected)
		return;
	track = 1;
	for (const Publication::AudioTrack& audio : pPublication->audios) {
		if (!audio.config)
			continue;
		writeAudio(UInt8(track), audio.config, audio.config);
		if (_ejected)
			return;
		++track;
	}
}
void Subscription::setVideoTrack(Int16 track) {
	if (track == target.videoTrack)
		return;
	target.videoTrack = track;
	if (!track || !_streaming || _ejected)
		return;
	track = 1;
	for (const Publication::VideoTrack& video : pPublication->videos) {
		if (!video.config)
			continue;
		writeVideo(UInt8(track), video.config, video.config);
		if (_ejected)
			return;
		++track;
	}
}


void Subscription::beginMedia() {
	if (_streaming)
		endMedia();

	if (pPublication && !pPublication->publishing())
		return; // wait publication running to start subscription

	target.setMediaParams(*this); // begin with media params!
	
	if (!target.beginMedia(name())) {
		_ejected = EJECTED_ERROR;
		return;
	}
	if (_pMediaWriter)
		_pMediaWriter->beginMedia(_onMediaWrite);
	_waitingFirstVideoSync.update();
	_streaming.update();
	

	if (!pPublication) {
		if(this->count())
			writeProperties(*this);
		return;
	}

	if(pPublication->count())
		writeProperties(*pPublication);

	// Send codecs settings on start! Time is 0 (like following first packet)
	UInt8 track(1);
	for (const Publication::AudioTrack& audio : pPublication->audios) {
		if (!audio.config)
			continue;
		writeAudio(UInt8(track), audio.config, audio.config);
		if (_ejected)
			return;
		++track;
	}
	track = 1;
	for (const Publication::VideoTrack& video : pPublication->videos) {
		if (!video.config)
			continue;
		writeVideo(UInt8(track), video.config, video.config);
		if (_ejected)
			return;
		++track;
	}
}

void Subscription::endMedia() {
	_ejected = EJECTED_NONE; // Reset => maybe the next publication will solve ejection
	if (!_streaming)
		return;

	if(_pMediaWriter)
		_pMediaWriter->endMedia(_onMediaWrite);
	target.endMedia(name());
	target.flush();

	_waitingFirstVideoSync = 0;
	_streaming = 0;
	_seekTime = _lastTime;
	_audios.clear();
	_videos.clear();
	_datas.clear();
	_firstTime = true;
	_startTime = 0;
	_congested = 0;
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
	if(target.writeProperties(properties)) {
		if (_pMediaWriter)
			_pMediaWriter->writeProperties(properties, _onMediaWrite);
	} else
		_ejected = EJECTED_ERROR;
}

void Subscription::writeData(UInt8 track, Media::Data::Type type, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	else
		_streaming.update();
	if (_ejected)
		return;
	if (!target.dataSelected(track))
		return;

	// Create track!
	if(track)
		_datas[track];

	if (congested()) {
		if (target.dataReliable || _congestion(target.queueing(), Net::RTO_MAX)) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Data timeout, insufficient bandwidth to play ", name());
			return;
		}
		// if data is unreliable, drop the packet
		++_datas.dropped;
		WARN("Data packet dropped, insufficient bandwidth to play ", name());
		return;
	}

	if (_pMediaWriter)
		_pMediaWriter->writeData(track, type, packet, _onMediaWrite);
	else if(!target.writeData(track, type, packet))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	else
		_streaming.update();
	if (_ejected)
		return;
	if (!target.audioSelected(track))
		return;

	// Create track!
	if(track)
		_audios[track];

	if (_waitingFirstVideoSync && !tag.isConfig) {
		if (!_waitingFirstVideoSync.isElapsed(1000))
			return; // wait 1 seconds of video silence (one video has always at less one frame by second!)
		_waitingFirstVideoSync = 0;
	}

	if (congested()) {
		if (target.audioReliable || _congestion(target.queueing(), Net::RTO_MAX)) {
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
	fixTag(tag.isConfig, tag, audio);
	// DEBUG("Audio time => ", audio.time);
	if (_pMediaWriter)
		_pMediaWriter->writeAudio(track, audio, packet, _onMediaWrite);
	else if(!target.writeAudio(track, audio, packet))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	else
		_streaming.update();
	if (_ejected)
		return;
	
	if (!target.videoSelected(track)) {
		if (track && track <= _videos.size())
			_videos[track].waitKeyFrame = true;
		return;
	}

	// Create track!
	VideoTrack* pVideo = track ? &_videos[track] : NULL;
	if (_waitingFirstVideoSync)
		_waitingFirstVideoSync.update(); // video is coming, wait more time

	bool isConfig(tag.frame == Media::Video::FRAME_CONFIG);
	if (!isConfig) {
		if (tag.frame == Media::Video::FRAME_KEY) {
			_waitingFirstVideoSync = 0;
			if(pVideo)
				pVideo->waitKeyFrame = false;
		} else if(pVideo && pVideo->waitKeyFrame) {
			++_videos.dropped;
			DEBUG("Video frame dropped, waits a key frame from ", name());
			return;
		}
	}

	if (congested()) {
		if (target.videoTrack || _congestion(target.queueing(), Net::RTO_MAX)) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Video timeout, insufficient bandwidth to play ", name());
			return;
		}
		if(!isConfig) {
			// if it's not config packet and video is unreliable, drop the packet
			++_videos.dropped;
			WARN("Video frame dropped, insufficient bandwidth to play ", name());
			if(pVideo)
				pVideo->waitKeyFrame = true;
			return;
		}
	}

	Media::Video::Tag video;
	video.compositionOffset = tag.compositionOffset;
	video.frame = tag.frame;
	fixTag(isConfig, tag, video);
	if (_pMediaWriter)
		_pMediaWriter->writeVideo(track, video, packet, _onMediaWrite);
	else if (!target.writeVideo(track, video, packet))
		_ejected = EJECTED_ERROR;
	// DEBUG("Video time => ", video.time, " (", video.frame,")");
}

void Subscription::setFormat(const char* format) {
	if (!format && !_pMediaWriter)
		return;
	endMedia(); // end in first to finish the previous format streaming => new format = new stream
	_pMediaWriter.reset(format ? MediaWriter::New(format) : NULL);
	if (format && !_pMediaWriter)
		WARN("Subscription format ", format, " unknown or unsupported");
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
	// update congestion status
	_congested = 0;
	_congestion(target.queueing());
}


} // namespace Mona
