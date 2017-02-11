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

#include "Mona/Writer.h"
#include "Mona/Client.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

Writer& Writer::Null() {
	static struct WriterNull : Writer, virtual Object {
		WriterNull() { flushable = false; _closed = true; }
		UInt64 queueing() const { return 0; }
	} Null;
	return Null;
}

Writer::~Writer() {
	// Close is usefull since script wrapper object, to signal C++ obj deletion!!
	// closing(error) and flush() have no effect here (children class is already deleted), we have just need to set _closed=true and raise OnClose
	close(); 
}


void Writer::close(Int32 error, const char* reason) {
	if(_closed)
		return;
	if (error < 0) // if impossible to send (code<0, See Session::DEATH_...) clear message queue impossible to send!
		clear();
	closing(error, reason);
	if (error >= 0)
		flush();
	_closed = true; // before onClose event to avoid overflow if isMain and so call kill, and kill close the main writer, etc...
	onClose(error, reason);
}



} // namespace Mona
