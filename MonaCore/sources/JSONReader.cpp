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

#include "Mona/JSONReader.h"
#include "Mona/Logs.h"
#include "Mona/Util.h"
#include <sstream>

using namespace std;

namespace Mona {


JSONReader::JSONReader(const Packet& packet) : _pos(reader.position()), DataReader(packet), _isValid(false) {

	// check first '[' and last ']' or '{ and '}'

	const UInt8* cur = current();
	if (!cur)
		return;
	bool isArray(*cur=='[');
	if (!isArray) {
		if (*cur != '{')
			return;
	} else
		reader.next();

	_pos = reader.position();

	if (!(cur = current()))
		return;

	const UInt8* end(cur + reader.available());
	
	while (end-- > cur) {
		if (!isspace(*end)) {
			if (isArray) {
				if (*end == ']') {
					_isValid = true;
					reader.shrink(end-cur);
				}
			} else if (*end == '}')
				_isValid = true;
			return;
		}
	}
}


UInt8 JSONReader::followingType() {
	if (!_isValid)
		return END;

	const UInt8* cur = current();
	if(!cur)
		return END;

	if (*cur == ',') {
		// avoid two following comma
		reader.next();
		cur = current();
		if (!cur) {
			ERROR("JSON malformed, no end");
			return END;
		}
		if (*cur == ',' || *cur == '}' || *cur == ']') {
			ERROR("JSON malformed, double comma without any value between");
			reader.next(reader.available());
			return END;
		}
		return followingType();
	}

	if (*cur == '{')
		return OBJECT;

	if(*cur=='[')
		return ARRAY;

	if(*cur=='}' || *cur==']') {
		reader.next(reader.available());
		ERROR("JSON malformed, marker ",*cur," without beginning");
		return END;
	}

	
	if(*cur=='"') {
		const char* value(jumpToString(_size));
		if (!value)
			return END;
		Exception ex;
		if (_date.update(ex,value,_size))
			return DATE;
		return STRING;
	}

	// necessary one char
	UInt32 available(reader.available());
	_size = 0;
	const char* value((const char*)cur);
	do {
		--available;
		++_size;
		++cur;
	} while (available && *cur != ',' && *cur != '}' && *cur != ']');

	if (_size == 4) {
		if (String::ICompare(value, EXPAND("true")) == 0) {
			_number = 1;
			return BOOLEAN;
		}
		if(String::ICompare(value, EXPAND("null")) == 0)
			return NIL;
	} else if (_size == 5 && String::ICompare(value, EXPAND("false")) == 0) {
		_number = 0;
		return BOOLEAN;
	}
	if (String::ToNumber(value, _size, _number))
		return NUMBER;

	ERROR("JSON malformed, unknown ",string(value,_size)," value");
	reader.next(reader.available());
	return END;
}


bool JSONReader::readOne(UInt8 type, DataWriter& writer) {

	switch (type) {

		case STRING:
			reader.next(1); // skip "
			writer.writeString(STR reader.current(), _size);
			reader.next(_size+1); // skip string"
			return true;

		case NUMBER:
			reader.next(_size);
			writer.writeNumber(_number);
			return true;

		case BOOLEAN:
			reader.next(_size);
			writer.writeBoolean(_number!=0);
			return true;

		case DATE:
			reader.next(_size+2); // "date"
			writer.writeDate(_date);
			return true;

		case NIL:
			reader.next(4); // null
			writer.writeNull();
			return true;

		case ARRAY: {
			reader.next(); // skip [
			// count number of elements
			UInt32 count(0);
			countArrayElement(count);
			// write array
			writer.beginArray(count);
			while (count-- > 0) {
				if(!readNext(writer))
					writer.writeNull();
			}
			writer.endArray();
			// skip ]
			current();
			reader.next();

			return true;
		}
	}

	// Object
	reader.next(); // skip {

	bool started(false);
	const UInt8* cur(NULL);
	DataWriter* pWriter = &writer;

	while ((cur=current()) && *cur != '}') {

		if (started) {
			if (*cur != ',') {
				// skip comma , (2nd iteration)
				ERROR("JSON malformed, comma object separator absent");
				reader.next(reader.available());
				pWriter->endObject();
				return true;
			}
			reader.next();
			cur = current();
			if (!cur)
				break;
		}

		const char* name(jumpToString(_size));
		if (!name) {
			if (!started)
				return  false;
			pWriter->endObject();
			return true;
		}
		reader.next(2+_size); // skip "string"

		if (!jumpTo(':')) {
			if (!started)
				return  false;
			pWriter->endObject();
			return true;
		}
		reader.next();

		// write key
		if (!started) {
			if (!(cur = current())) {
				ERROR("JSON malformed, value object of property ", String::Data(name, _size)," absent");
				return false;
			}
			if (*cur == '"') {

				if (_size == 6 && String::ICompare(name, EXPAND("__type"))==0) {
					UInt32 size;
					const char* value(jumpToString(size));
					if (!value)
						return false;
					reader.next(2+size); // skip "string"
					String::Scoped scoped(value + size);
					pWriter->beginObject(value);
					started = true;
					continue;
				}
				
				if (_size == 5 && String::ICompare(name, EXPAND("__bin"))==0) {
					UInt32 size;
					const char* value(jumpToString(size));
					if (!value)
						return false;
					shared<Buffer> pBuffer(SET);
					reader.next(2 + size); // skip "data"
					if (!Util::FromBase64(BIN value, size, *pBuffer)) {
						WARN("JSON raw ", String::Data(name, _size), " data must be in a base64 encoding format to be acceptable");
						pWriter->writeByte(Packet(self, BIN value, size));
					} else
						pWriter->writeByte(Packet(pBuffer));
					pWriter = &DataWriter::Null();
					continue;
				}
			}

			pWriter->beginObject();
			started = true;
		}

		{
			String::Scoped scoped(name+_size);
			pWriter->writePropertyName(name);
		}
	
		// write value
		if (!readNext(*pWriter)) {
			// here necessary position is at the end of the packet
			pWriter->writeNull();
			pWriter->endObject();
			return true;
		}

	}

	if (!started)
		pWriter->beginObject();
	pWriter->endObject();

	if (cur)
		reader.next(); // skip }
	else
		ERROR("JSON malformed, no object } end marker");

	return true;
}

const char* JSONReader::jumpToString(UInt32& size) {
	if (!jumpTo('"'))
		return NULL;
	const UInt8* cur(reader.current()+1);
	const UInt8* end(cur+reader.available()-1);
	size = 0;
	while (cur<end && *cur != '"') {
		if (*cur== '\\') {
			++size;
			if (++cur == end)
				break;
		}
		++size;
		++cur;
	}
	if(cur==end) {
		reader.next(reader.available());
		ERROR("JSON malformed, marker \" end of text not found");
		return NULL;
	}
	return (const char*)reader.current() + 1;
}

bool JSONReader::jumpTo(char marker) {
	const UInt8* cur(current());
	if (!cur || *cur != marker) {
		ERROR("JSON malformed, marker ", marker , " unfound");
		reader.next(reader.available());
		return false;
	}
	return cur != NULL;
}

const UInt8* JSONReader::current() {
	while(reader.available() && isspace(*reader.current()))
		reader.next(1);
	if(!reader.available())
		return NULL;
	return reader.current();
}


bool JSONReader::countArrayElement(UInt32& count) {
	count = 0;
	const UInt8* cur(current());
	const UInt8* end(cur+reader.available());
	UInt32 inner(0);
	bool nothing(true);
	bool done(false);
	while (cur<end && (inner || *cur != ']')) {

		// skip string
		if (*cur == '"') {
			while (++cur<end && *cur != '"') {
				if (*cur== '\\') {
					if (++cur == end)
						break;
				}
			}
			if(cur==end) {
				reader.next(reader.available());
				ERROR("JSON malformed, marker \" end of text not found");
				return false;
			}
			nothing = false;
		} else if (*cur == '{' || *cur == '[') {
			++inner;
			nothing = true;
		} else if (*cur == ']' || *cur == '}') {
			if (inner == 0) {
				reader.next(reader.available());
				ERROR("JSON malformed, marker ", *cur, " without beginning");
				return false;
			}
			--inner;
			nothing = false;
		} else if (*cur == ',') {
			if (nothing) {
				reader.next(reader.available());
				ERROR("JSON malformed, comma separator without value before");
				return false;
			}
			nothing = true;
			if(inner==0)
				done = false;
		} else if (!isspace(*cur))
			nothing = false;

		if (inner == 0 && !done && !nothing) {
			++count;
			done = true;
		}
		++cur;
	}
	if (cur==end) {
		ERROR("JSON malformed, marker ] end of array not found");
		return false;
	}
	return true;
}

} // namespace Mona
