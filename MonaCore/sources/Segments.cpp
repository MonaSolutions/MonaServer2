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

bool Segments::Init(Exception& ex, IOFile& io, Playlist& playlist, bool append) {
	playlist.reset();

	FileSystem::ForEach forEach([&](const string& file, UInt16 level) {
		Path path(file);
		if (path.isFolder())
			return true; // folder!
		// don't compare extension because playlist for different type have the same name => overrides it!
		UInt32 sequence;
		UInt16 duration;
		size_t nameSize = Segment::ReadName(path.baseName(), sequence, duration);
		if (nameSize==string::npos)
			return true;
		if (!duration) {
			// temporary segment (not correctly finished), release it
			FileWriter::Erase(path, io); // asynchronous deletion!
			return true;
		}
		if (String::ICompare(path.baseName().data(), nameSize, playlist.baseName()) != 0)
			return true; // not a item of this playlist
		if (!append) {
			FileWriter::Erase(path, io); // asynchronous deletion!
			return true;
		}
		if (playlist.addItem(duration).count() > 1) {
			if (sequence < playlist.sequence)
				playlist.sequence = sequence;
		} else // assign first sequence
			playlist.sequence = sequence;
		return true;
	});
	return FileSystem::ListFiles(ex, playlist.parent(), forEach) >= 0;
}

Playlist& Segments::Clear(Playlist& playlist, UInt32 maxSegments, IOFile& io) {
	// erase obsolete file segments (keep one more if maxDuration not reached)
	UInt32 maxDuration = maxSegments * playlist.maxDuration;
	while (playlist.count() > (maxSegments + 1) || playlist.duration() > maxDuration) {
		FileWriter::Erase(Segment::BuildPath(playlist, playlist.sequence, playlist.durations().front()), io);  // asynchronous deletion!
		playlist.removeItem();
	}
	return playlist;
}

UInt16 Segments::Writer::setDuration(UInt16 value) {
	// at less one second!
	_duration = max(value, 1000);
	if(_duration>_maxDuration)
		_maxDuration = _duration;
	return _maxDuration;
}

bool Segments::Writer::newSegment(UInt32 time, const OnWrite& onWrite) {
	if (_first) {
		_first = false;
		_lastTime = _segTime = time;
		return false;
	}
	// save _lastTime just for the last sequence,
	// use "time" otherwise to cut segments compatible with key frame with composition offset: their DTS have to be the segment front
	if (Util::Distance(_lastTime, time) > 0)
		_lastTime = time;
	
	// new segment if DURATION_TIMEOUT
	Int32 duration = Util::Distance(_segTime, time);
	if (duration < 0)
		return false; // can happen if segment starts by a frame with composition offset

	if (_keying) {
		// Is on key sequence
		if (!_keys)
			return false; // wait at less one video key sequence		
		// wait at less a segment superior to targetduration = round(maxDuration)/3
		if (duration/1000  < ((_maxDuration+500)/3000))
			return false; // wait one other key-frames sequence
	} else {
		if (_keys) // has video
			return false; // wait video keying
		// No video => wait maxDuration
		if (duration < _maxDuration)
			return false;
	}

	// memorize the maxDuration to never cut less now
	if (duration > _maxDuration)
		_maxDuration = duration;

	// end
	_writer.endMedia(onWrite);
	onSegment(duration);
	_segTime = time;

	// begin
	_keys = 0;
	_writer.beginMedia(onWrite);
	
	// Rewrite properties + configs to make segment readable individualy
	// properties
	if (_pProperties)
		_writer.writeProperties(Media::Properties(0, _pProperties->tag, *_pProperties), onWrite);
	// send config audio + video!
	for (auto& it : _videoConfigs) {
		it.second.time = time; // fix timestamp to stay inscreasing
		_writer.writeVideo(it.first, it.second, it.second, onWrite);
	}
	for (auto& it : _audioConfigs) {
		it.second.time = time; // fix timestamp to stay inscreasing
		_writer.writeAudio(it.first, it.second, it.second, onWrite);
	}
	return true;
}

void Segments::Writer::beginMedia(const OnWrite& onWrite) {
	_first = true;
	_keying = false;
	_keys = 0;
	_maxDuration = _duration;
	_writer.beginMedia(onWrite);
}
void Segments::Writer::endMedia(const OnWrite& onWrite) {
	_writer.endMedia(onWrite);
	if (!_first) // last segment, doesn't matter if not the exact duration
		onSegment(UInt16(Util::Distance(_segTime, _lastTime)));
	// release resources
	_audioConfigs.clear();
	_videoConfigs.clear();
	_pProperties.reset();
}
void Segments::Writer::writeProperties(const Media::Properties& properties, const OnWrite& onWrite) {
	Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
	const Packet& packet(properties.data(type));
	_pProperties.set<Media::Data>(type, packet);
	_writer.writeProperties(properties, onWrite);
}

void Segments::Writer::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite) {
	if (tag.isConfig) {
		// to support audio without config packet before => remove config packet
		if (packet)
			_audioConfigs[track].set(tag, packet);
		else
			_audioConfigs.erase(track);
	} else {
		_keying = false;
		newSegment(tag.time, onWrite);
	}
	_writer.writeAudio(track, tag, packet, onWrite);
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
			// change of sequence just on key frame (or audio if DURATION_TIMEOUT)
			if (!_keying) { // to support multitrack video => change sequence on first video key of any track!
				_keying = true;
				if (!newSegment(tag.time, onWrite)) {
					// write all config packet "skip"
					for (auto& it : _videoConfigs) {
						it.second.time = tag.time; // fix timestamp to stay inscreasing
						_writer.writeVideo(it.first, it.second, it.second, onWrite);
					}
				}
				++_keys; // after newSegment to avoid to segment on first video
			}
			break;
		default:
			_keying = false;
			newSegment(tag.time, onWrite);
	}
	// not config!
	_writer.writeVideo(track, tag, packet, onWrite);
}


/// SEGMENTS //////

Segments::Segments(UInt8 maxSegments) : _started(false), _duration(0), _maxSegments(maxSegments), _sequence(0), _writer(self) {
	init();
}
Segments::Segments(Segments&& segments) : _started(false), _duration(segments._duration), _maxSegments(segments._maxSegments), _sequence(segments._sequence), _writer(self) {
	init();
	segments._sequence += segments.count();
	segments._duration = 0;
	_segments = std::move(segments._segments);
}

void Segments::init() {
	_writer.onSegment = [this](UInt16 duration) {
		// add the valid segment to _segments
		_segment.add(_segment.time() + duration);
		_duration += duration;
		_segments.emplace_back(move(_segment));
		setMaxSegments(_maxSegments); // clean segments
		onSegment(duration);
	};
}

UInt8 Segments::setMaxSegments(UInt8 maxSegments) {
	// erase obsolete segments (keep one more if maxDuration not reached)
	UInt32 maxDuration = maxSegments * this->maxDuration();
	while (_segments.size() > (maxSegments+1u) || _duration > maxDuration) {
		++_sequence; // increments just on remove
		_duration -= _segments.front().duration();
		_segments.pop_front();
	}
	return _maxSegments = maxSegments;;
}

bool Segments::beginMedia(const string& name) {
	// can be called multiple time, just do a _writer.endMedia() on double call
	if (_started)
		_writer.endMedia(nullptr);
	_started = true;
	_writer.beginMedia(nullptr);
	return true;
}
bool Segments::endMedia() {
	if (!_started)
		return false;
	_started = false;
	_writer.endMedia(nullptr);
	// reset segment after endMedia because onSegment will emplace_back this last segment
	_segment.reset();
	// Don't reset _maxDuration and segments, must stays alive for playlist usage (delete the Segments object to reset all)
	return true;
}


const Segment& Segments::operator()(Int32 sequence) const {
	if (sequence < 0)
		sequence += _segments.size();
	else
		sequence -= _sequence;
	if(sequence < 0)
		return Segment::Null();
	if (UInt32(sequence) >= _segments.size())
		return Segment::Null();
	// sequence is index now!
	return _segments[(UInt32)sequence];
}

Playlist& Segments::to(Playlist& playlist) const {
	playlist.reset().sequence = _sequence;
	(UInt64&)playlist.maxDuration = maxDuration();
	// Skip the first segment in playlist, because can be deleted by segments before request
	bool first = _segments.size() >= _maxSegments;
	for (const Segment& segment : _segments) {
		if (first) {
			first = false;
			// anticipate the _segments.pop() => sequence+1
			++playlist.sequence;
			continue;
		}
		if (segment.discontinuous())
			playlist.addItem(0); // discontinuous
		playlist.addItem(segment.duration());
	}
	if (!_started)
		playlist.addItem(0); // end
	return playlist;
}

} // namespace Mona
