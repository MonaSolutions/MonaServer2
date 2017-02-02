/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Event.h"

namespace Mona {

struct Parameters : virtual Object {

	typedef Event<void(const std::string& key, const std::string* pValue)> ON(Change);
	typedef Event<void()>												   ON(Clear);

	typedef std::map<std::string, std::string>::const_iterator const_iterator;

private:
	struct ForEach {
		ForEach() : _begin(Null().end()), _end(Null().end()) {}
		ForEach(const_iterator& begin, const_iterator& end) : _begin(begin), _end(end) {}
		const_iterator		begin() const { return _begin; }
		const_iterator		end() const { return _end; }
	private:
		const_iterator  _begin;
		const_iterator  _end;
	};
public:

	Parameters() : _bytes(0) {}
	Parameters(Parameters&& other) { operator=(std::move(other));  }
	Parameters& operator=(Parameters&& other);

	const_iterator	begin() const { return _pMap ? _pMap->begin() : Null().begin(); }
	const_iterator	end() const { return _pMap ? _pMap->end() : Null().end(); }
	ForEach			from(const std::string& prefix) { return _pMap ? ForEach(_pMap->lower_bound(prefix), _pMap->end()) : ForEach(); }
	ForEach			band(const std::string& prefix);
	bool			empty() const { return _pMap ? _pMap->empty() : true; }
	UInt32			count() const { return _pMap ? _pMap->size() : 0; }
	UInt32			bytes() const { return _bytes; };

	void			clear() { if (!_pMap || _pMap->empty()) return; _pMap->clear(); _bytes = 0; onParamClear(); }

	/*!
	Return false if key doesn't exist (and don't change 'value'), otherwise return true and assign string 'value' */
	bool		getString(const std::string& key, std::string& value) const;
	/*!
	A short version of getString with default argument to get value by returned result */
	const char* getString(const std::string& key, const char* defaultValue = NULL) const;

	/*!
	Return false if key doesn't exist or if it's not a numeric type, otherwise return true and assign numeric 'value' */
	template<typename NumberType>
	bool getNumber(const std::string& key, NumberType& value) const { const char* temp = getParameter(key); return temp ? String::ToNumber<NumberType>(temp, value) : false; }
	/*!
	A short version of getNumber with template default argument to get value by returned result */
	template<typename NumberType = double, int defaultValue = 0>
	NumberType getNumber(const std::string& key) const { NumberType result((NumberType)defaultValue); getNumber(key, result); return result; }

	/*!
	Return false if key doesn't exist or if it's not a boolean type, otherwise return true and assign boolean 'value' */
	bool getBoolean(const std::string& key, bool& value) const;
	/*! A short version of getBoolean with template default argument to get value by returned result */
	template<bool defaultValue=false>
	bool getBoolean(const std::string& key) const { bool result(defaultValue); getBoolean(key, result); return result; }

	bool hasKey(const std::string& key) const { return getParameter(key) != NULL; }

	bool erase(const std::string& key);

	const std::string& setString(const std::string& key, const std::string& value) { return setParameter(key, value); }
	const std::string& setString(const std::string& key, const char* value, std::size_t size = std::string::npos) { return setParameter(key, value, size == std::string::npos ? strlen(value) : size); }

	template<typename NumberType>
	NumberType setNumber(const std::string& key, NumberType value) { setParameter(key, String(value)); return value; }

	bool setBoolean(const std::string& key, bool value) { setParameter(key, value ? "true" : "false");  return value; }

	template<typename ...Args>
	const std::string& emplace(Args&& ...args) {
		std::pair<std::string, std::string> item(std::forward<Args>(args)...);
		return setParameter(item.first, std::move(item.second));
	}

	static const Parameters& Null() { static Parameters Null(nullptr); return Null; }

protected:
	virtual void onParamChange(const std::string& key, const std::string* pValue) { onChange(key, pValue); }
	virtual void onParamClear() { onClear(); }

private:
	virtual const char* onParamUnfound(const std::string& key) const { return NULL; }

	Parameters(nullptr_t) : _pMap(new String::Map<std::string, std::string>()) {} // Null()

	const char* getParameter(const std::string& key) const;

	template<typename ...Args>
	const std::string& setParameter(const std::string& key, Args&& ...args) {
		if (!_pMap)
			_pMap.reset(new String::Map<std::string, std::string>());
		auto& it = _pMap->emplace(key, std::string());
		if (!it.second) // already exists
			_bytes -= it.first->second.size();
		_bytes += it.first->second.assign(std::forward<Args>(args)...).size();
		onParamChange(it.first->first, &it.first->second);
		return it.first->second;
	}

	UInt32													_bytes;
	// shared because a lot more faster than using st::map move constructor!
	// Also build _pMap just if required, and then not erase it but clear it (more faster that reset the shared)
	shared<String::Map<std::string, std::string>>	_pMap;
};



} // namespace Mona

