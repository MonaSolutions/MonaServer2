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

#include "Mona/QueryReader.h"
#include "Mona/Logs.h"
#include "Mona/URL.h"
#include <sstream>

using namespace std;

namespace Mona {

UInt8 QueryReader::followingType() {
	if (_type)
		return _type;

	bool hasProperty(false);
	URL::ForEachParameter forEach([this,&hasProperty](string& key, const char* value) {
		if (value) {
			_property = std::move(key);
			hasProperty = true;
			_value.assign(value);
		} else
			_value = std::move(key);
		return false; // we just want the first following key
	});

	if (URL::ParseQuery(STR reader.current(), reader.available(), forEach) == 0)
		return END;

	if (hasProperty)
		_type = OBJECT;
	else
		_type = ParseValue(_value.data(), _value.size(), _number);
	
	return _type;
}


bool QueryReader::readOne(UInt8 type, DataWriter& writer) {
	
	const UInt8* cur = reader.current();
	const UInt8* end = cur+reader.available();
	if (type==OBJECT) {

		// OBJECT
		writer.beginObject();
		do {
			cur = reader.current();
			writer.writeProperty(_property.c_str(), _value.data(), _value.size());

			// next!
			while (cur<end && *cur != '&')
				cur++;
			reader.next(cur-reader.current() + (*cur=='&'));

			_type = END;
		} while (cur<end && (_type=followingType())==OBJECT);
		writer.endObject();
		return true;
	}

	writer.writeValue(type, _value.data(), _value.size(), _number);

	// next!
	while (cur<end && *cur != '&')
		cur++;
	reader.next(cur-reader.current() + (*cur=='&'));
	_type = END;
	return true;
}


} // namespace Mona
