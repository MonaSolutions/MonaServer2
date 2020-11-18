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
	playlist.removeItems().sequence = 0;

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

Playlist& Segments::Erase(UInt32 count, Playlist& playlist, IOFile& io) {
	while(count-- && playlist.count()) {
		FileWriter::Erase(Segment::BuildPath(playlist, playlist.sequence, playlist.durations.front()), io);  // asynchronous deletion!
		playlist.removeItem();
	}
	return playlist;
}

bool Segments::Writer::newSegment(UInt32 time, const OnWrite& onWrite) {
	if (_first) {
		_first = false;
		_lastTime = _segTime = time;
		return false;
	}
	// save _lastTime just for the last sequence,
	// use "time" otherwise to cut segments compatible with key frame with composition offset: their DTS have to be the segment front
	if(Util::Distance(_lastTime, time)>0)
		_lastTime = time;
	
	// change sequences if DURATION_TIMEOUT
	UInt16 duration = UInt16(Util::Distance(_segTime, time));
	Int16 rest = Segment::MAX_DURATION - duration;
	if (rest>=0) {
		// wait number of sequences requested in trying to keep a key on start of sequence
		if (!_keying || !_sequences)
			return false;
		// look if rest time to add one new key-frames sequence again
		if (rest >= (duration / _sequences)) {
			// check if sequences defined reached
			if(!sequences || _sequences < sequences)
				return false;
		}
	}

	// end
	_writer.endMedia(onWrite);
	onSegment(duration);
	_segTime = time;

	// begin
	_sequences = 0;
	_keying = false;
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
	_sequences = 0;
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
	} else
		newSegment(tag.time, onWrite);
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
					for (const auto& it : _videoConfigs)
						_writer.writeVideo(it.first, it.second, it.second, onWrite);
				}
				++_sequences; // after newSegment to avoid to segment on first video
			}
			break;
		case Media::Video::FRAME_INTER:
		case Media::Video::FRAME_DISPOSABLE_INTER:
			_keying = false;
		default:
			newSegment(tag.time, onWrite);
	}
	// not config!
	_writer.writeVideo(track, tag, packet, onWrite);
}


/// SEGMENTS //////

Segments::Segments(UInt8 maxSegments) : _maxDuration(Segment::MIN_DURATION), _maxSegments(maxSegments), _sequence(0), _writer(self), sequences(_writer.sequences) {
	_writer.onSegment = [this](UInt16 duration) {
		// add the valid segment to _segments
		if (_segments.size() >= _maxSegments) {
			++_sequence; // increments just on remove
			_segments.pop_front();
		}
		_segment.add(_segment.time() + duration);
		_segments.emplace_back(move(_segment));
		if (duration > _maxDuration)
			_maxDuration = duration;
	};
}

UInt8 Segments::setMaxSegments(UInt8 maxSegments) {
	_maxSegments = maxSegments;
	if (maxSegments) {
		while (maxSegments < _segments.size()) {
			++_sequence; // increments just on remove
			_segments.pop_front();
		}
		_segments.resize(maxSegments);
	} else {
		// stop segmentation
		_segments.clear();
		_sequence = 0;
		_segment.clear();
		_maxDuration = Segment::MIN_DURATION;
	}
	return maxSegments;
}

bool Segments::endMedia() {
	_writer.endMedia(nullptr);
	if (_segments.size())
		_segments.back().discontinuous = true;
	_segment.clear();
	_maxDuration = Segment::MIN_DURATION;
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
	// anticipate the _segments.pop() => sequence+1
	playlist.sequence = _sequence + 1;
	(UInt64&)playlist.maxDuration = _maxDuration;
	// Skip the first segment in playlist, because can be deleted by segments before request
	bool first = true;
	for (const Segment& segment : _segments) {
		if (first) {
			first = false;
			continue;
		}
		if (segment.discontinuous)
			playlist.addItem(0);
		playlist.addItem(segment.duration());
	}
	return playlist;
}

} // namespace Mona
