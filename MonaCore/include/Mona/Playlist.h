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
	NULLABLE(_durations.empty())

	/*!
	Playlist master, path+name+bandwidth */
	struct Master : Path {
		NULLABLE(items.empty())
		Master(const Path& path) : Path(path) {}
		Master(Master&& playlist) : Path(std::move(playlist)), items(std::move(playlist.items)) {}
		std::map<std::string, UInt64> items;
	};
	/*!
	Write a master playlist */
	static bool Write(Exception& ex, const std::string& type, const Playlist::Master& playlist, Buffer& buffer);


	/*!
	Write a playlist type static (VOD or live-memory) */
	static bool Write(Exception& ex, const std::string& type, const Playlist& playlist, Buffer& buffer);
	/*!
	Write a playlist type event (file) */
	struct Writer : FileWriter, virtual Object {
		Writer(IOFile& io) : FileWriter(io) {}

		void write(Playlist& playlist, UInt16 duration);
	
	private:
		virtual void open(const Playlist& playlist) = 0;
		virtual void write(const std::string& format, UInt32 sequence, UInt16 duration) = 0;
		// set close private to allow child class to write a last sequence in desctructor on end-of-file (END-LIST for example) in 
		void close() { FileWriter::close(); } 
	};

	/*!
	Represents a playlist, fix maxDuration on item addition and sequence on remove item
	Path must informs on name and format playlist by its extension: ts/mp4 */
	Playlist(const Path& path) : Path(path), sequence(0), maxDuration(0), _duration(0) {}


	UInt32						sequence;
	UInt16						maxDuration;
	UInt32						duration() const { return _duration; }
	const std::deque<UInt16>	durations() const { return _durations; }
	UInt32						count() const { return _durations.size(); }

	template <typename ...Args>
	Playlist&					setPath(Args&&... args) { Path::set(std::forward<Args>(args)...); return self; }
	Playlist&					addItem(UInt16 duration);
	Playlist&					removeItem();
	Playlist&					reset();
private:
	void set() {} // make private Path::set, use setPath rather

	UInt32				_duration;
	std::deque<UInt16>	_durations;
};

} // namespace Mona

