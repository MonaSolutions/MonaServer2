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

#include "Mona/Publication.h"
#include "Mona/Util.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

Publication::Publication(const string& name): _latency(0),
	audios(_audios), videos(_videos), datas(_datas), _lostRate(_byteRate),
	_publishing(0),_new(false), _newLost(false), _name(name) {
	DEBUG("New publication ",name);
}

Publication::~Publication() {
	stopRecording();
	// delete _listeners!
	if (!subscriptions.empty())
		CRITIC("Publication ", _name, " with subscribers is deleting");
	if (_publishing)
		ERROR("Publication ",_name," running is deleting"); // ERROR because can happen on server shutdown + server.publish (extern)
	DEBUG("Publication ",_name," deleted");
}

UInt32 Publication::currentTime() const {
	if (_videos.empty())
		return _audios.lastTime;
	return _audios.size() && Util::Distance(_audios.lastTime, _videos.lastTime)>0 ? _audios.lastTime : _videos.lastTime;
}
UInt32 Publication::lastTime() const {
	if (_audios.empty())
		return _videos.lastTime;
	return _videos.size() && Util::Distance(_audios.lastTime, _videos.lastTime)>0 ? _videos.lastTime : _audios.lastTime;
}

void Publication::reportLost(Media::Type type, UInt32 lost, UInt8 track) {
	if (!lost)
		return;
	switch (type) {
		case Media::TYPE_AUDIO: {
			if(track<=audios.size())
				_audios.lostRate += lost;
			break;
		}
		case Media::TYPE_VIDEO: {
			if (track > _videos.size())
				break;
			if(track)
				_videos[track].waitKeyFrame = true;
			_videos.lostRate += lost;
			break;
		}
		case Media::TYPE_DATA:
			if (track <= _datas.size())
				_datas.lostRate += lost;
			break;
		default:
			_audios.lostRate += UInt32(lost*_audios.byteRate / _byteRate);
			_videos.lostRate += UInt32(lost*_videos.byteRate / _byteRate);
			_datas.lostRate += UInt32(lost*_datas.byteRate / _byteRate);
	}
	_newLost = true;
	_lostRate += lost;
}


void Publication::startRecording(unique<MediaFile::Writer>&& pRecorder, bool append) {
	stopRecording();
	_pRecording.set(*pRecorder.release());
	NOTE("Start ", _name, "=>", pRecorder->path.name(), " recording");
	_pRecording->pPublication = this;
	_pRecording->setBoolean("append",append);
	((set<Subscription*>&)subscriptions).emplace(_pRecording.get());
	pRecorder->start(); // start MediaFile::Writer before subscription!
}

void Publication::stopRecording() {
	if (!_pRecording)
		return;
	NOTE("Stop ", _name, "=>", _pRecording->target<MediaFile::Writer>().path.name(), " recording");
	((set<Subscription*>&)subscriptions).erase(_pRecording.get());
	_pRecording->pPublication = NULL;
	delete &_pRecording->target<MediaFile::Writer>();
	_pRecording.reset();
}

MediaFile::Writer* Publication::recorder() {
	if (!_pRecording)
		return NULL;
	if (_pRecording->ejected())
		_pRecording->reset();// to reset ejected and allow a MediaFile::Writer::start() manual to retry recording!
	return &_pRecording->target<MediaFile::Writer>();
}

void Publication::start(unique<MediaFile::Writer>&& pRecorder, bool append) {
	if (!_publishing) {
		INFO("Publication ", _name, " started");
		_publishing = 1;
	} else if(_publishing>0) {
		// reset!
		_publishing = -1;
		_audios.clear();
		_videos.clear();
		_datas.clear();
		_latency = 0;
		_new = _newLost = false;

		// Erase track metadata just!
		clearTracks();

		auto it = subscriptions.begin();
		while (it != subscriptions.end()) { // using "while" rather "for each" because "reset" can remove an element of "subscriptions"!
			Subscription* pSubscription(*it++);
			if (pSubscription->pPublication != this && pSubscription->pPublication)
				continue; // subscriber not yet subscribed
			pSubscription->pPublication = this;
			pSubscription->reset(); // call writer.endMedia on subscriber side and do the flush!
		}
	}
	_timeProperties = timeChanged(); // useless to dispatch metadata changes, will be done on next media by Subscriber side!
	if(pRecorder)
		startRecording(move(pRecorder), append);
	// no flush here, wait first flush or first media to allow to subscribe to onProperties event for a publisher!
}

void Publication::reset() {
	if (_publishing<=0)
		return; // already reseted!
	INFO("Publication ", _name, " reseted");
	start();
}

void Publication::stop() {
	if (!_publishing)
		return;
	Media::Properties::clear();
	if (_publishing > 0)
		start(); // reset!
	stopRecording(); // after the reset to call the endWriter!
	_publishing = 0;
	INFO("Publication ", _name, " stopped");
}

void Publication::flush(UInt16 ping) {
	if(_publishing && ping)
		_latency = ping >> 1;
	flush();
}

void Publication::flush() {
	if (!_publishing) {
		ERROR("Publication flush called on publication ", _name, " stopped");
		return;
	}
	_publishing = 1;
	flushProperties(); // to send metadata if need!
	if (!_new)
		return;
	_new = false;

	if(_newLost) {
		double lost;
		if((lost = _audios.lostRate))
			INFO(String::Format<double>("%.2f",lost * 100),"% audio lost on publication ",_name);
		if((lost = _videos.lostRate))
			INFO(String::Format<double>("%.2f", lost * 100),"% video lost on publication ",_name);
		if((lost = _datas.lostRate))
			INFO(String::Format<double>("%.2f", lost * 100),"% data lost on publication ",_name);
		_newLost = false;
	}

	auto it = subscriptions.begin();
	while (it!=subscriptions.end()) { // using "while" rather "for each" because "flush" can remove an element of "subscriptions"!
		Subscription* pSubscription(*it++);
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->flush();
	}
}


void Publication::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track) {
	if (!_publishing) {
		ERROR("Audio packet on publication ", _name, " stopped");
		return;
	}
	_audios.lastTime = tag.time;

	// create track
	AudioTrack* pAudio = track ? &_audios[track] : NULL;

	// flush properties before to deliver media packet (to be sync with media content)
	flushProperties();

	// save audio codec packet for future listeners
	if (tag.isConfig)
		DEBUG("Audio configuration received on publication ", _name, " (size=", packet.size(), ")");
	_byteRate += packet.size() + sizeof(tag);
	_audios.byteRate += packet.size() + sizeof(tag);
	_new = true;
	//INFO(name()," audio ",tag.time);
	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->writeAudio(tag, packet, track);
	}

	// Hold config packet after video distribution to avoid to distribute two times config packet if subscription call beginMedia
	if (pAudio && tag.isConfig)
		pAudio->config.set(tag, packet);
}

void Publication::writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track) {
	if (!_publishing) {
		ERROR("Video packet on stopped ", _name, " publication");
		return;
	}
	_publishing = 1;
	_videos.lastTime = tag.time;

	// create track
	VideoTrack* pVideo = track ? &_videos[track] : NULL;

	UInt32 offsetCC = 0;

	if (tag.frame != Media::Video::FRAME_CONFIG) {
		if (pVideo) {
			if (pVideo->waitKeyFrame) {
				if (tag.frame != Media::Video::FRAME_KEY) {
					// wait one key frame (allow to rebuild the stream and saves bandwith without useless transfer
					INFO("Video frame dropped, ", _name, " waits a key frame");
					return;
				}
				pVideo->waitKeyFrame = false;
			}
			CCaption::OnText onText([this, track](UInt8 channel, shared<Buffer>& pBuffer) {
			//	DEBUG("cc", channel, " => ", *pBuffer);
				writeData(Media::Data::TYPE_TEXT, Packet(pBuffer), (track - 1) * 4 + channel);
			});
			CCaption::OnLang onLang([this, track](UInt8 channel, const char* lang) {
				// 1 <= channel <= 4
				if (lang) {
					DEBUG("Subtitle lang ", lang);
					setString(String((track - 1) * 4 + channel, ".textLang"), lang);
				} else
					erase(String((track - 1) * 4 + channel, ".textLang"));
			});
			offsetCC = pVideo->cc.extract(tag, packet, onText, onLang);
		}
	} else if(packet) // if empty it's config packet to maintain subtitle alive!
		DEBUG("Video configuration received on publication ", _name, " (size=", packet.size(), ")");


	// flush properties before to deliver media packet (to be sync with media content)
	flushProperties();

	_byteRate += packet.size() + sizeof(tag);
	_videos.byteRate += packet.size() + sizeof(tag);
	_new = true;
	//INFO(name(), " video ", tag.time, " (", tag.frame, ")");

	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication != this && pSubscription->pPublication)
			continue; // subscriber not yet subscribed
		if (offsetCC && (!pSubscription->datas.pSelection || *pSubscription->datas.pSelection)) { // if a data track is selected => send without CC!
			if (packet.size() > offsetCC)
				pSubscription->writeVideo(tag, packet + offsetCC, track); // without CC
		} else
			pSubscription->writeVideo(tag, packet, track); // with CC
	}

	// Hold config packet after video distribution to avoid to distribute two times config packet if subscription call beginMedia
	if (pVideo && tag.frame == Media::Video::FRAME_CONFIG && packet) // don't save the config "empty" (keep alive data stream!)
		pVideo->config.set(tag, packet);
}

void Publication::writeData(Media::Data::Type type, const Packet& packet, UInt8 track) {
	if (!_publishing) {
		ERROR("Data packet on ", _name, " publication stopped");
		return;
	}
	_publishing = 1;

	if (type != Media::Data::TYPE_TEXT) { // else has no handler!
		unique<DataReader> pReader(Media::Data::NewReader(type, packet));
		if (pReader) {
			string handler;
			// for performance reason read just if type is explicity a STRING (not a convertible string as binary etc...)
			if (pReader->nextType()==DataReader::STRING && pReader->readString(handler) && handler == "@properties")
				return addProperties(track, type, packet + (*pReader)->position());
		}
	} else if(track)
		DEBUG(track, " => ", packet);

	// create track
	if(track) {
		DataTrack& data = _datas[track];
		if (!data.textLang && type == Media::Data::TYPE_TEXT) {
			const char* lang(NULL);
			if (track <= _audios.size() && (lang = _audios[track].lang))
				setString(String(track, ".textLang"), lang); // audioLang can be used for metadata?
			else
				setString(String(track, ".textLang"), EXPAND("und"));
		}
	}

	// flush properties before to deliver media packet (to be sync with media content)
	flushProperties();

	_byteRate += packet.size();
	_datas.byteRate += packet.size();
	_new = true;
	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->writeData(type, packet, track);
	}
}

void Publication::onParamChange(const string& key, const string* pValue) {
	Media::Properties::onParamChange(key, pValue);
	size_t found = key.find('.');
	if (found == string::npos)
		return;
	UInt8 track = String::ToNumber<UInt8, 0>(key.c_str(), found);
	if (!track)
		return;
	const char* prop = key.c_str()+found+1;

	if (String::ICompare(prop, "audioLang") == 0)
		_audios[track].lang = pValue ? pValue->c_str() : NULL;
	else if (String::ICompare(prop, "textLang") == 0)
		_datas[track].textLang = pValue ? pValue->c_str() : NULL;
}
void Publication::onParamClear() {
	Media::Properties::onParamClear();
	for (AudioTrack& track : _audios)
		track.lang = NULL;
	for (DataTrack& track : _datas)
		track.textLang = NULL;
}

void Publication::flushProperties() {
	if (_timeProperties >= Media::Properties::timeChanged())
		return;
	_timeProperties = Media::Properties::timeChanged();
	// Logs before subscription logs!
	if (self)
		INFO("Write ", _name, " publication properties ", self)
	else
		INFO("Clear ", _name, " publication properties");
	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->writeProperties(self);
	}
}

} // namespace Mona
