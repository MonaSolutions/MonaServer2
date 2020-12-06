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
#include "Mona/Media.h"
#include "Mona/Util.h"

namespace Mona {


/*!
Class to save a segment, limited in size to UInt16 duration*/
struct Segment : virtual Object  {
	NULLABLE(!count())

	/*!
	Format segment name in the format NAME.S### with ### duration encoded, and S the sequence number */
	template<typename BufferType>
	static BufferType& WriteName(const std::string& name, UInt32 sequence, UInt16 duration, BufferType& buffer) {
		return WriteDuration(duration, String::Append(buffer, name, '.', sequence));
	}
	/*!
	Format segment duration in 3 bytes encoded in Base64URL */
	template<typename BufferType>
	static BufferType& WriteDuration(UInt16 duration, BufferType& buffer) {
		duration = Byte::To16Network(duration);
		buffer.resize(Util::ToBase64URL(BIN &duration, 2, buffer, true)); // without '=' padding
		return buffer;
	}
	/*!
	Read sequence and duration from name and returns size of basename, if file is not in a segment format NAME.S###.EXT returns string::npos */
	static std::size_t ReadName(const std::string& name, UInt32& sequence, UInt16& duration);
	/*!
	Build segment path in the format PARENT/BASENAME.S### with S the sequence number and ### duration encoded */
	static Path BuildPath(const Path& path, UInt32 sequence, UInt16 duration) {
		std::string buffer;
		return Path(path.parent(), path.baseName(), '.', sequence, WriteDuration(duration, buffer), '.', path.extension());
	}
	
	Segment() : _lastTime(0), _discontinuous(false) {}
	Segment(const Segment& segment) : _lastTime(segment._lastTime), 
		_discontinuous(segment._discontinuous), _medias(segment._medias) {
		if (segment._pFirstTime)
			_pFirstTime.set(*segment._pFirstTime);
	}
	Segment(Segment&& segment) : _lastTime(segment._lastTime), _pFirstTime(std::move(segment._pFirstTime)),
		_discontinuous(segment._discontinuous), _medias(std::move(segment._medias)) {
		segment._discontinuous = false;
	}

	bool discontinuous() const { return _discontinuous; }

	typedef std::vector<shared<const Media::Base>>::const_iterator const_iterator;
	const_iterator	begin() const { return _medias.begin(); }
	const_iterator	end() const { return _medias.end(); }

	UInt32			count() const { return _medias.size(); }
	UInt32			time() const { return _pFirstTime ? *_pFirstTime : 0;  }
	UInt16			duration() const { return _pFirstTime ? UInt16(Util::Distance(*_pFirstTime, _lastTime)) : 0; }

	void			reset() { _discontinuous = true; _medias.clear(); _pFirstTime.reset(); }

	template<typename MediaType, typename ...Args>
	bool			add(Args&&... args) {
		_medias.emplace_back();
		MediaType& media = _medias.back().set<MediaType>(std::forward<Args>(args)...);
		if (!media.hasTime() || add(media.time()))
			return true;
		// rejected!
		_medias.pop_back();
		return false;
	}
	bool			add(UInt32 time);
	
	static const Segment& Null() {
		static const Segment Null;
		return Null;
	}

private:
	std::vector<shared<const Media::Base>> _medias;
	unique<UInt32>						   _pFirstTime;
	UInt32								   _lastTime;
	bool								   _discontinuous;
};

} // namespace Mona

