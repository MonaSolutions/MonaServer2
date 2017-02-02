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
#include "Mona/Path.h"
#include "Mona/ThreadQueue.h"
#include "Mona/Congestion.h"
#include "Mona/Event.h"

namespace Mona {

/*!
File is a Path file with read and write operation 
/!\ Direct access to disk cache, no buffering */
struct File : virtual NullableObject {
	typedef Event<void(shared<Buffer>& pBuffer, bool end)>	OnReaden;
	typedef Event<void(const Exception&)>					OnError;
	typedef Event<void()>									OnFlush;
	/*!
	Decoder offers to decode data in the reception thread when file is used with IOFile
	decode returns the size of data decoded passing to onReaden,
	or if pBuffer is captured and returns > 0 it reads returned size */
	struct Decoder { virtual UInt32 decode(shared<Buffer>& pBuffer, bool end) = 0; };

	// A mode R+W has no sense at this system level, because there is just one reading/writing shared header (R and W position)
	enum Mode {
		MODE_READ = 0, // To allow to test WRITE with a simple "if"
		MODE_WRITE,
		MODE_APPEND
	};
	File(const Path& path, Mode mode);
	~File();

	const Mode  mode;

	explicit operator bool() const { return _handle != -1; }
	operator const Path&() const { return _path; }

	// properties
	const std::string&  path() const { return _path; }
	const std::string&	name() const { return _path.name(); }
	const std::string&	baseName() const { return _path.baseName(); }
	const std::string&	extension() const { return _path.extension(); }
	const std::string&  parent() const { return _path.parent(); }
	bool				isAbsolute() const { return _path.isAbsolute(); }

	bool		exists(bool refresh = false) const { return _path.exists(refresh); }
	UInt64		size(bool refresh = false) const;
	Int64		lastModified(bool refresh = false) const { return _path.size(refresh); }
	UInt8		device() const { return _path.device(); }

	bool loaded() const { return _handle != -1; }

	// File operation
	/*!
	If reading error => Ex::Permission || Ex::Unfound || Ex::Intern */
	bool				load(Exception& ex);
	/*!
	If reading error => Ex::System::File || Ex::Intern */
	int					read(Exception& ex, void* data, UInt32 size);
	/*!
	If writing error => Ex::System::File || Ex::Intern */
	bool				write(Exception& ex, const void* data, UInt32 size);

	void				reset();

	/*!
	Attributes used when manipulate whithin IOFile */
	const Congestion& congestion() { Congestion* pCongestion(_pDevice); return pCongestion ? *pCongestion : Congestion::Null(); }
	bool			  queueing() const { return _flushing; }

private:
	Path			_path;
	long			_handle;

	//// Used by IOFile /////////////////////
	shared<Decoder>				pDecoder;
	OnReaden					onReaden;
	OnFlush						onFlush;
	OnError						onError;

	std::atomic<Congestion*>	_pDevice;

	std::atomic<bool>			_flushing;
	std::atomic<UInt32>			_loading;
	UInt16						_loadingTrack;
	UInt16						_decodingTrack;
	friend struct IOFile;
};


} // namespace Mona
