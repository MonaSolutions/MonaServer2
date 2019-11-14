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
#include "Mona/StringWriter.h"
#include "Mona/ByteWriter.h"

namespace Mona {

struct DataReader : Packet, virtual Object {

	enum : UInt8 {
		END=0, // keep equal to 0!
		NIL,
		BOOLEAN,
		NUMBER,
		STRING,
		DATE,
		BYTE,
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
	virtual void	reset() { _nextType = _type; reader.reset(); }
////////////////////

	template <typename BufferType>
	bool			readString(BufferType& buffer) { StringWriter<BufferType> writer(buffer, false); return read(STRING, writer); }
	template <typename NumberType>
	bool			readNumber(NumberType& value) {  NumberWriter<NumberType> writer(value); return read(NUMBER, writer); }
	bool			readBoolean(bool& value) {  BoolWriter writer(value); return read(BOOLEAN, writer); }
	bool			readDate(Date& date) { DateWriter writer(date); return read(DATE, writer); }
	bool			readNull() { return read(NIL,DataWriter::Null()); }
	bool			readByte(Packet& bytes) { ByteWriter writer(bytes); return read(BYTE, writer); }

	BinaryReader*		operator->() { return &reader; }
	const BinaryReader*	operator->() const { return &reader; }
	BinaryReader&		operator*() { return reader; }
	const BinaryReader&	operator*() const { return reader; }

	static DataReader&	Null();

protected:
	DataReader(const Packet& packet = Packet::Null(), UInt8 type = END) : Packet(packet), reader(packet.data(), packet.size(), Byte::ORDER_NETWORK), _type(type), _nextType(type) {}

	BinaryReader	reader;

	bool			readNext(DataWriter& writer);
	
private:
	
////  TO DEFINE ////
	// must return true if something has been written in DataWriter object (so if DataReader has always something to read and write, !=END)
	virtual bool	readOne(UInt8 type, DataWriter& writer) = 0;
	virtual UInt8	followingType() { return _nextType; }
////////////////////

	UInt8			_nextType;
	UInt8			_type;
	
	template<typename NumberType>
	struct NumberWriter : DataWriter {
		NumberWriter(NumberType& value) : _value(value) {}
		void	writePropertyName(const char* value) { String::ToNumber(value, _value); }
		UInt64	writeDate(const Date& date) { _value = (NumberType)date.time(); return 0; }
		void	writeNumber(double value) { _value = (NumberType)value; }
		void	writeString(const char* value, UInt32 size) { String::ToNumber(value, size, _value); }
		void	writeBoolean(bool value) { _value = (value ? 1 : 0); }
		void	writeNull() { _value = 0; }
		UInt64	writeByte(const Packet& bytes) { writeString(STR bytes.data(), bytes.size()); return 0; }
	private:
		NumberType&	_value;
	};
	struct DateWriter : DataWriter {
		DateWriter(Date& date) : _date(date) {}
		void	writePropertyName(const char* value) { Exception ex; _date.update(ex, value); }
		UInt64	writeDate(const Date& date) { _date = date; return 0; }
		void	writeNumber(double value) { _date = (Int64)value; }
		void	writeString(const char* value, UInt32 size) { Exception ex; _date.update(ex, value, size); }
		void	writeBoolean(bool value) { _date = (value ? Time::Now() : 0); }
		void	writeNull() { _date = 0; }
		UInt64	writeByte(const Packet& bytes) { writeString(STR bytes.data(), bytes.size()); return 0; }

	private:
		Date&	_date;
	};
	struct BoolWriter : DataWriter {
		BoolWriter(bool& value) : _value(value) {}

		void	writePropertyName(const char* value) { _value = !String::IsFalse(value); }
		UInt64	writeDate(const Date& date) { _value = date ? true : false; return 0; }
		void	writeNumber(double value) { _value = value ? true : false; }
		void	writeString(const char* value, UInt32 size) { _value = !String::IsFalse(value, size); }
		void	writeBoolean(bool value) { _value = value; }
		void	writeNull() { _value = false; }
		UInt64	writeByte(const Packet& bytes) { writeString(STR bytes.data(), bytes.size()); return 0; }

	private:
		bool&	_value;
	};
};


} // namespace Mona
