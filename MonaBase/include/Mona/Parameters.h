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

struct Parameters : String::Object<Parameters> {
	typedef Event<void(const std::string& key, const std::string* pValue)> ON(Change);
	typedef Event<void()>												   ON(Clear);
	typedef Event<const std::string*(const std::string& key)>			   ON(Unfound);
	
	typedef std::map<std::string, std::string, String::IComparator>::const_iterator const_iterator;
	typedef std::map<std::string, std::string, String::IComparator>::key_compare key_compare;

	struct ForEach {
		ForEach(const const_iterator& begin, const const_iterator& end) : _begin(begin), _end(end) {}
		const_iterator		begin() const { return _begin; }
		const_iterator		end() const { return _end; }
	private:
		const_iterator  _begin;
		const_iterator  _end;
	};

	Parameters() {}
	Parameters(Parameters&& other) { setParams(std::move(other));  }
	Parameters& setParams(Parameters&& other);

	const Parameters& parameters() const { return self; }

	const_iterator	begin() const { return params().begin(); }
	const_iterator	end() const { return params().end(); }
	const_iterator  lower_bound(const std::string& key) const { return params().lower_bound(key); }
	const_iterator  find(const std::string& key) const { return params().find(key); }
	ForEach			from(const std::string& prefix) const { return ForEach(params().lower_bound(prefix), params().end()); }
	ForEach			range(const std::string& prefix) const;
	UInt32			count() const { return params().size(); }


	const Time&		timeChanged() const { return _timeChanged; }


	Parameters&		clear(const std::string& prefix = String::Empty());

	/*!
	Return false if key doesn't exist (and don't change 'value'), otherwise return true and assign string 'value' */
	bool		getString(const std::string& key, std::string& value) const;
	/*!
	A short version of getString with default argument to get value by returned result */
	const char* getString(const std::string& key, const char* defaultValue = NULL) const;

	/*!
	Return false if key doesn't exist or if it's not a numeric type, otherwise return true and assign numeric 'value' */
	template<typename Type>
	bool getNumber(const std::string& key, Type& value) const { STATIC_ASSERT(std::is_arithmetic<Type>::value); const std::string* pValue = getParameter(key); return pValue && String::ToNumber(*pValue, value); }
	/*!
	A short version of getNumber with template default argument to get value by returned result */
	template<typename Type = double, int defaultValue = 0>
	Type getNumber(const std::string& key) const { STATIC_ASSERT(std::is_arithmetic<Type>::value); Type result((Type)defaultValue); getNumber(key, result); return result; }

	/*!
	Return false if key doesn't exist or if it's not a boolean type, otherwise return true and assign boolean 'value' */
	bool getBoolean(const std::string& key, bool& value) const;
	/*! A short version of getBoolean with template default argument to get value by returned result */
	template<bool defaultValue=false>
	bool getBoolean(const std::string& key) const { bool result(defaultValue); getBoolean(key, result); return result; }

	bool hasKey(const std::string& key) const { return getParameter(key) != NULL; }

	bool erase(const std::string& key);
	const_iterator erase(const_iterator first, const_iterator last);
	const_iterator erase(const_iterator it) { const_iterator last(it);  return erase(it, ++last); }

	const std::string& setString(const std::string& key, const std::string& value) { return setParameter(key, value); }
	const std::string& setString(const std::string& key, const char* value, std::size_t size = std::string::npos) {
		return size == std::string::npos ? setParameter(key, value) : setParameter(key, std::string(value, size));
	}

	template<typename Type>
	Type setNumber(const std::string& key, Type value) { STATIC_ASSERT(std::is_arithmetic<Type>::value); setParameter(key, String(value)); return value; }

	bool setBoolean(const std::string& key, bool value) { setParameter(key, value ? "true" : "false");  return value; }


	const std::string* getParameter(const std::string& key) const;
	template<typename ValueType>
	const std::string& setParameter(const std::string& key, ValueType&& value) { return emplace(key, std::forward<ValueType>(value)).first->second; }
	/*!
	Just to match STD container (see MapWriter) */
	template<typename ValueType>
	std::pair<const_iterator, bool> emplace(const std::string& key, ValueType&& value) {
		if (!_pMap)
			_pMap.set();
		const auto& it = _pMap->emplace(key, std::string());
		if (it.second || it.first->second.compare(value) != 0) {
			it.first->second = std::forward<ValueType>(value);
			onParamChange(it.first->first, &it.first->second);
		}
		return it;
	}


	static const Parameters& Null() { static Parameters Null(nullptr); return Null; }

protected:
	Parameters(const Parameters& other) { setParams(other); }
	Parameters& setParams(const Parameters& other);

	virtual void onParamChange(const std::string& key, const std::string* pValue) { _timeChanged.update(); onChange(key, pValue); }
	virtual void onParamClear() { _timeChanged.update(); onClear(); }

private:
	virtual const std::string* onParamUnfound(const std::string& key) const { return onUnfound(key); }
	virtual void onParamInit() {}

	const std::map<std::string, std::string, String::IComparator>& params() const { if (!_pMap) ((Parameters&)self).onParamInit(); return _pMap ? *_pMap : *Null()._pMap; }


	Parameters(std::nullptr_t) : _pMap(SET) {} // Null()

	Time _timeChanged;

	// shared because a lot more faster than using st::map move constructor!
	// Also build _pMap just if required, and then not erase it but clear it (more faster that reset the shared)
	shared<std::map<std::string, std::string, String::IComparator>>	_pMap;
};



} // namespace Mona

