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

#pragma once

#include "Mona/Mona.h"
#include "Mona/DataWriter.h"
#include "Mona/Media.h"

namespace Mona {

struct Client;
struct Writer : Media::Target, virtual NullableObject {
	typedef Event<void(Int32 error, const char* reason)> ON(Close);

	virtual ~Writer();

	const bool				isMain;
	bool					reliable;
	/*!
	Allow to cancel temporary flush, to allow to write a safe transaction sequence cancelable */
	bool					flushable;

	virtual Writer&			newWriter() { return *this; }

	virtual void			clear() {} // must erase the queueing messages (don't change the writer state)

	/*!
	Close the writer, override closing(Int32 code) to execute closing code */
	void					close(Int32 error = 0, const char* reason=NULL);
	bool					closed() const { return _closed; }

	virtual bool			writeMember(const Client& client);

	virtual DataWriter&		writeInvocation(const char* name) { return DataWriter::Null(); }
	virtual DataWriter&		writeMessage() { return DataWriter::Null(); }
	virtual DataWriter&		writeResponse(UInt8 type) { return writeMessage(); }
	virtual void			writeRaw(DataReader& reader) {}

	void					flush() { if (!_closed && flushable) flushing(); }

	operator bool() const { return !_closed; }

	static Writer&			Null();

protected:
	Writer();

private:
	virtual void			flushing() {}

	/*!
	Override it to execute code on close
	If code=0, it's a normal writer close (or Session::ERROR_CLIENT)
	If code>0, it's a user close (or Session::DEATH_###)
	If code<0, it's a close without response possible  */
	virtual void			closing(Int32 code, const char* reason = NULL) {}

	bool					_closed;
};


} // namespace Mona
