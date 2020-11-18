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

#include "Mona/Playlist.h"
#include "Mona/M3U8.h"

using namespace std;

namespace Mona {

bool Playlist::Write(Exception& ex, const Playlist& playlist, Buffer& buffer) {
	if (!playlist) {
		ex.set<Ex::Unfound>("Playlist ", playlist.name(), " empty");
		return false;
	}
	if (String::ICompare(playlist.extension(), EXPAND("m3u8")) == 0) {
		M3U8::Write(playlist, buffer);
		return true;
	}
	ex.set<Ex::Unsupported>("Playlist ", playlist.name(), " unsupported");
	return false;
}

void Playlist::Writer::write(Playlist& playlist, UInt16 duration) {
	if (!opened()) {
		// assign duration, but not addItem yet to differenciate NEW item of OLD item (discontinuity)
		if (duration > playlist.maxDuration)
			(UInt64&)playlist.maxDuration = duration;
		open(playlist);
	}
	write(playlist.sequence + playlist.count(), duration);
	playlist.addItem(duration);
}

Playlist& Playlist::addItem(UInt16 duration) {
	if (duration > maxDuration)
		(UInt64&)maxDuration = duration;
	((deque<UInt16>&)durations).emplace_back(duration);
	return self;
}

Playlist& Playlist::removeItem() {
	// don't fix maxDuration, must stay unchanged
	if (durations.size()) {
		++sequence;
		((deque<UInt16>&)durations).pop_front();
	}
	return self;
}

Playlist& Playlist::removeItems() {
	sequence += durations.size();
	((deque<UInt16>&)durations).clear();
	return self;
}




} // namespace Mona
