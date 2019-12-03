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

#include "Mona/AMFWriter.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

AMFWriter::AMFWriter(Buffer& buffer, bool amf0) : _amf0References(0),_amf3(false),amf0(amf0),DataWriter(buffer) {

}

void AMFWriter::reset() {
	_amf3 = false;
	_levels.clear();
	_references.clear();
	_amf0References = 0;
	_stringReferences.clear();
	DataWriter::reset();
}

bool AMFWriter::repeat(UInt64 reference) {
	if (reference & 0x01) {
		// AMF3
		reference >>= 1;
		if(--reference>=_references.size()) {
			ERROR("No AMF3 reference to repeat");
			return false;
		}
		if (!_amf3)
			writer.write8(AMF::AMF0_AMF3_OBJECT);
		writer.write8(_references[(vector<UInt8>::size_type&)reference]);
		writer.write7Bit<UInt32>((UInt32)reference<<1, 4);
		return true;
	}

	// AMF0
	reference >>= 1;
	if(--reference>=_amf0References) {
		ERROR("No AMF0 reference to repeat");
		return false;
	}
	writer.write8(AMF::AMF0_REFERENCE);
	writer.write16((UInt16)reference);
	return true;
}

void AMFWriter::writeString(const char* value, UInt32 size) {
	if(!_amf3) {
		if(amf0) {
			if(size>65535) {
				writer.write8(AMF::AMF0_LONG_STRING);
				writer.write32(size);
			} else {
				writer.write8(AMF::AMF0_STRING);
				writer.write16(size);
			}
			writer.write(value,size);
			return;
		}
		writer.write8(AMF::AMF0_AMF3_OBJECT);
	}
	writer.write8(AMF::AMF3_STRING); // marker
	writeText(value,size);
	return;
}

void AMFWriter::writePropertyName(const char* name) {
	UInt32 size(strlen(name));
	// no marker, no string_long, no empty value
	if(!_amf3) {
		writer.write16(size).write(name,size);
		return;
	}
	writeText(name,size);
}

void AMFWriter::writeText(const char* value,UInt32 size) {
	if(size>0) {
		const auto& it = _stringReferences.emplace(SET, forward_as_tuple(value, size), forward_as_tuple(_stringReferences.size()));
		if (!it.second) {
			// already exists
			writer.write7Bit<UInt32>(it.first->second << 1, 4);
			return;
		}
	}
	writer.write7Bit<UInt32>((size<<1) | 0x01, 4).write(value,size);
}

void AMFWriter::writeNull() {
	writer.write8(_amf3 ? UInt8(AMF::AMF3_NULL) : UInt8(AMF::AMF0_NULL)); // marker
}

void AMFWriter::writeBoolean(bool value){
	if(!_amf3)
		writer.write8(AMF::AMF0_BOOLEAN).writeBool(value);
	else
		writer.write8(value ? AMF::AMF3_TRUE : AMF::AMF3_FALSE);
}

UInt64 AMFWriter::writeDate(const Date& date){
	if(!_amf3) {
		if(amf0) {
			writer.write8(AMF::AMF0_DATE);
			writer.write64(date);
			writer.write16(0); // Timezone, useless in AMF0 format (always equals 0)
			return 0;
		}
		writer.write8(AMF::AMF0_AMF3_OBJECT);
	}
	writer.write8(AMF::AMF3_DATE);
	writer.write8(0x01);
	writer.write64(date);
	_references.emplace_back(AMF::AMF3_DATE);
	return (_references.size()<<1) | 1;
}

void AMFWriter::writeNumber(double value){
	if(!amf0 && round(value) == value && abs(value)<=0xFFFFFFF) {
		// writeInteger
		if(!_amf3)
			writer.write8(AMF::AMF0_AMF3_OBJECT);
		writer.write8(AMF::AMF3_INTEGER); // marker
		if(value<0)
			value+=(1<<29);
		writer.write7Bit<UInt32>((UInt32)value, 4);
		return;
	}
	writer.write8(_amf3 ? UInt8(AMF::AMF3_NUMBER) : UInt8(AMF::AMF0_NUMBER)); // marker
	writer.writeDouble(value);
}

UInt64 AMFWriter::writeByte(const Packet& bytes) {
	if (!_amf3) {
		if (amf0)
			WARN("Impossible to write a byte array in AMF0, switch to AMF3");
		writer.write8(AMF::AMF0_AMF3_OBJECT); // switch in AMF3 format
	}
	writer.write8(AMF::AMF3_BYTEARRAY); // bytearray in AMF3 format!
	writer.write7Bit<UInt32>((bytes.size() << 1) | 1, 4);
	writer.write(bytes.data(), bytes.size());
	_references.emplace_back(AMF::AMF3_BYTEARRAY);
	return (_references.size()<<1) | 0x01;
}

UInt64 AMFWriter::beginObject(const char* type) {
	_levels.push_back(_amf3);
	if(!_amf3) {
		if(amf0) {	
			if (!type)
				writer.write8(AMF::AMF0_BEGIN_OBJECT);
			else {
				writer.write8(AMF::AMF0_BEGIN_TYPED_OBJECT);
				UInt16 size((UInt16)strlen(type));
				writer.write16(size).write(type,size);
			}
			return (++_amf0References)<<1;
		}
		writer.write8(AMF::AMF0_AMF3_OBJECT);
		_amf3=true;
	}
	writer.write8(AMF::AMF3_OBJECT);

	/* Can be ignored finally!
	if(externalizable) {
		// What follows is the value of the “inner” object
	} else if(hardProperties>0 && !pClassDef) {
		// The remaining integer-data represents the number of class members that exist.
		// If there is a class-def reference there are no property names and the number of values is equal to the number of properties in the class-def
		flags |= (hardProperties<<4);
	}*/

	// ClassDef always inline (because never hard properties, all is dynamic)
	// Always dynamic (but can't be externalizable AND dynamic!)
	writer.write7Bit<UInt32>(11, 4); // 00001011 => inner object + classdef inline + dynamic

	writePropertyName(type ? type : "");

	_references.emplace_back(AMF::AMF3_OBJECT);
	return (_references.size() << 1) | 1;
}

UInt64 AMFWriter::beginArray(UInt32 size) {
	_levels.push_back(_amf3);
	if(!_amf3) {
		if(amf0) {
			writer.write8(AMF::AMF0_STRICT_ARRAY);
			writer.write32(size);
			return (++_amf0References)<<1;
		}
		writer.write8(AMF::AMF0_AMF3_OBJECT);
		_amf3=true;
	}

	writer.write8(AMF::AMF3_ARRAY);
	writer.write7Bit<UInt32>((size << 1) | 1, 4);
	writer.write8(01); // end marker, no properties (pure array)
	_references.emplace_back(AMF::AMF3_ARRAY);
	return (_references.size()<<1) | 1;
}

UInt64 AMFWriter::beginObjectArray(UInt32 size) {
	_levels.push_back(_amf3); // endObject
	if(!_amf3) {
		if (amf0)
			WARN("Mixed object in AMF0 are not supported, switch to AMF3");
		writer.write8(AMF::AMF0_AMF3_OBJECT);
		_amf3=true;
	}
	_levels.push_back(_amf3); // endArray
	writer.write8(AMF::AMF3_ARRAY);
	writer.write7Bit<UInt32>((size << 1) | 1, 4);
	_references.emplace_back(AMF::AMF3_ARRAY);
	return (_references.size()<<1) | 1;
}

UInt64 AMFWriter::beginMap(Exception& ex, UInt32 size,bool weakKeys) {
	_levels.push_back(_amf3);
	if(!_amf3) {
		if (amf0)
			WARN("Impossible to write a map in AMF0, switch to AMF3");
		writer.write8(AMF::AMF0_AMF3_OBJECT);
		_amf3=true;
	}
	writer.write8(AMF::AMF3_DICTIONARY);
	writer.write7Bit<UInt32>((size << 1) | 1, 4);
	writer.write8(weakKeys ? 0x01 : 0x00);
	_references.emplace_back(AMF::AMF3_DICTIONARY);
	return (_references.size()<<1) | 1;
}

void AMFWriter::endComplex(bool isObject) {
	if(_levels.empty()) {
		ERROR("endComplex called without beginComplex calling");
		return;
	}

	if(isObject) {
		if(!_amf3) {
			writer.write16(0); 
			writer.write8(AMF::AMF0_END_OBJECT);
		} else
			writer.write8(01); // end marker
	}

	_amf3 = _levels.back();
	_levels.pop_back();
}


Media::Data::Type AMFWriter::convert(Media::Data::Type type, Packet& packet) {
	Media::Data::Type amfType = amf0 ? Media::Data::TYPE_AMF0 : Media::Data::TYPE_AMF;
	if (!packet || type == amfType)
		return amfType;
	unique<DataReader> pReader(Media::Data::NewReader(type, packet));
	if (pReader)
		pReader->read(self); // Convert to AMF
	else
		writeByte(packet); // Write Raw
	packet = nullptr;
	return amfType;
}



} // namespace Mona
