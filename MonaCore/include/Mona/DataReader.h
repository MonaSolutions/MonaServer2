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
#include "Mona/BinaryReader.h"
#include "Mona/DataWriter.h"

namespace Mona {

struct DataReader : virtual Object {
	NULLABLE

	enum : UInt8 {
		END=0, // keep equal to 0!
		NIL,
		BOOLEAN,
		NUMBER,
		STRING,
		DATE,
		BYTES,
		OTHER
	};

	static UInt8	ParseValue(const char* value, UInt32 size, double& number);

	void			next(UInt32 count=1) { read(DataWriter::Null(), count); }
	UInt8			nextType() { if (!_nextType) _nextType = followingType(); return _nextType; }

	// return the number of writing success on writer object
	// can be override to capture many reading on the same writer
	virtual UInt32	read(DataWriter& writer,UInt32 count=END);

	bool			read(UInt8 type, DataWriter& writer);
	bool			available() { return nextType()!=END; }

////  OPTIONAL DEFINE ////
	virtual void	reset() { reader.reset(); }
////////////////////

	template <typename BufferType>
	bool			readString(BufferType& buffer) { BufferWriter<BufferType> writer(buffer); return read(STRING, writer); }
	template <typename NumberType>
	bool			readNumber(NumberType& value) {  NumberWriter<NumberType> writer(value); return read(NUMBER, writer); }
	bool			readBoolean(bool& value) {  BoolWriter writer(value); return read(BOOLEAN, writer); }
	bool			readDate(Date& date) { DateWriter writer(date); return read(DATE, writer); }
	bool			readNull() { return read(NIL,DataWriter::Null()); }
	template <typename BufferType>
	bool			readBytes(BufferType& buffer) { BufferWriter<BufferType> writer(buffer); return followingType()==STRING ? read(STRING, writer) : read(BYTES, writer); }

	operator bool() const { return reader.operator bool(); }
	BinaryReader*		operator->() { return &reader; }
	const BinaryReader*	operator->() const { return &reader; }
	BinaryReader&		operator*() { return reader; }
	const BinaryReader&	operator*() const { return reader; }

	static DataReader&	Null();

protected:
	DataReader(const UInt8* data, UInt32 size) : reader(data, size, Byte::ORDER_NETWORK), _nextType(END) {}
	DataReader() : reader(NULL, 0, Byte::ORDER_NETWORK), _nextType(END) {}

	BinaryReader	reader;

	bool			readNext(DataWriter& writer);
	
private:
	
////  TO DEFINE ////
	// must return true if something has been written in DataWriter object (so if DataReader has always something to read and write, !=END)
	virtual bool	readOne(UInt8 type, DataWriter& writer) = 0;
	virtual UInt8	followingType() = 0;
////////////////////

	UInt8			_nextType;
	
	template<typename NumberType>
	struct NumberWriter : DataWriter {
		NumberWriter(NumberType& value) : _value(value) {}
		UInt64	beginObject(const char* type) { return 0; }
		void	writePropertyName(const char* value) {}
		void	endObject() {}
		
		UInt64	beginArray(UInt32 size) { return 0; }
		void	endArray() {}

		UInt64	writeDate(const Date& date) { _value = (NumberType)date.time(); return 0; }
		void	writeNumber(double value) { _value = (NumberType)value; }
		void	writeString(const char* value, UInt32 size) { String::ToNumber(value, size, _value); }
		void	writeBoolean(bool value) { _value = (value ? 1 : 0); }
		void	writeNull() { _value = 0; }
		UInt64	writeBytes(const UInt8* data, UInt32 size) { writeString(STR data,size); return 0; }
	private:
		NumberType&	_value;
	};
	struct DateWriter : DataWriter {
		DateWriter(Date& date) : _date(date) {}
		UInt64	beginObject(const char* type) { return 0; }
		void	writePropertyName(const char* value) {}
		void	endObject() {}
		
		UInt64	beginArray(UInt32 size) { return 0; }
		void	endArray() {}

		UInt64	writeDate(const Date& date) { _date = date; return 0; }
		void	writeNumber(double value) { _date = (Int64)value; }
		void	writeString(const char* value, UInt32 size) { Exception ex; _date.update(ex, value, size); }
		void	writeBoolean(bool value) { _date = (value ? Time::Now() : 0); }
		void	writeNull() { _date = 0; }
		UInt64	writeBytes(const UInt8* data, UInt32 size) { writeString(STR data,size); return 0; }

	private:
		Date&	_date;
	};
	struct BoolWriter : DataWriter {
		BoolWriter(bool& value) : _value(value) {}
		UInt64	beginObject(const char* type) { return 0; }
		void	writePropertyName(const char* value) {}
		void	endObject() {}
		
		UInt64	beginArray(UInt32 size) { return 0; }
		void	endArray() {}

		UInt64	writeDate(const Date& date) { _value = date ? true : false; return 0; }
		void	writeNumber(double value) { _value = value ? true : false; }
		void	writeString(const char* value, UInt32 size) { _value = !String::IsFalse(value, size); }
		void	writeBoolean(bool value) { _value = value; }
		void	writeNull() { _value = false; }
		UInt64	writeBytes(const UInt8* data, UInt32 size) { writeString(STR data,size); return 0; }

	private:
		bool&	_value;
	};

	template<typename BufferType>
	struct BufferWriter : DataWriter {
		BufferWriter(BufferType& buffer) : _buffer(buffer) { }
		UInt64	beginObject(const char* type) { return 0; }
		void	writePropertyName(const char* value) {}
		void	endObject() {}
		
		UInt64	beginArray(UInt32 size) { return 0; }
		void	endArray() {}

		UInt64	writeDate(const Date& date) { String::Assign(_buffer,date.time()); return 0; }
		void	writeNumber(double value) { String::Assign(_buffer,value); }
		void	writeString(const char* value, UInt32 size) { _buffer.clear(); _buffer.append(value, size); }
		void	writeBoolean(bool value) { String::Assign(_buffer,value); }
		void	writeNull() { writeString(EXPAND("null")); }
		UInt64	writeBytes(const UInt8* data, UInt32 size) { writeString(STR data, size); return 0; }
	private:
		BufferType&  _buffer;
	};
};


} // namespace Mona
