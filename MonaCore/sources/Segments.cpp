/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#include "Mona/Segments.h"
#include "Mona/FileWriter.h"
#include "Mona/Util.h"

using namespace std;

namespace Mona {

bool Segments::MatchSegment(const Path& path, const Path& file, UInt32& sequence) {
	if (file.extension() != path.extension())
		return false; // not same extension!
	if (file.baseName().compare(0, path.baseName().size(), path.baseName()) != 0)
		return false; // not same prefix name!
	const char* varName = file.baseName().c_str() + path.baseName().size();
	if (*varName++ != '.')
		return false; // not in the form baseName.##.ext
	if (!String::ToNumber(varName, sequence))
		return false; // not name with number as suffix
	return true;
}

bool Segments::Clear(Exception &ex, const Path& path, IOFile& io) {
	FileSystem::ForEach forEach([&path, &io](const string& file, UInt16 level) {
		Path filePath(file);
		if(MatchSegment(path, filePath))
			FileWriter(io).open(filePath).erase(); // asynchronous deletion!
		return true;
	});
	return FileSystem::ListFiles(ex, path.parent(), forEach)<0 ? false : true;
}

UInt32 Segments::NextSequence(Exception &ex, const Path& path, IOFile& io) {
	UInt32 lastSequence = 0;
	FileSystem::ForEach forEach([&path, &lastSequence](const string& file, UInt16 level) {
		UInt32 sequence;
		if (MatchSegment(path, file, sequence) && sequence > lastSequence)
			lastSequence = sequence;
		return true;
	});
	return FileSystem::ListFiles(ex, path.parent(), forEach) ? (lastSequence+1) : 0;
}


UInt32 Segments::Writer::lastTime() const {
	if (!_pVideoTime)
		return _pAudioTime ? *_pAudioTime : 0;
	return (_pAudioTime && Util::Distance(*_pVideoTime, *_pAudioTime) > 0) ? *_pAudioTime : *_pVideoTime;
}
UInt32 Segments::Writer::currentTime() const {
	if (!_pVideoTime)
		return _pAudioTime ? *_pAudioTime : 0;
	return (_pAudioTime && Util::Distance(*_pAudioTime, *_pVideoTime) > 0) ? *_pAudioTime : *_pVideoTime;
}

bool Segments::Writer::newSegment(const OnWrite& onWrite) {
	if (_first) {
		_first = false;
		_segTime = this->currentTime();
		return false;
	}
	UInt32 lastTime = this->lastTime();
	// change sequences if DURATION_TIMEOUT
	Int32 duration = Util::Distance(_segTime, lastTime);
	Int32 rest = DURATION_TIMEOUT - duration;
	if (rest>=0) {
		// wait number of sequences requested in trying to keep a key on start of sequence
		if (!_keying || !_sequence)
			return false;
		// look if rest time to add one new key-frames sequence again
		if (rest >= (duration / _sequence)) {
			// check if sequences defined reached
			if(!sequences || _sequence < sequences)
				return false;
		}
	}

	// end
	_pWriter->endMedia(onWrite);
	onSegment(lastTime - _segTime);
	_segTime = lastTime;

	// begin
	_sequence = 0;
	_keying = false;
	_pWriter->beginMedia(onWrite);

	// Rewrite properties + configs to make segment readable individualy
	// properties
	if (_pProperties)
		_pWriter->writeProperties(Media::Properties(0, _pProperties->tag, *_pProperties), onWrite);
	// send config audio + video!
	for (auto& it : _videoConfigs) {
		// fix DTS, can provoke warning in VLC if next frame has compositionOffset (picture too late),
		// but we can ignore it: doesn't impact playing or individual segment play
		it.second.time = lastTime;
		_pWriter->writeVideo(it.first, it.second, it.second, onWrite);
	}
	for (auto& it : _audioConfigs) {
		it.second.time = lastTime;
		_pWriter->writeAudio(it.first, it.second, it.second, onWrite);
	}
	return true;
}

void Segments::Writer::beginMedia(const OnWrite& onWrite) {
	_first = true;
	_keying = false;
	_sequence = 0;
	_pWriter->beginMedia(onWrite);
}
void Segments::Writer::endMedia(const OnWrite& onWrite) {
	_pWriter->endMedia(onWrite);
	if (!_first)
		onSegment(lastTime() - _segTime); // last segment, doesn't matter if not the exact duration
	// release resources
	_audioConfigs.clear();
	_videoConfigs.clear();
	_pProperties.reset();
	_pAudioTime.reset();
	_pVideoTime.reset();
}
void Segments::Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
	const Packet& packet(properties.data(type));
	_pProperties.set<Media::Data>(type, packet);
	_pWriter->writeProperties(properties, onWrite);
}
void Segments::Writer::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!tag.isConfig) {
		PTR_SET(_pAudioTime, tag.time);
		newSegment(onWrite);
	}
	_pWriter->writeAudio(track, tag, packet, onWrite);
	if (tag.isConfig) {
		// to support audio without config packet before => remove config packet
		if (packet)
			_audioConfigs[track].set(tag, packet);
		else
			_audioConfigs.erase(track);
	}
}
void Segments::Writer::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	switch (tag.frame) {
		case Media::Video::FRAME_CONFIG:
			// to support video without config packet before => remove config packet
			if (packet)
				_videoConfigs[track].set(tag, packet);
			else
				_videoConfigs.erase(track);
			_keying = false;
			// return => not write it immediatly if we change of sequence after key
			return;
		case Media::Video::FRAME_KEY:
			PTR_SET(_pVideoTime, tag.time);
			// change of sequence just on key frame (or audio if DURATION_TIMEOUT)
			if (!_keying) { // to support multitrack video => change sequence on first video key of any track!
				_keying = true;
				if (!newSegment(onWrite)) {
					// write all config packet "skip"
					for (const auto& it : _videoConfigs)
						_pWriter->writeVideo(it.first, it.second, it.second, onWrite);
				}
				++_sequence; // after newSegment to avoid to segment on first video
			}
			break;
		case Media::Video::FRAME_INTER:
		case Media::Video::FRAME_DISPOSABLE_INTER:
			_keying = false;
		default:
			PTR_SET(_pVideoTime, tag.time);
			newSegment(onWrite);
	}
	// not config!
	_pWriter->writeVideo(track, tag, packet, onWrite);
}


} // namespace Mona
