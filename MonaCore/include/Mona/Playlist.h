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

#pragma once

#include "Mona/Mona.h"
#include "Mona/FileWriter.h"
#include "Mona/Segment.h"

namespace Mona {


struct Playlist : Path, virtual Object {
	NULLABLE(durations.empty())
	/*!
	Write a playlist type live (memory) */
	static bool Write(Exception& ex, const Playlist& playlist, Buffer& buffer);

	/*!
	Write a playlist type event (file) */
	struct Writer : FileWriter, virtual Object {
		Writer(IOFile& io) : FileWriter(io) {}

		void write(Playlist& playlist, UInt16 duration);
	
	private:
		virtual void open(const Playlist& playlist) = 0;
		virtual void write(UInt32 sequence, UInt16 duration) = 0;
		// set close private to allow child class to write a last sequence in desctructor on end-of-file (END-LIST for example) in 
		void close() { FileWriter::close(); } 
	};

	/*!
	Represents a playlist, fix maxDuration on item addition and sequence on remove item*/
	Playlist(const Path& path) : Path(path), sequence(0), maxDuration(0) {}

	UInt32						sequence;
	UInt64						maxDuration;
	const std::deque<UInt16>	durations;
	UInt32						count() const { return durations.size(); }

	Playlist&					addItem(UInt16 duration);
	Playlist&					removeItem();
	Playlist&					removeItems();
};

} // namespace Mona

