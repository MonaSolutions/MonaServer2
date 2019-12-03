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
#include "Mona/BinaryWriter.h"
#include "Mona/Date.h"
#include "Mona/Exceptions.h"
#include "Mona/Packet.h"

namespace Mona {

struct DataWriter : virtual Object {
	NULLABLE(!writer) // something has been written

////  TO DEFINE ////	
	virtual void   writePropertyName(const char* value)=0;

	virtual void   writeNumber(double value)=0;
	virtual void   writeString(const char* value, UInt32 size)=0;
	virtual void   writeBoolean(bool value)=0;
	virtual void   writeNull()=0;
	virtual UInt64 writeDate(const Date& date)=0;
	virtual UInt64 writeByte(const Packet& bytes)=0;
////////////////////


////  OPTIONAL DEFINE ////
	virtual UInt64 beginObject(const char* type = NULL) { return 0; }
	virtual void   endObject() {}

	virtual UInt64 beginArray(UInt32 size) { return 0; }
	virtual void   endArray() {}

	// if serializer don't support a mixed object, set the object as the first element of the array
	virtual UInt64 beginObjectArray(UInt32 size) { UInt64 ref(beginArray(size+1)); beginObject(); return ref; }

	virtual UInt64 beginMap(Exception& ex, UInt32 size, bool weakKeys = false) { ex.set<Ex::Format>(typeof(*this)," doesn't support map type, a object will be written rather");  return beginObject(); }
	virtual void   endMap() { endObject(); }

	virtual void   reset() { writer.clear(); }
	virtual bool   repeat(UInt64 reference) { return false; }

////////////////////
	void		   writeValue(const char* value, UInt32 size);
	void		   writeValue(UInt8 type, const char* value, UInt32 size, double number);
	void		   writeProperty(const char* name, const char* value, UInt32 size) { writePropertyName(name); writeValue(value, size); }

	void		   writeNullProperty(const char* name) { writePropertyName(name); writeNull(); }
	void		   writeDateProperty(const char* name, const Date& date) { writePropertyName(name); writeDate(date); }
	void		   writeNumberProperty(const char* name, double value) { writePropertyName(name); writeNumber(value); }
	void		   writeBooleanProperty(const char* name, bool value) { writePropertyName(name); writeBoolean(value); }
	void		   writeStringProperty(const char* name, const char* value, std::size_t size = std::string::npos) { writePropertyName(name); writeString(value, size==std::string::npos ? strlen(value) : size); }
	void		   writeStringProperty(const char* name, const std::string& value) { writePropertyName(name); writeString(value.data(), value.size()); }

	BinaryWriter*		operator->() { return &writer; }
	const BinaryWriter*	operator->() const { return &writer; }
	BinaryWriter&		operator*() { return writer; }
	const BinaryWriter&	operator*() const { return writer; }

	static DataWriter& Null();
protected:
	DataWriter(Buffer& buffer): writer(buffer, Byte::ORDER_NETWORK) {}
	DataWriter() : writer(Buffer::Null()) {}



	BinaryWriter   writer;
};


} // namespace Mona
