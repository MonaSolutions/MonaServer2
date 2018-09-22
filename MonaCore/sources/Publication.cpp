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
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

Publication::Publication(const string& name): _latency(0),
	audios(_audios), videos(_videos), datas(_datas), _lostRate(_byteRate),
	_publishing(false),_new(false), _newLost(false), _name(name) {
	DEBUG("New publication ",name);
}

Publication::~Publication() {
	stopRecording();
	// delete _listeners!
	if (!subscriptions.empty())
		CRITIC("Publication ",_name," with subscribers is deleting")
	if (_publishing)
		CRITIC("Publication ",_name," running is deleting")
	DEBUG("Publication ",_name," deleted");
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


void Publication::startRecording(MediaFile::Writer& recorder, bool append) {
	stopRecording();
	_pRecording.reset(new Subscription(recorder));
	NOTE("Start ", _name, "=>", recorder.path.name(), " recording");
	_pRecording->pPublication = this;
	_pRecording->setBoolean("append",append);
	((set<Subscription*>&)subscriptions).emplace(_pRecording.get());
	recorder.start(); // start MediaFile::Writer before subscription!
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

void Publication::start(MediaFile::Writer* pRecorder, bool append) {
	if (!_publishing) {
		_publishing = true;
		INFO("Publication ", _name, " started");
		_timeProperties = Media::Properties::timeProperties(); // useless to dispatch metadata changes, will be done on next media by Subscriber side!
	}
	if(pRecorder)
		startRecording(*pRecorder, append);
	// no flush here, wait first flush or first media to allow to subscribe to onProperties event for a publisher!
}

void Publication::reset() {
	if (!_publishing)
		return;
	INFO("Publication ", _name, " reseted");

	_audios.clear();
	_videos.clear();
	_datas.clear();

	// Erase track metadata just!
	auto it = begin();
	while(it!=end()) {
		size_t point = it->first.find('.');
		if (point != string::npos && String::ToNumber(it->first.data(), point, point))
			erase((it++)->first);
		else
			++it;
	}
	_timeProperties = Media::Properties::timeProperties(); // useless to dispatch metadata changes, will be done on next media by Subscriber side!

	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication != this && pSubscription->pPublication)
			continue; // subscriber not yet subscribed
		pSubscription->pPublication = this;
		pSubscription->reset(); // call writer.endMedia on subscriber side and do the flush!
	}
	
	onEnd();
}

void Publication::stop() {
	if(!_publishing)
		return; // already done

	stopRecording();

	_publishing =false;

	// release resources!
	_audios.clear();
	_videos.clear();
	_datas.clear();
	Media::Properties::clear();

	_latency = 0;
	_new = false;

	INFO("Publication ", _name, " stopped")

	auto it = subscriptions.begin();
	while (it != subscriptions.end()) { // using "while" rather "for each" because "reset" can remove an element of "subscriptions"!
		Subscription* pSubscription(*it++);
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->reset(); // call writer.endMedia on subscriber side and do the flush!
	}

	onFlush();
	onEnd();
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
	onFlush();
}


void Publication::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) {
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
	//	DEBUG(name()," audio ",tag.time);
	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->writeAudio(track, tag, packet);
	}
	onAudio(track, tag, packet);
	
	// Hold config packet after video distribution to avoid to distribute two times config packet if subscription call beginMedia
	if (pAudio && tag.isConfig)
		pAudio->config.set(tag, packet);
}

void Publication::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) {
	if (!_publishing) {
		ERROR("Video packet on stopped ", _name, " publication");
		return;
	}
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
			//	DEBUG("cc", channel, " => ", String::Data(pBuffer->data(), pBuffer->size()));
				writeData((track - 1) * 4 + channel, Media::Data::TYPE_TEXT, Packet(pBuffer));
			});
			CCaption::OnLang onLang([this, &tag, track](UInt8 channel, const char* lang) {
				if (lang) {
					DEBUG("Subtitle lang ", lang);
					setString(String((track - 1) * 4 + channel, ".textLang"), lang);
				} else
					erase(String((track - 1) * 4 + channel, ".textLang"));
			});
			offsetCC = pVideo->cc.extract(tag, packet, onText, onLang);
		}
	} else
		DEBUG("Video configuration received on publication ", _name, " (size=", packet.size(), ")");


	// flush properties before to deliver media packet (to be sync with media content)
	flushProperties();

	_byteRate += packet.size() + sizeof(tag);
	_videos.byteRate += packet.size() + sizeof(tag);
	_new = true;
	//	INFO(name(), " video ", tag.time, " (", tag.frame, ")");

	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication != this && pSubscription->pPublication)
			continue; // subscriber not yet subscribed
		if (offsetCC && (pSubscription->datas.pSelection || pSubscription->datas.multiTracks)) { // if a data track is selected (or data track is disabled when track data=0) => send without CC!
			if (offsetCC<packet.size())
				pSubscription->writeVideo(track, tag, packet + offsetCC); // without CC
		} else
			pSubscription->writeVideo(track, tag, packet); // with CC
	}
	if (offsetCC && onData) {
		if(offsetCC<packet.size())
			this->onVideo(track, tag, packet + offsetCC); // without CC
	} else
		this->onVideo(track, tag, packet); // with CC

	// Hold config packet after video distribution to avoid to distribute two times config packet if subscription call beginMedia
	if (pVideo && tag.frame == Media::Video::FRAME_CONFIG)
		pVideo->config.set(tag, packet);
}

void Publication::writeData(UInt8 track, Media::Data::Type type, const Packet& packet) {
	if (!_publishing) {
		ERROR("Data packet on ", _name, " publication stopped");
		return;
	}

	if (type != Media::Data::TYPE_TEXT) { // else has no handler!
		unique_ptr<DataReader> pReader(Media::Data::NewReader(type, packet));
		if (pReader) {
			string handler;
			// for performance reason read just if type is explicity a STRING (not a convertible string as binary etc...)
			if (pReader->nextType()==DataReader::STRING && pReader->readString(handler) && handler == "@properties")
				return setProperties(track, type, packet + (*pReader)->position());
		}
	}

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
			pSubscription->writeData(track, type, packet);
	}
	onData(track, type, packet);
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

	if (String::ICompare(prop, EXPAND("audioLang")) == 0)
		_audios[track].lang = pValue ? pValue->c_str() : NULL;
	else if (String::ICompare(prop, EXPAND("textLang")) == 0)
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
	if (_timeProperties >= Media::Properties::timeProperties())
		return;
	_timeProperties = Media::Properties::timeProperties();
	// Logs before subscription logs!
	if (self)
		INFO("Write ", _name, " publication properties ", self)
	else
		INFO("Clear ", _name, " publication properties");
	for (Subscription* pSubscription : subscriptions) {
		if (pSubscription->pPublication == this || !pSubscription->pPublication)
			pSubscription->writeProperties(self);
	}
	onProperties(self);
}

} // namespace Mona
