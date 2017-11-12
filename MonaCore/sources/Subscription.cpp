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

Subscription::Subscription(Media::Target& target) : pPublication(NULL), _congested(0), pNextPublication(NULL), _target(target), _ejected(EJECTED_NONE),
	_flushable(0), audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _seekTime(0), _timeout(0), _startTime(0), _lastTime(0),
	_audios(true), _videos(true), _datas(true),
	_waitingFirstVideoSync(0), _onMediaWrite([this](const Packet& packet) {
		writeToTarget(_datas, 0, Media::Data::TYPE_MEDIA, packet);
	}) {
}

Subscription::Subscription(Media::TrackTarget& target) : pPublication(NULL), _congested(0), pNextPublication(NULL), _target(target), _ejected(EJECTED_NONE),
	_flushable(0), audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _seekTime(0), _timeout(0), _startTime(0), _lastTime(0),
	_audios(false), _videos(false), _datas(false),
	_waitingFirstVideoSync(0), _onMediaWrite([this](const Packet& packet) {
		writeToTarget(_datas, 0, Media::Data::TYPE_MEDIA, packet);
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
			video.waitKeyFrame = 1;
		return;
	}
	if (track <= _videos.size())
		_videos[track].waitKeyFrame = 1;
}

const string& Subscription::name() const {
	return pPublication ? pPublication->name() : typeof(_target);
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
		_audios.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("videoReliable")) == 0)
		_videos.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, EXPAND("dataReliable")) == 0)
		_datas.reliable = pValue && String::IsFalse(*pValue) ? false : true;
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
		// audio/video/data = true => receives everything!
		// audio/video/data = false or 0 => receives just track 0!
		// audio/video/data = # => receives track #!
		if (String::ICompare(key, EXPAND("data")) == 0) {
			UInt8 track = 0;
			if (!pValue || (!String::ToNumber(*pValue, track) && !String::IsFalse(*pValue)))
				_datas.pSelection.reset();
			else
				_datas.pSelection.reset(new UInt8(track));
		} else if (String::ICompare(key, EXPAND("audio")) == 0)
			setMediaSelection(pPublication ? &pPublication->audios : NULL, pValue, _audios);
		else if (String::ICompare(key, EXPAND("video")) == 0)
			setMediaSelection(pPublication ? &pPublication->videos : NULL, pValue, _videos);
	}
	Media::Properties::onParamChange(key, pValue);
	if(_streaming)
		_target.setMediaParams(self);

	DEBUG(name(), " subscription parameters ", self);
}
void Subscription::onParamClear() {
	_audios.reliable = _videos.reliable = _datas.reliable = true;
	_timeout = 0;
	setMediaSelection(pPublication ? &pPublication->audios : NULL, NULL, _audios);
	setMediaSelection(pPublication ? &pPublication->videos : NULL, NULL, _videos);
	_datas.pSelection.reset();
	setFormat(NULL);
	Media::Properties::onParamClear();
	if (_streaming)
		_target.setMediaParams(*this);
}

void Subscription::beginMedia() {
	if (_streaming)
		endMedia();

	if (pPublication && !pPublication->publishing())
		return; // wait publication running to start subscription

	_target.setMediaParams(*this); // begin with media params!
	
	if (!_target.beginMedia(name())) {
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
	_target.endMedia(name());
	_target.flush();

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
	if(_target.writeProperties(properties)) {
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
	if (!_datas.selected(track))
		return;

	// Create track!
	if(track)
		_datas[track];

	if (congested()) {
		if (_datas.reliable || _congestion(_target.queueing(), Net::RTO_MAX)) {
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
	else if(!writeToTarget(_datas, track, type, packet))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	else
		_streaming.update();
	if (_ejected)
		return;
	if (!_audios.selected(track))
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
		if (_audios.reliable || _congestion(_target.queueing(), Net::RTO_MAX)) {
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
	else if(!writeToTarget(_audios, track, audio, packet, tag.isConfig))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) {
	if (!_streaming)
		beginMedia();
	else
		_streaming.update();
	if (_ejected)
		return;
	
	if (!_videos.selected(track)) {
		if (track && track <= _videos.size())
			_videos[track].waitKeyFrame = 1;
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
			if (pVideo && pVideo->waitKeyFrame) {
				DEBUG("Video key frame gotten from ", name());
				pVideo->waitKeyFrame = 0;
			}
		} else if(pVideo && pVideo->waitKeyFrame) {
			++_videos.dropped;
			if (pVideo->waitKeyFrame > 1)
				return;
			pVideo->waitKeyFrame = 2;
			DEBUG("Video key frame waiting from ", name());
			return;
		}
	}

	if (congested()) {
		if (_videos.reliable || _congestion(_target.queueing(), Net::RTO_MAX)) {
			_ejected = EJECTED_BANDWITDH;
			WARN("Video timeout, insufficient bandwidth to play ", name());
			return;
		}
		if(!isConfig) {
			// if it's not config packet and video is unreliable, drop the packet
			++_videos.dropped;
			WARN("Video frame dropped, insufficient bandwidth to play ", name());
			if(pVideo)
				pVideo->waitKeyFrame = 1;
			return;
		}
	}

	Media::Video::Tag video;
	video.compositionOffset = tag.compositionOffset;
	video.frame = tag.frame;
	fixTag(isConfig, tag, video);
	if (_pMediaWriter)
		_pMediaWriter->writeVideo(track, video, packet, _onMediaWrite);
	else if (!writeToTarget(_videos, track, video, packet, isConfig))
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
		_congested = 0xF0 | (_congestion(_target.queueing()) ? 1 : 0);
	return _congested & 1;
}

void Subscription::flush() {
	if (_ejected)
		return;
	_flushable = 0;
	_target.flush();
	// update congestion status
	_congested = 0;
	_congestion(_target.queueing());
}


} // namespace Mona
