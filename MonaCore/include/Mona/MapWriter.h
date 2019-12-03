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
#include "Mona/Logs.h"

namespace Mona {

template<typename MapType>
struct MapWriter : DataWriter, virtual Object {
	/*!
	Beware on DataWriter::reset map is fully-erased */
	MapWriter(MapType& map) : _layers({{0,0}}), _map(map), _isProperty(false) {}

	UInt64 beginObject(const char* type = NULL) { return beginComplex(); }
	void   writePropertyName(const char* value) { _property = value; _isProperty = true; }
	void   endObject() { endComplex(); }

	UInt64 beginArray(UInt32 size) { return beginComplex(String(size)); }
	void   endArray() { endComplex();  }

	UInt64 beginObjectArray(UInt32 size) { beginComplex(String(size)); _layers.emplace_back(_key.size(), 0); return 0; }

	virtual void writeString(const char* value, UInt32 size) { set(String::Data(value, size)); }
	virtual void writeNumber(double value) { set(value); }
	virtual void writeBoolean(bool value) { set(value);}
	virtual void writeNull() { set(nullptr); }
	virtual UInt64 writeDate(const Date& date) { set(date); return 0; }
	virtual UInt64 writeByte(const Packet& packet) { set(packet); return 0; }
	
	void   reset() {
		_isProperty = false;
		_property.clear();
		_key.clear();
		_layers.assign({ { 0,0 } });
		// Impossible to reset map as was on build, so erase it!
		_map.clear();
	}
private:
	UInt64 beginComplex(std::string&& count=std::string()) {
		_layers.emplace_back(_key.size(), 0);
		if (_layers.size()<3)
			return 0;
		if (_isProperty) {
			_key.append(_property);
			_isProperty = false;
		} else
			String::Append(_key, (++_layers.rbegin())->second++);
		if(!count.empty())
			_map.emplace(_key, std::move(count)); // count of array!
		_key += '.';
		return 0;
	}

	void endComplex() {
		if (_layers.empty()) {
			ERROR("endComplex called without beginComplex calling");
			return;
		}
		_key.resize(_layers.back().first);
		_layers.pop_back();
	}
	
	template <typename Type>
	void set(const Type& data) {
		if (!_isProperty) {
			if (_layers.size() < 2) {
				String key(data);
				if(setKey(key))
					_map.emplace(std::move(key), std::string());
				return;
			}
			String::Assign(_property, _layers.back().second++);
		} else
			_isProperty = false;
		String key(_key, _property);
		if (setKey(key))
			_map.emplace(key, String(data));
	}
	virtual bool setKey(const std::string& key) {
		return true;
	}

	MapType&							   _map;
	std::string							   _property;
	bool								   _isProperty;
	std::vector<std::pair<UInt32, UInt32>> _layers; // keySize + index
	std::string							   _key;
};



} // namespace Mona
