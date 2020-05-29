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

bool Subscription::MediaTrack::setLastTime(UInt32 time) {
	if (_started && Util::Distance(lastTime, time) < 0) {
		WARN("Non-monotonic ", typeid(self) == typeid(MediaTrack) ? "audio" : "video", " time ", time, ", packet ignored");
		return false;
	}
	(UInt32&)lastTime = time;
	return _started =true;
}

Subscription::Subscription(Media::Target& target) : pPublication(NULL), _pNextPublication(NULL), _target(target), _ejected(EJECTED_NONE),
	_flushable(0), audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _timeout(0), _startTime(0), _seekTime(0),
	_audios(true), _videos(true), _datas(true), _timeoutMBRUP(10000), _medias(self), _updating(0), _duration(0) {
}

Subscription::Subscription(Media::TrackTarget& target) : pPublication(NULL), _pNextPublication(NULL), _target(target), _ejected(EJECTED_NONE),
	_flushable(0), audios(_audios), videos(_videos), datas(_datas), _streaming(0), _firstTime(true), _timeout(0), _startTime(0), _seekTime(0),
	_audios(false), _videos(false), _datas(false), _timeoutMBRUP(10000), _medias(self), _updating(0), _duration(0) {
}

Subscription::~Subscription() {
	// No reset() call here! stop "means" endMedia signal, it's not the case here! (publication may be always running)
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

UInt32 Subscription::currentTime() const {
	if (_videos.empty())
		return _audios.empty() ? _seekTime : _audios.lastTime;
	return Util::Distance(_audios.lastTime, _videos.lastTime)>0 ? _audios.lastTime : _videos.lastTime;
}
UInt32 Subscription::lastTime() const {
	if (_audios.empty())
		return _videos.empty() ? _seekTime : _videos.lastTime;
	return Util::Distance(_audios.lastTime, _videos.lastTime)>0 ? _videos.lastTime : _audios.lastTime;
}

Subscription::EJECTED Subscription::ejected() {
	if (_ejected)
		return next() ? ejected() : _ejected; // next publication will maybe resolve ejection cause, or must be done if _medias timeout!

	if (_pNextPublication && !_medias.synchronizing())
		next(); // timeout next!
	if (_streaming && _timeout && _streaming.isElapsed(_timeout)) {
		INFO(name(), " timeout, idle since ", _timeout / 1000, " seconds");
		reset(); // not an error, timeout = send the signal "end of publication"
	}
	return EJECTED_NONE;
}

void Subscription::onParamChange(const string& key, const string* pValue) {
	if (String::ICompare(key, "audioReliable") == 0)
		_audios.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, "videoReliable") == 0)
		_videos.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, "dataReliable") == 0)
		_datas.reliable = pValue && String::IsFalse(*pValue) ? false : true;
	else if (String::ICompare(key, "mbr") == 0) {
		if (pValue)
			String::Split(*pValue, "|", _streams);
		else
			_streams.clear();
	} else if (String::ICompare(key, "timeout") == 0) {
		_timeout = 0;
		if (pValue && String::ToNumber(*pValue, _timeout))
			_timeout *= 1000;
	} else if (String::ICompare(key, "time") == 0) {
		parseTime(pValue ? pValue->c_str() : NULL);
	} else if (String::ICompare(key, "from") == 0) {
		parseFromTime(pValue ? pValue->c_str() : NULL);
	} else if (String::ICompare(key, "duration") == 0) {
		_duration = 0;
		if (pValue && String::ToNumber(*pValue, _duration))
			insideDuration(currentTime()); // to reset if need!
	} else {
		// audio/video/data = true => receives everything!
		// audio/video/data = false or 0 => receives just track 0!
		// audio/video/data = # => receives track #!
		if (String::ICompare(key, "data") == 0) {
			UInt8 track = 0;
			if (!pValue || (!String::ToNumber(*pValue, track) && !String::IsFalse(*pValue)))
				_datas.pSelection.reset();
			else
				_datas.pSelection.set(track);
		} else if (String::ICompare(key, "audio") == 0)
			setMediaSelection(pPublication ? &pPublication->audios : NULL, pValue, _audios);
		else if (String::ICompare(key, "video") == 0)
			setMediaSelection(pPublication ? &pPublication->videos : NULL, pValue, _videos);
	}
	Media::Properties::onParamChange(key, pValue);
}
void Subscription::onParamClear() {
	parseFromTime(NULL);
	parseTime(NULL);
	_duration = 0;
	_audios.reliable = _videos.reliable = _datas.reliable = true;
	_timeout = 0;
	_streams.clear();
	setMediaSelection(pPublication ? &pPublication->audios : NULL, NULL, _audios);
	setMediaSelection(pPublication ? &pPublication->videos : NULL, NULL, _videos);
	_datas.pSelection.reset();
	Media::Properties::onParamClear();
}

bool Subscription::start() {
	if (_ejected)
		return false;
	if (_streaming) {
		// already streaming: just compute congestion + 
		if (_timeProperties<timeChanged()) {
			_timeProperties = timeChanged();
			DEBUG(name(), " subscription parameters ", self);
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

	if (!_target.beginMedia(name())) {
		_ejected = EJECTED_ERROR;
		return false;
	}

	if (_pMediaWriter && !_onMediaWrite) {
		_onMediaWrite = [this](const Packet& packet) {
			if(!writeToTarget(_datas, 0, Media::Data::TYPE_MEDIA, packet))
				_ejected = EJECTED_ERROR;
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

	if (_waitingFirstVideoSync) { // else not a first subscription
		// Just on first subcription and just if the publication has already begin to wait first key frame
		// If the publication reset, on next start it will begin with first key frame!
		if (pPublication->videos.size())
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
			writeAudio(audio.config, audio.config, track);
		if (_ejected)
			return false;
		
	}
	track = 0;
	for (const Publication::VideoTrack& video : pPublication->videos) {
		++track;
		if (video.config)
			writeVideo(video.config, video.config, track);
		if (_ejected)
			return false;
	}
	return true;
}
bool Subscription::start(UInt32 time) {
	if (_pFromTime) {
		Int32 distance = Util::Distance(*_pFromTime, time);
		if (distance < 0)
			return false;
		// after 10 seconds (max key frame interval) remove _pFromTime to allow cyclic (49 days) streaming!
		if (_firstTime && distance > 10000) // check _firstTime because source timeline can change while reseting
			_pFromTime.reset();
	}
	return insideDuration(time) && start();
}
bool Subscription::start(UInt8 track, Media::Data::Type type, const Packet& packet) {
	// In first flush medias previous media before to progress timeline (setLastTime)
	if (_pNextPublication && _medias.add(type, packet, track))
		return false;
	return start(lastTime());
}
bool Subscription::start(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) {
	if (tag.isConfig) {
		if (!_streaming && pPublication)
			return false; // ignored, will be sent on start
		return start(); // config packet has to be ignored by time progression and from/duration processing
	}
	// In first flush medias previous media before to progress timeline (setLastTime)
	if (_pNextPublication && _medias.add(tag, packet, track))
		return false;
	return _audios.setLastTime(track, tag.time) && start(tag.time);
}
bool Subscription::start(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) {
	if (tag.frame == Media::Video::FRAME_CONFIG) {
		if (!_streaming && pPublication)
			return false; // ignored, will be sent on start
		return start(); // config packet has to be ignored by time progression and from/duration processing
	}
	// In first flush medias previous media before to progress timeline (setLastTime)
	if (_pNextPublication && _medias.add(tag, packet, track))
		return false;
	return _videos.setLastTime(track, tag.time) && start(tag.time);
}

void Subscription::reset() {
	if(_ejected) // else is a publication reset (smooth publication transition, key frame should come in first)
		_waitingFirstVideoSync.update(); // to retablish audio/video sync!
	_ejected = EJECTED_NONE; // Reset => maybe the next publication will solve ejection
	if (!_streaming)
		return;
	if (next())
		return; // useless to wait sync on reset, next publication now!
	
	_timeoutMBRUP = 10000; // reset timeout MBRUP just when endMedia, to stay valid on MBR switch! (10 sec = max key frame interval possible)
	parseFromTime(getString("from")); // reset _pFromTime to its from value (cyclic algo)
	_seekTime = 0;
	clear(); 
	if (_pMediaWriter) {
		_pMediaWriter->endMedia(_onMediaWrite);
		_onMediaWrite = nullptr; // to call _pMediaWriter->begin just after reset (and not on MBR switch!)
	}
	if(_target.endMedia()) // keep in last to tolerate a this deletion, no need to eject here we are on a new media, can solve the target possible issue!
		_target.flush();
}

void Subscription::clear() {
	_firstTime = true;
	_startTime = 0; // todo working scaleTime on first config packet
	_streaming = 0;
	_updating = 0;
	// release resources
	_audios.clear();
	_videos.clear();
	_datas.clear();
}

void Subscription::release() {
	// must be like a new subscription creation beware not change pPublication (supervised by ServerAPI)
	reset();
	setNext(NULL);
	_medias.clear(); // clear invalid medias to empty _medias collection!
	_waitingFirstVideoSync.update(); // in last to not pertub flush of valid _medias in setNext(NULL)
}

bool Subscription::insideDuration(UInt32 time) {
	if (!_duration || _firstTime)
		return true;
	UInt32 duration = time - _startTime;
	if (duration < _duration)
		return true;
	// after 10 seconds (max key frame interval) reset (no more media would happen BEFORE toTime)
	if ((duration - _duration)>10000)
		reset();
	return false;
}

void Subscription::parseTime(const char* time) {
	if (_firstTime)
		return; // wait first time!
	if (!time)
		return; // no change on "time=" removing to keep progressive time 
	// time=
	/// a or absolute	=> time from source/publication
	/// +100			=> time from source + 100
	/// -100			=> time from source - 100
	/// r or relative	=> time relative to subscription
	/// 100				=> fix time now to 100 (if set on subscription start to 100)
	_seekTime = 0;
	switch (time[0]) {
		case '-':
			String::ToNumber(time + 1, _seekTime);
			_seekTime = _startTime - _seekTime;
			break;
		case '+':
			String::ToNumber(time + 1, _seekTime);
		case 'a': // or "absolute"
			_seekTime = _startTime + _seekTime;
			break;
		case 'r': // or "relative"
			break;
		default:  // set current time!
			if (String::ToNumber(time, _seekTime))
				_seekTime += currentTime();
	}
}
void Subscription::parseFromTime(const char* time) {
	UInt32 fromTime;
	if (time && String::ToNumber(time, fromTime)) {
		if (Util::Distance(lastTime(), _pFromTime.set(fromTime)) > 0)
			reset();
	} else
		_pFromTime.reset();
}


void Subscription::writeProperties(const Media::Properties& properties) {
	bool streaming = _streaming ? true : false;
	if (!start())
		return;
	if (!streaming && pPublication == &properties)
		return; // already done in beginMedia!

	DEBUG(name()," subscription properties sent to ", typeof(_target))
	if(_target.writeProperties(properties)) {
		if (_pMediaWriter)
			_pMediaWriter->writeProperties(properties, _onMediaWrite);
	} else
		_ejected = EJECTED_ERROR;
}

void Subscription::writeData(Media::Data::Type type, const Packet& packet, UInt8 track) {
	if (!start(track, type, packet) || !_datas.selected(track))
		return;

	if (track) {
		// Create track!
		_datas[track];
		// Sync audio/video/data subscription (data can be subtitle)
		if (_waitingFirstVideoSync) {
			if (!_waitingFirstVideoSync.isElapsed(1000)) {
				if (_medias.count() && _medias.add(type, packet, track)) // has audio, data has the same timestamp than prev audio packet!
					return; // wait 1 seconds of video silence (one video has always at less one frame by second!)
			} else
				_waitingFirstVideoSync = 0;
		}
	} // else pass in force! (audio track = 0)

	UInt32 congestion = _congestion();
	if (congestion) {
		if (_datas.reliable || congestion>=Net::RTO_MAX) {
			_ejected = EJECTED_BANDWITDH;
			WARN(typeof(_target), " data timeout, insufficient bandwidth to play ", name());
			return;
		}
		// if data is unreliable, drop the packet
		++_datas.dropped;
		WARN(typeof(_target), " data packet dropped, insufficient bandwidth to play ", name());
		return;
	}

	if (_pMediaWriter)
		_pMediaWriter->writeData(track, type, packet, _onMediaWrite);
	else if(!writeToTarget(_datas, track, type, packet))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track) {
	if (!start(track, tag, packet) || !_audios.selected(track))
		return;

	if (track) {
		// Create track!
		 _audios[track];
		// Sync audio/video/data subscription (data can be subtitle)
		if (_waitingFirstVideoSync) {
			if (!_waitingFirstVideoSync.isElapsed(1000)) {
				if(_medias.add(tag, packet, track))
					return; // wait 1 seconds of video silence (one video has always at less one frame by second!)
			} else
				_waitingFirstVideoSync = 0;
		}
	} // else pass in force! (audio track = 0)

	UInt32 congestion = _congestion();
	if (congestion) {
		if (_audios.reliable || congestion>=Net::RTO_MAX) {
			_ejected = EJECTED_BANDWITDH;
			WARN(typeof(_target), " audio timeout, insufficient bandwidth to play ", name());
			return;
		}
		if (!tag.isConfig) {
			// if it's not config packet and audio is unreliable, drop the packet
			++_audios.dropped;
			WARN(typeof(_target), " audio packet dropped, insufficient bandwidth to play ", name());
			return;
		}
	}

	Media::Audio::Tag audio;
	audio.channels = tag.channels;
	audio.rate = tag.rate;
	audio.isConfig = tag.isConfig;
	audio.codec = tag.codec;
	audio.time = scaleTime(tag.time, tag.isConfig);
	if (Logs::GetLevel() >= LOG_TRACE && pPublication && typeid(_target)!=typeid(Medias))
		TRACE(pPublication->name(), " audio time, ", tag.time, "=>", audio.time, tag.isConfig ? " (7)" : " (1)");

	if (_pMediaWriter)
		_pMediaWriter->writeAudio(track, audio, packet, _onMediaWrite);
	else if(!writeToTarget(_audios, track, audio, packet, tag.isConfig))
		_ejected = EJECTED_ERROR;
}

void Subscription::writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track) {
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
	if (packet && _waitingFirstVideoSync)
		_waitingFirstVideoSync.update(); // video is coming, wait more time

	bool isConfig = tag.frame == Media::Video::FRAME_CONFIG;
	if (!isConfig) {
		if (tag.frame == Media::Video::FRAME_KEY) {
			if (pVideo && pVideo->waitKeyFrame) {
				DEBUG("Video key frame gotten from ", name()," (", tag.time,")");
				pVideo->waitKeyFrame = 0;
			}
			_waitingFirstVideoSync = 0;
		} else if(pVideo && pVideo->waitKeyFrame) {
			_medias.clear(); // remove audio between two inter frames
			++_videos.dropped;
			if (pVideo->waitKeyFrame > 1)
				return;
			pVideo->waitKeyFrame = 2;
			DEBUG(typeof(_target), " video key frame waiting from ", name());
			return;
		}
	} else if (!packet) // special case of video config empty to keep alive a data stream input (SRT/VTT subtitle for example)
		return;

	UInt32 congestion = _congestion();
	if (congestion) {
		if (_videos.reliable || congestion >= Net::RTO_MAX) {
			_ejected = EJECTED_BANDWITDH;
			WARN(typeof(_target), " video timeout, insufficient bandwidth to play ", name());
			return;
		}
		if(!isConfig) {
			// if it's not config packet and video is unreliable, drop the packet
			++_videos.dropped;
			WARN(typeof(_target), " video frame dropped, insufficient bandwidth to play ", name());
			if(pVideo)
				pVideo->waitKeyFrame = 1;
			return;
		}
	}

	Media::Video::Tag video;
	video.compositionOffset = tag.compositionOffset;
	video.frame = tag.frame;
	video.codec = tag.codec;
	video.time = scaleTime(tag.time, isConfig);
	if (Logs::GetLevel() >= LOG_TRACE && pPublication && typeid(_target) != typeid(Medias))
		TRACE(pPublication->name(), " video time, ", tag.time, "=>", video.time, " (", video.frame, ")");

	if (_pMediaWriter)
		_pMediaWriter->writeVideo(track, video, packet, _onMediaWrite);
	else if (!writeToTarget(_videos, track, video, packet, isConfig))
		_ejected = EJECTED_ERROR;
}

UInt32 Subscription::scaleTime(UInt32 time, bool isConfig) {
	if (_firstTime) {
		if (!isConfig) {
			_firstTime = false;
			if (_seekTime) {
				// MBR SWITCH
				_startTime = time - _seekTime;  // to do working duration parameter => _seekTime(duration) = time - _startTime;			
				_seekTime = 0;
			} else {
				_startTime = time;
				parseTime(getString("time"));
			}
			_pFromTime.set(time); // accept just time after startTime now!
			_medias.clear(time);
			_medias.flush(self); // flush all has been buffered in waiting key frame and after _startTime
		} else
			time = 0; // wrong but allow to be played immediatly after publication rest (start to 0)
#if defined(_DEBUG)
	} else {
		// allow to debug a no monotonic time in debug, in release we have to accept "cyclic" time value (live of more than 49 days),
		// also for container like TS with AV offset it can be a SECOND packet just after the FIRST which can be just before, not an error
		if (_startTime > time)
			WARN("Media time ", time," inferior to start time ", _startTime, " on ", name());
#endif
	}
	return time - _startTime + _seekTime;
}

void Subscription::setFormat(const char* format) {
	if (!format && !_pMediaWriter)
		return;
	reset(); // end in first to finish the previous format streaming => new format = new stream
	_pMediaWriter = format ? MediaWriter::New(format) : nullptr;
	if (format && !_pMediaWriter)
		WARN(typeof(_target), " subscription format ", format, " unknown or unsupported");
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
		_pNextSubscription = new Subscription(self);
		_pNextSubscription->setString("time", "absolute");
	} else if(_pNextSubscription->pPublication) {
		if (pNextPublication == _pNextSubscription->pPublication)
			return; // no change!
		// unsubscribe prev
		_nextTimeout = 0;
		if (pNextPublication || _pNextSubscription->pPublication != _subscription.pPublication)
			resize(size() - _nextSize);  // remove next medias because publication has changed OR publication canceled!
		else
			clear();  // NEXT DONE => remove medias remaining BEFORE _nextSize BUt keep next medias unchanged (transform next medias in medias!)
		_nextSize = 0;

		// unsubscribe in last to have "_nextSize = 0" and like that no reset packet stacked!
		((set<Subscription*>&)_pNextSubscription->pPublication->subscriptions).erase(_pNextSubscription.get());
		// reinitialize subscription instead of recreate it (more faster on multiple MBR switch)
		_pNextSubscription->release();
	}
	_aligning = false;
	_pNextSubscription->pPublication = pNextPublication;
	if (!pNextPublication)
		return;
	UInt32 lastTime = _subscription.lastTime();
	// 22ms is the interval between 44000 and 48000 usual audio interval (and gives acceptable interval value for video)
	//_pNextSubscription->setNumber("time", UInt32(lastTime + 22));
	if(pNextPublication->publishing()) {
		UInt32 alignment = abs(Util::Distance(lastTime, pNextPublication->lastTime()));
		DEBUG(_subscription.name()," setNext ", pNextPublication->name()," with timestamp distance of ", alignment);
		if (alignment < 1000) {
			_aligning = true;
			_pNextSubscription->_pFromTime.set(lastTime + 1); // +1 = strictly positive (otherwise two frames can be overrided)
		} else
			WARN(pNextPublication->name(), " and ", _subscription.name(), " haven't an aligned timestamp (", alignment, "ms)")
	}
	if (_subscription.audios.pSelection)
		_pNextSubscription->_audios.pSelection.set(*_subscription.audios.pSelection);
	else
		_pNextSubscription->_audios.pSelection.reset();
	if (_subscription.videos.pSelection)
		_pNextSubscription->_videos.pSelection.set(*_subscription.videos.pSelection);
	else
		_pNextSubscription->_videos.pSelection.reset();
	if (_subscription.datas.pSelection)
		_pNextSubscription->_datas.pSelection.set(*_subscription.datas.pSelection);
	else
		_pNextSubscription->_datas.pSelection.reset();
	((set<Subscription*>&)pNextPublication->subscriptions).emplace(_pNextSubscription.get());
	_nextTimeout.update();
}

bool Subscription::Medias::add(unique<Media::Base>&& pMedia) {
	// play now if has no time (data or config packet) and when there is just next publication medias (or nothing)
	if (!pMedia->hasTime() && _nextSize >= size()) 
		return false; // can be played now
	// buffering
	emplace(begin() + size() - _nextSize, move(pMedia));
	if (_nextTimeout) // flush just if synchronizing!
		flush(_subscription);
	return true;
}
bool Subscription::Medias::writeMedia(unique<Media::Base>&& pMedia) {
	// must be sorted here to get good correct sync limit (medias resulting of _pNextSubscription and waitVideoKeySync)
	auto it = begin() + size();
	if(_nextSize && pMedia->hasTime()) {
		for (UInt32 i = 0; i < _nextSize; ++i) {
			if (Util::Distance((*--it)->time(), pMedia->time()) >= 0) {
				++it;
				break;
			}
		}
	}
	emplace(it, move(pMedia));
	if (!_nextSize++)
		_nextTimeout.update();// update timeout on first next frame to wait now just 1 second the alignment
	flush(_subscription);
	return true;
}

void Subscription::Medias::clear() {
	if (!_flushing)
		erase(begin(), begin() + size() - _nextSize);
}
void Subscription::Medias::clear(UInt32 untilTime) {
	if (_flushing)
		return;
	while (size() > _nextSize) {
		if (front()->hasTime() && Util::Distance(front()->time(), untilTime) <= 0)
			return;
		pop_front();
	}
}

void Subscription::Medias::flush(Media::Source& source) {
	if (_flushing)
		return;
	_flushing = true;
	// flush all _medias before time and before next (or future next)
	UInt32 limit;
	bool hasLimit = _aligning;
	if (hasLimit) {
		if (!_nextSize) {
			if (_pNextSubscription->pPublication->publishing())
				limit = _pNextSubscription->pPublication->currentTime();
			else
				hasLimit = false;
		} else
			limit = at(size() - _nextSize)->time();
	}
	while (size() > _nextSize) {
		if (hasLimit && front()->hasTime() && Util::Distance(front()->time(), limit) <= 0) {
			if (_nextSize) // means that the limit reached is the next publication media
				hasLimit = false;
			break;
		}
		if (front()->type)
			source.writeMedia(*front());
		else
			source.reset();
		pop_front();
	}
	_flushing = false; // set false BEFORE clear()!
	if (!_nextSize || hasLimit)
		return;
	// joined
	clear(); // clear the rest
	_nextTimeout = 0; // sync complete
	
}

bool Subscription::next() {
	if (!_pNextPublication) {
		// called on Subscription reset => clear possible media buffered if no next publication
		_medias.clear();
		return false;
	}
	Publication& publication = *_pNextPublication;
	_pNextPublication = NULL; // set NULL before flush (writeMedia) to avoid to call next() in infinite loop (can be call in start())

	// flush _medias before next
	_medias.flush(self);
	
	// onNext
	UInt32 lastTime = this->lastTime();
	DEBUG(name(), " at ", lastTime, " switch to ", publication.name(), " at ", publication.currentTime());
	onNext(publication);
	_medias.setNext(NULL); // remove next!
	if (!_ejected && _medias.count()) {
		/// Set next timestamp => 22ms is the interval between 44000 and 48000 usual audio interval (and gives acceptable interval value for video)
		_seekTime = max(scaleTime(lastTime) + 22, 1u); /// must be strictly positif to be detected in next scaleTime
		clear();
	} else // fix _ejected OR wait next publication starting! (no media gotten)
		reset();
	
	// flush next _medias!
	_medias.flush(self);
	return true;
}

Publication* Subscription::setNext(Publication* pPublication) {
	if (pPublication == _pNextPublication)
		return NULL; // next already running
	Publication* pOldNext = _pNextPublication;
	if (pPublication && pPublication != this->pPublication) {
		_pNextPublication = pPublication;
		if (_firstTime) {
			_medias.clear(); // remove possible buffered "waiting video sync" audio and data packet
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

} // namespace Mona
