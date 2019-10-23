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

#include "Mona/AMFReader.h"
#include "Mona/StringWriter.h"
#include "Mona/Logs.h"
#include "Mona/Exceptions.h"

using namespace std;

namespace Mona {



AMFReader::AMFReader(const Packet& packet) : ReferableReader(packet),_amf3(0),_referencing(true) {

}

void AMFReader::reset() {
	DataReader::reset();
	_stringReferences.clear();
	_classDefReferences.clear();
	_references.clear();
	_amf0References.clear();
	_amf3 = 0;
	_referencing = true;
}

const char* AMFReader::readText(UInt32& size) {
	UInt32 reset(0), reference(0);
	if (_amf3) {
		reference = reader.position();
		size = reader.read7Bit<UInt32>(4);
		bool isInline = size & 0x01;
		size >>= 1;
		if(!isInline) {
			if (size >= _stringReferences.size()) {
				ERROR("AMF3 string reference not found");
				return NULL;
			}
			reset = reader.position();
			reader.reset(_stringReferences[size]);
			size = (reader.read7Bit<UInt32>(4) >> 1);
		}
	} else
		size = reader.read16();

	if (size > reader.available()) {
		ERROR("AMF text bad-formed with a ", size, " size exceeding the ", reader.available(), " bytes available");
		if (reset)
			reader.reset(reset);
		return NULL;
	}
	const char* text(STR reader.current());

	if (reset)
		reader.reset(reset);
	else if (size) { // not record string empty
		if (reference)
			_stringReferences.emplace_back(reference);
		reader.next(size);
	}
	return text;
}

UInt8 AMFReader::followingType() {

	if(!reader.available())
		return END;
	UInt8 type = *reader.current();
	
	if(_amf3) {
		switch(type) {
			case AMF::AMF3_UNDEFINED:
			case AMF::AMF3_NULL:
				return NIL;
			case AMF::AMF3_FALSE:
			case AMF::AMF3_TRUE:
				return BOOLEAN;
			case AMF::AMF3_INTEGER:
			case AMF::AMF3_NUMBER:
				return NUMBER;
			case AMF::AMF3_STRING:
				return STRING;
			case AMF::AMF3_DATE:
				return DATE;
			case AMF::AMF3_BYTEARRAY:
				return BYTE;
			case AMF::AMF3_ARRAY:
				return ARRAY;
			case AMF::AMF3_DICTIONARY:
				return MAP;
			case AMF::AMF3_OBJECT:
				return OBJECT;
		}

		ERROR("Unknown AMF3 type ",String::Format<UInt8>("%.2x",type))
		reader.next(reader.available());
		return END;
	}
		
	switch(type) {
		case AMF::AMF0_AMF3_OBJECT:
			reader.next();
			_amf3 = 1;
			return followingType();
		case AMF::AMF0_UNDEFINED:
		case AMF::AMF0_NULL:
			return NIL;
		case AMF::AMF0_BOOLEAN:
			return BOOLEAN;
		case AMF::AMF0_NUMBER:
			return NUMBER;
		case AMF::AMF0_LONG_STRING:
		case AMF::AMF0_STRING:
			return STRING;
		case AMF::AMF0_MIXED_ARRAY:
		case AMF::AMF0_STRICT_ARRAY:
			return ARRAY;
		case AMF::AMF0_DATE:
			return DATE;
		case AMF::AMF0_BEGIN_OBJECT:
		case AMF::AMF0_BEGIN_TYPED_OBJECT:
			return OBJECT;
		case AMF::AMF0_REFERENCE:
			return AMF0_REF;
		case AMF::AMF0_END_OBJECT:
			ERROR("AMF0 end object type without begin object type before")
			reader.next(reader.available());
			return END;
		case AMF::AMF0_UNSUPPORTED:
			WARN("Unsupported type in AMF0 format")
			reader.next(reader.available());
			return END;		
	}

	ERROR("Unknown AMF0 type ", String::Format<UInt8>("%.2x",type))
	reader.next(reader.available());
	return END;
}

bool AMFReader::readOne(UInt8 type, DataWriter& writer) {
	bool resetAMF3(false);
	if (_amf3 == 1) {
		resetAMF3 = true;
		_amf3 = 2;
	}

	bool written(writeOne(type, writer));

	if (resetAMF3)
		_amf3 = 0;

	return written;
}

bool AMFReader::writeOne(UInt8 type, DataWriter& writer) {

	switch (type) {

		case AMF0_REF: {
			reader.next();
			UInt16 reference = reader.read16();
			if(reference>=_amf0References.size()) {
				ERROR("AMF0 reference not found")
				return false;
			}
			if (writeReference(writer, (reference+1) << 1))
				return true;
			UInt32 reset(reader.position());
			reader.reset(_amf0References[reference]);
			bool referencing(_referencing);
			_referencing = false;
			bool written = readNext(writer);
			_referencing = referencing;
			reader.reset(reset);
			return written;
		}

		case STRING:  {
			type = reader.read8();
			UInt32 size(0);
			if (type == AMF::AMF0_LONG_STRING) {
				size = reader.read32();
				if (size > reader.available()) {
					ERROR("AMF long string bad-formed with a ", size, " size exceeding the ", reader.available(), " bytes available");
					return false;
				}
				writer.writeString(STR reader.current(), size);
				reader.next(size);
				return true;
			}
			const char* value(readText(size));
			if (!value)
				return false;
			writer.writeString(value, size);
			return true;
		}

		case BOOLEAN:
			type = reader.read8();
			writer.writeBoolean(_amf3 ? (type == AMF::AMF3_TRUE) : reader.read8()!=0x00);
			return true;

		case NUMBER: {
			type = reader.read8();
			if (!_amf3 || type==AMF::AMF3_NUMBER) {
				writer.writeNumber(reader.readDouble());
				return true;
			}
			// Forced in AMF3 here!
			UInt32 value = reader.read7Bit<UInt32>(4);
			if(value>0xFFFFFFF)
				value-=(1<<29);
			writer.writeNumber(value);
			return true;
		}

		case NIL:
			reader.next();
			writer.writeNull();
			return true;

		case BYTE: {
			reader.next();
			// Forced in AMF3 here!
			UInt32 pos = reader.position();
			UInt32 size = reader.read7Bit<UInt32>(4);
			bool isInline = size&0x01;
			size >>= 1;

			if (size > reader.available()) {
				ERROR("AMF ByteArray bad-formed with a ", size, " size exceeding the ", reader.available(), " bytes available");
				return false;
			}

			if(isInline) {
				if (_referencing) {
					_references.emplace_back(pos);
					writeByte(writer, (_references.size()<<1) | 0x01, Packet(self, reader.current(), size));
				} else
					writer.writeByte(Packet(self, reader.current(), size));
				reader.next(size);
				return true;
			}
			if(!writeReference(writer, ((size+1)<<1) | 0x01)) {
				if(size>=_references.size()) {
					ERROR("AMF3 reference not found")
					return false;
				}
				UInt32 reset = reader.position();
				reader.reset(_references[size]);
				writeByte(writer, ((++size)<<1) | 0x01, Packet(self, reader.current(), reader.read7Bit<UInt32>(4)>>1));
				reader.reset(reset);
			}
			return true;
		}

		case DATE: {
			reader.next();
			Date date;
			if (_amf3) {
				UInt32 pos = reader.position();
				UInt32 flags = reader.read7Bit<UInt32>(4);
				bool isInline = flags & 0x01;
				if (isInline) {
					if (_referencing) {
						_references.emplace_back(pos);
						writeDate(writer, (_references.size()<<1) | 0x01, date = reader.read64());
					} else
						writer.writeDate(date = reader.read64());
					return true;
				}
				flags >>= 1;
				if(!writeReference(writer, ((flags+1)<<1) | 0x01)) {
					if (flags >= _references.size()) {
						ERROR("AMF3 reference not found")
						return false;
					}
					UInt32 reset = reader.position();
					reader.reset(_references[flags]);
					writeDate(writer,((++flags)<<1) | 0x01,date = reader.read64());
					reader.reset(reset);
				}
				return true;
			}
			
			writer.writeDate(date = reader.read64());
			reader.next(2); // Timezone, useless
			return true;
		}

		case MAP: {
			reader.next();
			// AMF3
			UInt32 reference = reader.position();
			UInt32 size = reader.read7Bit<UInt32>(4);
			bool isInline = size&0x01;
			size >>= 1;


			UInt32 reset(0);
			bool referencing(_referencing);
			Reference* pReference(NULL);
			Exception ex;

			if (!isInline) {
				if (writeReference(writer, (((reference=size)+1)<<1) | 0x01))
					return true;
				if (size >= _references.size()) {
					ERROR("AMF3 map reference not found")
					return false;
				}
				reset = reader.position();
				reader.reset(_references[reference]);
				size = reader.read7Bit<UInt32>(4) >> 1;
				pReference = beginMap(writer,((++reference)<<1) | 0x01, ex, size, reader.read8() & 0x01);
				_referencing = false;
			} else if (_referencing) {
				_references.emplace_back(reference);
				pReference = beginMap(writer,(_references.size()<<1) | 0x01,ex, size, reader.read8() & 0x01);
			} else
				writer.beginMap(ex, size, reader.read8() & 0x01);

			if (ex)
				WARN(ex);

			while (size-- > 0) {
				if (ex) {
					_buffer.clear();
					StringWriter<string> stringWriter(_buffer);
					if (!readNext(stringWriter))
						continue;
					writer.writePropertyName(_buffer.c_str());
				} else if (!readNext(writer)) // key
					writer.writeNull();

				if (!readNext(writer)) // value
					writer.writeNull();
			}

			endMap(writer,pReference);

			if (reset)
				reader.reset(reset);

			_referencing = referencing;

			return true;

		}

		case ARRAY: {

			UInt32 size(0);
			const char* text(NULL);
			UInt32 reset(0);
			bool referencing(_referencing);
			Reference* pReference(NULL);
			UInt32 reference(0);

			if(!_amf3) {

				type = reader.read8();

				if (_referencing) {
					_amf0References.emplace_back(reader.position());
					reference = _amf0References.size();
				}
				size = reader.read32();

				if (type == AMF::AMF0_MIXED_ARRAY) {
					
					// skip the elements
					_referencing = false;
					UInt32 position(reader.position());
					UInt32 sizeText;
					UInt32 i;
					for (i = 0; i < size; ++i) {
						reset = reader.position();
						text = readText(sizeText);
						if (text) {
							if (!String::ToNumber(text, sizeText, sizeText)) {
								reader.reset(reset);
								size = i; // end of array (can be size=3 with just index 0!)
								break;
							}
							// key (string number, "0")
							readNext(DataWriter::Null()); // value
							if (++sizeText == size) {
								size = ++i;
								break; // anticipate end (next key number exceeds array size)
							}
						} else {
							// Fix beginning
							if (!i)
								position = reader.position();
							--i;
						}
						if (!reader.available()) {
							// Fix size!
							ERROR("AMF array bad-formed with a ", size, " of elements unfound");
							size = ++i; 
							break;
						}
					}
					_referencing = referencing;

					// AMF_MIXED_ARRAY
					pReference = beginObjectArray(writer,reference<<1,size);
		
					// write properties in first
					while ((text = readText(sizeText)) && sizeText) { // property can't be empty
						writer.writePropertyName(_buffer.assign(text,sizeText).c_str());
						if (!readNext(writer))
							writer.writeNull();
					}
					// skip end object marker
					if(reader.read8()!=AMF::AMF0_END_OBJECT)
						ERROR("AMF0 end marker object absent for this mixed array");

					// finalize object part
					endObject(writer,pReference);

					// reset on elements

					reset = reader.position();
					reader.reset(position);

					i = 0;
					while (size--) {
						text = readText(sizeText);
						if (text) {
							String::ToNumber(text, sizeText, sizeText);
							while (sizeText > i++)
								writer.writeNull();
							if (readNext(writer))
								continue;
						}
						writer.writeNull();
					}

					endArray(writer,pReference);

					reader.reset(reset);
			
					_referencing = referencing;
					return true;
				} 
				
				// AMF::AMF0_STRICT_ARRAY
				pReference = beginArray(writer,reference<<1,size);

			} else {
	
				// AMF3
				reader.next();
				
				UInt32 reference = reader.position();
				size = reader.read7Bit<UInt32>(4);
				bool isInline = size&0x01;
				size >>= 1;

				if(!isInline) {
					reference = ((size+1)<<1) | 0x01;
					if (writeReference(writer, reference))
						return true;
					if (size >= _references.size()) {
						ERROR("AMF3 array reference not found")
						return false;
					}
		
					reset = reader.position();
					reader.reset(_references[size]);
					size = reader.read7Bit<UInt32>(4) >> 1;
					_referencing = false;
				} else if (_referencing) {
					_references.emplace_back(reference);
					reference = (_references.size()<<1) | 0x01;
				} else
					reference = 0;

				bool started(false);

				UInt32 sizeText(0);
				while ((text = readText(sizeText)) && sizeText) {  // property can't be empty
					if (!started) {
						pReference = beginObjectArray(writer,reference,size);
						started = true;
					}
					writer.writePropertyName(_buffer.assign(text, sizeText).c_str());
					if (!readNext(writer))
						writer.writeNull();
				}

				if (!started)
					pReference = beginArray(writer,reference, size);
				else
					endObject(writer,pReference);

			}

			while (size-- > 0) {
				if (!readNext(writer))
					writer.writeNull();
			}

			endArray(writer,pReference);

			if (reset)
				reader.reset(reset);

			_referencing = referencing;

			return true;
		}

	}

	// Object

	Reference* pReference(NULL);
	UInt32 reference(0);

	if(!_amf3) {

		///  AMF0

		type = reader.read8();
		if (_referencing) {
			_amf0References.emplace_back(reader.position());
			reference = _amf0References.size() << 1;
		}
		const char* text(NULL);
		UInt32 size(0);
		if(type==AMF::AMF0_BEGIN_TYPED_OBJECT)	 {		
			text = readText(size);
			pReference = beginObject(writer, reference, _buffer.assign(text, size).c_str());
		} else
			pReference = beginObject(writer, reference);
		
		while ((text = readText(size)) && size) {  // property can't be empty
			writer.writePropertyName(_buffer.assign(text, size).c_str());
			if (!readNext(writer))
				writer.writeNull();
		}

		if(reader.read8()!=AMF::AMF0_END_OBJECT)
			ERROR("AMF0 end marker object absent");

		endObject(writer,pReference);

		return true;
	}
	
	///  AMF3
	reader.next();

	UInt32 flags = reader.read7Bit<UInt32>(4);
	UInt32 pos(reader.position());
	UInt32 resetObject(0);
	bool isInline = flags&0x01;
	flags >>= 1;
	bool referencing(_referencing);

	if(!isInline) {
		reference = ((flags+1)<<1) | 0x01;
		if (writeReference(writer, reference))
			return true;
		if (flags >= _references.size()) {
			ERROR("AMF3 object reference not found")
			return false;
		}
		
		resetObject = reader.position();
		reader.reset(_references[flags]);
		flags = reader.read7Bit<UInt32>(4) >> 1;
		_referencing = false;
	} else if (_referencing) {
		_references.emplace_back(pos);
		reference = (_references.size()<<1) | 0x01;
	} else
		reference = 0;

	// classdef reading
	isInline = flags&0x01; 
	flags >>= 1;
	UInt32 reset=0;
	UInt32 size(0);
	const char* text(NULL);
	if(isInline) {
		 _classDefReferences.emplace_back(pos);
		text = readText(size); // type
	} else if(flags<_classDefReferences.size()) {
		reset = reader.position();
		reader.reset(_classDefReferences[flags]);
		flags = reader.read7Bit<UInt32>(4)>>2;
		text = readText(size);
		_referencing = false;
	} else
		ERROR("AMF3 classDef reference not found")

	if (flags & 0x01) {
		// external, support just "flex.messaging.io.ArrayCollection"
		if (reset)
			reader.reset(reset);
		bool result(readNext(writer));
		if (resetObject)
			reader.reset(resetObject); // reset object
		_referencing = referencing;
		return result;
	}

	if (size)
		pReference = beginObject(writer, reference, _buffer.assign(text, size).c_str());
	else
		pReference = beginObject(writer, reference);

	isInline = (flags&0x02) ? true : false; // is dynamic!

	flags>>=2;

	 // Find values position for classdef name properties
	pos = reader.position(); // save name position
	for (UInt32 i = 0; i < flags;++i) {
		if (!readText(size)) {
			// fix flags if error during properties reading
			if (!i)
				pos = reader.position();
			--flags; 
		}
	}
	// Reset classdef to be on values now!
	if (reset) {
		reader.reset(reset);
		reset = 0;
		if (!resetObject)
			_referencing = referencing;
	}

	// Read classdef properties
	while (flags--) {
		reset = reader.position(); // save value position
		// Read property name in classdef
		reader.reset(pos); // reset on name
		while(!(text = readText(size)));
		writer.writePropertyName(_buffer.assign(text, size).c_str());
		pos = reader.position(); // save new name position
		// Read value
		reader.reset(reset); // reset on value
		if (!readNext(writer))
			writer.writeNull();
	}

	
	if (isInline) { // is dynamic
		while ((text = readText(size)) && size) { // property can't be empty
			writer.writePropertyName(_buffer.assign(text, size).c_str());
			if (!readNext(writer))
				writer.writeNull();
		}
	}

	endObject(writer,pReference);

	if (resetObject)
		reader.reset(resetObject); // reset object
	_referencing = referencing;

	return true;
}



} // namespace Mona
