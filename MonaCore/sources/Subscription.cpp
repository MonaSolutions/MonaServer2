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

Subscription::Subscription(Media::Target& target) : pPublication(NULL), _pNextPublication(NULL), _target(target), _ejected(EJECTED_NONE),
	_flushable(0), audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _timeout(0), _startTime(0), _lastTime(0),
	_audios(true), _videos(true), _datas(true), _timeoutMBRUP(10000), _fromTime(0), _toTime(0xFFFFFFFF), _medias(self), _updating(0) {
}

Subscription::Subscription(Media::TrackTarget& target) : pPublication(NULL), _pNextPublication(NULL), _target(target), _ejected(EJECTED_NONE),
	_flushable(0), audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _timeout(0), _startTime(0), _lastTime(0),
	_audios(false), _videos(false), _datas(false), _timeoutMBRUP(10000), _fromTime(0), _toTime(0xFFFFFFFF), _medias(self), _updating(0) {
}

Subscription::~Subscription() {
	// No reset() call here! stop "means" @end, it's not the case here! (publication may be always running)
	if (pPublication)
		CRITIC("Unsafe ", pPublication->name(), " subscription deletion");
	if (_pNextPublication)
		CRITIC("Unsafe ", _pNextPublication->name(), " subscription deletion");
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

bool Subscription::subscribed(const std::string& stream) const {
	return (pPublication && pPublication->name() == stream) || (_pNextPublication && _pNextPublication->name() == stream);
}

Subscription::EJECTED Subscription::ejected() {
	if (_ejected) {
		if (!_pNextPublication)
			return _ejected;
		next(); // next publication will maybe resolve ejection cause, or must be done if _medias timeout!
		return ejected();
	}
	if (_pNextPublication && !_medias.synchronizing())
		next(); // timeout next!
	if (_streaming && _timeout && _streaming.isElapsed(_timeout)) {
		INFO(name(), " timeout, idle since ", _timeout / 1000, " seconds");
		reset(); // not an error, timeout = send the signal "end of publication"
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
	else if (String::ICompare(key, EXPAND("mbr")) == 0) {
		if (pValue)
			String::Split(*pValue, "|", _streams);
		else
			_streams.clear();
	} else if (String::ICompare(key, EXPAND("timeout")) == 0) {
		_timeout = 0;
		if (pValue && String::ToNumber(*pValue, _timeout))
			_timeout *= 1000;
	} else if (String::ICompare(key, EXPAND("time")) == 0) {
		if (pValue) // else not change on remove to keep time progressive
			setTime(pValue->c_str());
	} else if (String::ICompare(key, EXPAND("from")) == 0) {
		_fromTime = 0;
		if (pValue && String::ToNumber(*pValue, _fromTime) && (_lastTime < _fromTime))
			reset();
	} else if (String::ICompare(key, EXPAND("to")) == 0) {
		_toTime = 0xFFFFFFFF;
		if (pValue && String::ToNumber(*pValue, _toTime) && (_toTime > _lastTime))
			reset();
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
}
void Subscription::onParamClear() {
	_fromTime = 0;
	_toTime = 0xFFFFFFFF;
	_audios.reliable = _videos.reliable = _datas.reliable = true;
	_timeout = 0;
	_streams.clear();
	setMediaSelection(pPublication ? &pPublication->audios : NULL, NULL, _audios);
	setMediaSelection(pPublication ? &pPublication->videos : NULL, NULL, _videos);
	_datas.pSelection.reset();
	setFormat(NULL);
	// not change time on remove to keep time progressive
	Media::Properties::onParamClear();
}

bool Subscription::start() {
	if (_ejected)
		return false;
	if (_streaming) {
		if (_timeProperties<timeProperties()) {
			_timeProperties = timeProperties();
			DEBUG(name(), " subscription parameters ", self);
			_target.setMediaParams(self);
			if(!pPublication)
				writeProperties(self); // if no publication media params are also the media medatata
		}
		_streaming.update();

		if (_updating)
			return true;
		_updating = 1;
		// Compute congestion on first write to be more far away of the previous flush (work like that for file and network)
		_congestion = _target.queueing();
		if(_congestion(0)) {
			if (!_streams.empty() && _mbr != MBR_DOWN && (!_pNextPublication || (pPublication && _pNextPublication->byteRate() >= pPublication->byteRate()))) {
				_timeoutMBRUP *= 2; // increase MBR_UP attempt timemout (has been congested one time!)
				DEBUG("Subscription ", name(), " MBR DOWN");
				_mbr = MBR_DOWN;
				_updating = -2;
			} // else if pNextPublication and !pPublication, wait next publication!
			_queueing.update();
		} else if (!_queueing || _queueing.isElapsed(_timeoutMBRUP)) {
			_queueing = 0;
			if (!_streams.empty() && _mbr != MBR_UP && (!_pNextPublication || (pPublication && _pNextPublication->byteRate() < pPublication->byteRate()))) {
				DEBUG("Subscription ", name(), " MBR UP");
				_mbr = MBR_UP;
				_updating = -1;
			} // else if pNextPublication and !pPublication, wait next publication!
		}
		return true;
	}
	// reset congestion, will maybe change with this new media!
	_mbr = MBR_NONE;
	_congestion = 0;

	if (pPublication && !pPublication->publishing())
		return false; // wait publication running to start subscription

	_target.setMediaParams(self); // begin with media params!
	
	if (!_target.beginMedia(name())) {
		_ejected = EJECTED_ERROR;
		return false;
	}

	if (_pMediaWriter && !_onMediaWrite) {
		_onMediaWrite = [this](const Packet& packet) {
			writeToTarget(_datas, 0, Media::Data::TYPE_MEDIA, packet);
		};
		_pMediaWriter->beginMedia(_onMediaWrite);
	}

	// init time variables just after be sure that it will success!
	_streaming.update();
	_queueing.update();
	
	if (!pPublication) {
		_waitingFirstVideoSync = 0;
		if(this->count())
			writeProperties(self);
		return true;
	}

	if (_waitingFirstVideoSync) {
		// Just on first subcription and just if the publication has already begun to wait first key frame
		// If the publication reset, on next start it will begin with first key frame!
		if (pPublication->audios.lastTime || pPublication->videos.lastTime)
			_waitingFirstVideoSync.update();
		else
			_waitingFirstVideoSync = 0;
	}

	// write metadata
	if (pPublication->count())
		writeProperties(*pPublication);

	// Send codecs settings on start! Time is 0 (like following first packet)
	UInt8 track(0);
	for (const Publication::AudioTrack& audio : pPublication->audios) {
		++track;
		if (audio.config)
			writeAudio(track, audio.config, audio.config);
		if (_ejected)
			return false;
		
	}
	track = 0;
	for (const Publication::VideoTrack& video : pPublication->videos) {
		++track;
		if (video.config)
			writeVideo(track, video.config, video.config);
		if (_ejected)
			return false;
	}
	return true;
}

bool Subscription::start(UInt32 time) {
	if (time < _fromTime)  // no cycle compatible here, because _toTime marks a timevalue inferior to 49 days
		return false;
	if (time > _toTime) { // no cycle compatible here, because _toTime marks a timevalue inferior to 49 days
		reset();
		return false;
	}
	return start();
}

void Subscription::reset() {
	_ejected = EJECTED_NONE; // Reset => maybe the next publication will solve ejection
	if (!_streaming)
		return;
	if (_pNextPublication) {
		next(); // useless to wait sync on reset, next publication now!
		return;
	}
	if (_pMediaWriter) {
		_pMediaWriter->endMedia(_onMediaWrite);
		_onMediaWrite = nullptr; // to call _pMediaWriter->begin just after reset (and not on MBR switch!)
	}
	_target.endMedia();
	_target.flush();
	_firstTime = true;
	_lastTime = 0;
	_timeoutMBRUP = 10000; // reset timeout MBRUP just when endMedia, to stay valid on MBR switch! (10 sec = max key frame interval possible)
	stop();
}

void Subscription::clear() {
	// must be like a new subscription creation beware not change pPublication (supervised by ServerAPI)
	reset();
	_waitingFirstVideoSync.update();
	setNext(NULL);
	_medias.flush(Source::Null());
}

void Subscription::stop() {
	_streaming = 0;
	_updating = 0;
	// release resources
	_audios.clear();
	_videos.clear();
	_datas.clear();
}

void Subscription::setTime(const char* time) {
	if (!time)
		time = getString("time", String::Empty().c_str());
	UInt32 seek;
	switch (time[0]) {
		case '+':  // time relative to publication time (source time)
			String::ToNumber(time+1, seek=0);
			_seekTime = _startTime + seek;
			break;
		case '-': // time relative to publication time (source time)
		case 'a': // or "absolute"
			String::ToNumber(time+1, seek=0);
			_seekTime = seek>_startTime ? 0 : (_startTime-seek);
			break;
		case 'r': // or "relative" (pValue is not a number => _seekTime = 0)
			++time;
		default:
			// set current time!
			String::ToNumber(time, _seekTime = 0);
	}
}


void Subscription::writeProperties(const Media::Properties& properties) {
	bool streaming = _streaming ? true : false;
	if (!start())
		return;
	if (!streaming && pPublication == &properties)
		return; // already done in beginMedia!

	INFO("Properties sent to one ",name()," subscription")
	if(_target.writeProperties(properties)) {
		if (_pMediaWriter)
			_pMediaWriter->writeProperties(properties, _onMediaWrite);
	} else
		_ejected = EJECTED_ERROR;
}

void Subscription::writeData(UInt8 track, Media::Data::Type type, const Packet& packet) {
	if (!start(track, type, packet) || !_datas.selected(track))
		return;

	if (track) {
		// Create track!
		_datas[track];
		// Sync audio/video/data subscription (data can be subtitle)
		if (_waitingFirstVideoSync) {
			if (!_waitingFirstVideoSync.isElapsed(1000)) {
				if (_medias.count()) // has audio, data has the same timestamp than prev audio packet!
					_medias.add(track, type, packet);
				return; // wait 1 seconds of video silence (one video has always at less one frame by second!)
			}
			_waitingFirstVideoSync = 0;
			_medias.flush(Source::Null()); // impossible to recover prev audio (< to _lastTime or duration=0!)
		}
	} // else pass in force! (audio track = 0)

	if (_congestion) {
		if (_datas.reliable || _congestion(Net::RTO_MAX)) {
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
	bool progress = tag.time>_lastTime;
	if (!start(track, tag, packet) || !_audios.selected(track))
		return;

	if (track) {
		// Create track!
		 _audios[track];
		// Sync audio/video/data subscription (data can be subtitle)
		if (_waitingFirstVideoSync && !tag.isConfig) {
			if (progress)
				_medias.flush(Source::Null());
			if (!_waitingFirstVideoSync.isElapsed(1000)) {
				_medias.add(track, tag, packet);
				return; // wait 1 seconds of video silence (one video has always at less one frame by second!)
			}
			_waitingFirstVideoSync = 0;
			_medias.flush(Source::Null()); // impossible to recover prev audio (< to _lastTime or duration=0!)
		}
	} // else pass in force! (audio track = 0)
	
	if (_congestion) {
		if (_audios.reliable || _congestion(Net::RTO_MAX)) {
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
//	if (pPublication)
	//	DEBUG("Audio time => ", tag.time, "-", audio.time);
	if (_pMediaWriter)
		_pMediaWriter->writeAudio(track, audio, packet, _onMediaWrite);
	else if(!writeToTarget(_audios, track, audio, packet, tag.isConfig))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) {
	bool progress = tag.time>_lastTime;
	if (!start(track, tag, packet))
		return;

	if (!_videos.selected(track)) {
		if (track && track <= _videos.size())
			_videos[track].waitKeyFrame = 1;
		return;
	}

	// Create track!
	VideoTrack* pVideo = track ? &_videos[track] : NULL;

	// Sync audio/video/data subscription (data can be subtitle)
	if (_waitingFirstVideoSync)
		_waitingFirstVideoSync.update(); // video is coming, wait more time

	bool isConfig(tag.frame == Media::Video::FRAME_CONFIG);
	if (!isConfig) {
		if (tag.frame == Media::Video::FRAME_KEY) {
			if (pVideo && pVideo->waitKeyFrame) {
				DEBUG("Video key frame gotten from ", name()," (", tag.time,")");
				pVideo->waitKeyFrame = 0;
			}
			// flush medias buffered (audio and data with the same timestamp than this first video frame!)
			if (_waitingFirstVideoSync) {
				_waitingFirstVideoSync = 0;
				if (progress)
					_medias.flush(Source::Null());
				else
					_medias.flush(self);
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

	if (_congestion) {
		if (_videos.reliable || _congestion(Net::RTO_MAX)) {
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
//	if (pPublication)
//		DEBUG("Video time => ", video.time, " (", video.frame, ")");
	if (_pMediaWriter)
		_pMediaWriter->writeVideo(track, video, packet, _onMediaWrite);
	else if (!writeToTarget(_videos, track, video, packet, isConfig))
		_ejected = EJECTED_ERROR;
}

void Subscription::setFormat(const char* format) {
	if (!format && !_pMediaWriter)
		return;
	reset(); // end in first to finish the previous format streaming => new format = new stream
	_pMediaWriter.reset(format ? MediaWriter::New(format) : NULL);
	if (format && !_pMediaWriter)
		WARN("Subscription format ", format, " unknown or unsupported");
}

void Subscription::flush() {
	_flushable = 0;
	_target.flush(); // keep flush free even if ejected (usefull for example for ServerAPI::WaitingSync)
	if (_streaming) { // no "ejected" check because it's valid to switch when ejected (can potentialy solve the ejection)
		// Following code can modify subscriptions lists of publication and it's acceptable just in flush!
		if (_updating < 0)
			onMBR(_streams, _updating<-1);
		else if (_pNextPublication && !_medias.synchronizing()) // do the next in real time (acceptable just on flush because allow to modify pPublication->subscriptions list)
			next(); // timeout next!
	}
	_updating = 0;
}







//////////////////////////////////////////////////////////////////////////////
////////////////////////// NEXT /////////////////////////////////////////////

bool Subscription::Medias::synchronizing() const {
	// if is not ejected (timeout of 11 seconds to get next publication) and "timestamp alignement" waiting inferior to 1 sec (maximum interval configured in setNext)
	if (!_nextTimeout)
		return false; // always not wait update if no setNext!
	DEBUG(_subscription.name(), " and ", _pNextSubscription->pPublication->name(), " synchronizing elapsed (", _nextSize,") => ", _nextTimeout.elapsed());
	if (_nextSize)
		return !_nextTimeout.isElapsed(1000); // wait "timestamp alignement" => inferior to 1 sec (maximum interval configured in setNext)
	return !_nextTimeout.isElapsed(11000); // 10 max video key frame configurable + 1 max tolerable interval between prev and next
}

void Subscription::Medias::setNext(Publication* pNextPublication) {
	if (!_pNextSubscription) {
		if (!pNextPublication)
			return; // nothing to do, keep _medias unchanged
		_pNextSubscription.reset(new Subscription(self));
		_pNextSubscription->setString("time", "absolute");
	} else if(_pNextSubscription->pPublication) {
		if (pNextPublication == _pNextSubscription->pPublication)
			return; // no change!
		// unsubscribe prev
		_nextTimeout = 0;
		if (pNextPublication || _pNextSubscription->pPublication != _subscription.pPublication)
			resize(size() - _nextSize);  // remove next medias because publication has changed OR publication canceled!
		else
			erase(begin(), begin() + size() - _nextSize);  // NEXT DONE => remove medias remaining BEFORE _nextSize BUt keep next medias unchanged (transform next medias in medias!)
		_nextSize = 0;

		// unsubscribe in last to have "_nextSize = 0" and like that no reset packet stacked!
		((set<Subscription*>&)_pNextSubscription->pPublication->subscriptions).erase(_pNextSubscription.get());
		// reinitialize subscription instead of recreate it (more faster on multiple MBR switch)
		_pNextSubscription->clear();
	}
	_pNextSubscription->pPublication = pNextPublication;
	if (!pNextPublication)
		return;
	// use minimum lastTime of pNextPublication to compute timestamp aligment
	UInt32 alignment = abs(Util::Distance(
		Util::Distance(pNextPublication->audios.lastTime, pNextPublication->videos.lastTime) >= 0 ? pNextPublication->audios.lastTime : pNextPublication->videos.lastTime,
		_subscription._lastTime)
	);
	DEBUG(_subscription.name()," setNext ", pNextPublication->name()," with timestamp distance of ", alignment);
	if (alignment > 1000) {
		WARN(pNextPublication->name(), " and ", _subscription.name(), " haven't an aligned timestamp (", alignment, "ms)");
		_pNextSubscription->setNumber("time", _subscription._lastTime+22); // 22ms is the interval between 44000 and 48000 usual audio interval (and gives acceptable interval value for video)
	} else
		_pNextSubscription->setString("time", "absolute");
	_pNextSubscription->setNumber("from", _subscription._lastTime+1); // +1 = strictly positive (otherwise two frames can be overrided)
	if (_subscription.audios.pSelection)
		_pNextSubscription->_audios.pSelection.reset(new UInt8(*_subscription.audios.pSelection));
	else
		_pNextSubscription->_audios.pSelection.reset();
	if (_subscription.videos.pSelection)
		_pNextSubscription->_videos.pSelection.reset(new UInt8(*_subscription.videos.pSelection));
	else
		_pNextSubscription->_videos.pSelection.reset();
	if (_subscription.datas.pSelection)
		_pNextSubscription->_datas.pSelection.reset(new UInt8(*_subscription.datas.pSelection));
	else
		_pNextSubscription->_datas.pSelection.reset();
	((set<Subscription*>&)pNextPublication->subscriptions).emplace(_pNextSubscription.get());
	_nextTimeout.update();
}

template<typename MediaType>
bool Subscription::Medias::add(UInt32 time, UInt8 track, const typename MediaType::Tag& tag, const Packet& packet) {
	// Must return TRUE if buffered OR if hit next publication!
	if (_nextSize) {
		if (!_nextTimeout)
			return true; // already joined
		if (Util::Distance(time, at(size() - _nextSize)->time()) > 0) // time() is necessary on an audio or video media which is not a config packet (so media has valid time())
			return false; // can be play now!
		DEBUG(_subscription.name(), " sync with ", _pNextSubscription->name(), " (", time, ")");
		_nextTimeout = 0; // join! no more updating!
		return true;
	}
	if (_pNextSubscription && _pNextSubscription->pPublication) {
		UInt32 lastTime = typeid(MediaType) == typeid(Media::Audio) ? _pNextSubscription->pPublication->audios.lastTime : _pNextSubscription->pPublication->videos.lastTime;
		if (Util::Distance(time, lastTime) > 0)
			return false; // no join, can play now!
	}
	emplace(begin() + size() - _nextSize, new MediaType(tag, packet, track));
	return true;
}

bool Subscription::Medias::flush(Media::Source& source) {
	_flushing = true;
	// flush all _medias before time and before next (or future next)
	unique<UInt32> pLimit;
	if (!_nextSize && _pNextSubscription && _pNextSubscription->pPublication) {
		// take the minimum lastTime limit (audio or video!)
		if(Util::Distance(_pNextSubscription->pPublication->audios.lastTime, _pNextSubscription->pPublication->videos.lastTime)>=0)
			pLimit.reset(new UInt32(_pNextSubscription->pPublication->audios.lastTime));
		else
			pLimit.reset(new UInt32(_pNextSubscription->pPublication->videos.lastTime));
	}
	while (size() > _nextSize) {
		if (pLimit && front()->hasTime() && Util::Distance(front()->time(), *pLimit) <= 0)
			return _flushing = false; // rest to flush => false!
	//	DEBUG(_subscription.name(), &source == &Source::Null() ?  " medias remove " : " medias flush ", Media::TypeToString(front()->type), " - ", front()->time());
		if (front()->type)
			source.writeMedia(*front());
		else
			source.reset();
		pop_front();
	}
	_flushing = false;
	return true; // return true if all flushed and time parameter inferior to next time or possible future next time!
}

void Subscription::next() {
	if (!_pNextPublication)
		return;
	Publication& publication = *_pNextPublication;
	_pNextPublication = NULL; // set NULL before flush (writeMedia) to avoid to call next() in infinite loop (can be call in start())
	// flush _medias before next
	_medias.flush(self);
	// onNext
	if (_ejected)
		reset(); // to fix _ejected and reset timestamp!
	stop();
	onNext(publication);
	_medias.setNext(NULL); // remove next!
	// flush next _medias!
	_medias.flush(self);
}

Publication* Subscription::setNext(Publication* pPublication) {
	if (pPublication == _pNextPublication)
		return NULL; // next already running
	Publication* pOldNext = _pNextPublication;
	if (pPublication && pPublication != this->pPublication) {
		_pNextPublication = pPublication;
		if (!_streaming || _waitingFirstVideoSync) {
			_medias.flush(Source::Null()); // remove possible buffered "waiting video sync" audio and data packet
			next(); // always not started, so call "next" immediatly!
			return pOldNext;
		}
		// subscribe to the next publication
		_medias.setNext(pPublication);
	} else {
		_pNextPublication = NULL; // set NULL before flush (writeMedia) to avoid to call next() in infinite loop (can be call in start())
		_medias.setNext(NULL);
	}
	_medias.flush(self); // flush valid medias!
	return pOldNext;
}


template bool Subscription::Medias::add<Media::Audio>(UInt32 time, UInt8 track, const Media::Audio::Tag& tag, const Packet& packet);
template bool Subscription::Medias::add<Media::Video>(UInt32 time, UInt8 track, const Media::Video::Tag& tag, const Packet& packet);

} // namespace Mona
