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

bool Segments::MathSegment(const Path& path, const Path& file, UInt32& sequence) {
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
		if(MathSegment(path, filePath))
			FileWriter(io).open(filePath).erase(); // asynchronous deletion!
		return true;
	});
	return FileSystem::ListFiles(ex, path.parent(), forEach)<0 ? false : true;
}

UInt32 Segments::NextSequence(Exception &ex, const Path& path, IOFile& io) {
	UInt32 lastSequence = 0;
	FileSystem::ForEach forEach([&path, &lastSequence](const string& file, UInt16 level) {
		UInt32 sequence;
		if (MathSegment(path, file, sequence) && sequence > lastSequence)
			lastSequence = sequence;
		return true;
	});
	return FileSystem::ListFiles(ex, path.parent(), forEach) ? (lastSequence+1) : 0;
}


bool Segments::Writer::newSegment(const OnWrite& onWrite) {
	if (!sequences || ++_sequence<sequences)
		return false; // wait sequences!
	_sequence = 0;
	onSegment(_lastTime - _segTime);
	_segTime = _lastTime;

	// Rewrite properties + configs to make segment readable individualy
	// properties
	if (_pProperties)
		_pWriter->writeProperties(Media::Properties(_pProperties->track, _pProperties->tag, *_pProperties), onWrite);
	// send config audio + video!
	for (auto& it : _videoConfigs) {
		// fix DTS, can provoke warning in VLC if next frame has compositionOffset (picture too late),
		// but we can ignore it: doesn't impact playing or individual segment play
		it.second.time = _lastTime;
		_pWriter->writeVideo(it.first, it.second, it.second, onWrite);
	}
	for (auto& it : _audioConfigs) {
		it.second.time = _lastTime;
		_pWriter->writeAudio(it.first, it.second, it.second, onWrite);
	}
	return true;
}

void Segments::Writer::beginMedia(const OnWrite& onWrite) {
	_firstVideo = _first = true;
	_keying = false;
	_sequence = 0;
	_pWriter->beginMedia(onWrite);
}
void Segments::Writer::endMedia(const OnWrite& onWrite) {
	_audioConfigs.clear();
	_videoConfigs.clear();
	_pProperties.reset();
	_pWriter->endMedia(onWrite);
	if (!_first)
		onSegment(_lastTime - _segTime); // last segment, doesn't matter if not the exact duration
}
void Segments::Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
	const Packet& packet(properties.data(type));
	_pProperties.set<Media::Data>(type, packet);
	_pWriter->writeProperties(properties, onWrite);
}
void Segments::Writer::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (!tag.isConfig) {
		_lastTime = tag.time;
		if (_first) {
			_first = false;
			_segTime = tag.time;
		} else if (Util::Distance(_segTime, tag.time) >= DURATION_TIMEOUT)
			newSegment(onWrite);
	}
	_pWriter->writeAudio(track, tag, packet, onWrite);
	if (tag.isConfig) {
		if (packet)
			_audioConfigs[track].set(tag, packet);
		else
			_audioConfigs.erase(track);
	}
}
void Segments::Writer::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	switch (tag.frame) {
		case Media::Video::FRAME_CONFIG:
			// to support multitrack video without config packet before
			if (packet)
				_videoConfigs[track].set(tag, packet);
			else
				_videoConfigs.erase(track);
			_keying = false;
			return;
		case Media::Video::FRAME_KEY:
			_lastTime = tag.time;
			if (!_keying) { // to support multitrack video
				_keying = true;
				if (!_firstVideo && newSegment(onWrite))
					return _pWriter->writeVideo(track, tag, packet, onWrite);
				// first video
				for (const auto& it : _videoConfigs)
					_pWriter->writeVideo(it.first, it.second, it.second, onWrite);
				_firstVideo = false;
			}
			break;
		case Media::Video::FRAME_INTER:
		case Media::Video::FRAME_DISPOSABLE_INTER:
			_keying = false;
		default:
			_lastTime = tag.time;
	}
	// not config!
	if (_first) {
		_first = false;
		_segTime = tag.time;
	} else if (Util::Distance(_segTime, tag.time) >= DURATION_TIMEOUT)
		newSegment(onWrite);
	_pWriter->writeVideo(track, tag, packet, onWrite);
}


} // namespace Mona
