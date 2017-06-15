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
#include "Mona/DataReader.h"

namespace Mona {

template<typename MapType>
struct MapReader : DataReader, virtual Object {
	MapReader(const MapType& map) : _done(false), _begin(map.begin()),_it(map.begin()),_end(map.end()) {}
	MapReader(const MapType& map, const typename MapType::const_iterator itBegin) :_done(false), _begin(itBegin), _it(itBegin), _end(map.end()) {}
	MapReader(const MapType& map, const typename MapType::const_iterator itBegin, const typename MapType::const_iterator itEnd) : _done(false), _begin(itBegin), _it(itBegin), _end(itEnd) {}
	template<typename RangeType>
	MapReader(const RangeType& range) : _done(false), _begin(range.begin()), _it(range.begin()), _end(range.end()) {}

	void			reset() { _it = _begin; _done = false; }

private:

	UInt8 followingType() {
		if (_done && _it == _end)
			return END;
		return OTHER;
	}

	bool readOne(UInt8 type, DataWriter& writer) { return readOne(type, writer, String::Empty()); }

	bool readOne(UInt8 type, DataWriter& writer, const std::string& prefix) {
		
		// read all
		_done = true;
		writer.beginObject();
		while (_it != _end) {
			const char* key = _it->first.c_str();
			if (_it->first.compare(0, prefix.size(), prefix) != 0)
				break; // stop sub
	
			if (const char* sub = strchr(key+prefix.size(), '.')) {
				string newPrefix(key, sub - key);
				writer.writePropertyName(newPrefix.data()+prefix.size());
				readOne(type, writer, newPrefix+='.');
				continue;
			}
	
			writer.writePropertyName(key + prefix.size());

			if (String::ICompare(_it->second, "true") == 0)
				writer.writeBoolean(true);
			else if (String::ICompare(_it->second, "false") == 0)
				writer.writeBoolean(false);
			else if (String::ICompare(_it->second, "null") == 0)
				writer.writeNull();
			else {
				double number;
				if (String::ToNumber(_it->second, number))
					writer.writeNumber(number);
				else {
					Exception ex;
					if (_date.update(ex, _it->second))
						writer.writeDate(_date);
					else
						writer.writeString(_it->second.data(),_it->second.size());
				}
			}
			++_it;
		}
		writer.endObject();

		return true;
	}

	Date								_date;
	bool								_done;
	typename MapType::const_iterator	_begin;
	typename MapType::const_iterator	_it;
	typename MapType::const_iterator	_end;
};


} // namespace Mona
