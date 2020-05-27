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

#include "Mona/Parameters.h"

using namespace std;

namespace Mona {

Parameters& Parameters::setParams(const Parameters& other) {
	// clear self!
	clear();
	// copy data
	if (other.count())
		_pMap.set() = *other._pMap;
	// onChange!
	for (auto& it : self)
		onParamChange(it.first, &it.second);
	return self;
}

Parameters& Parameters::setParams(Parameters&& other) {
	// clear self!
	clear();
	// move data
	if(other.count()) {
		_pMap = std::move(other._pMap);
		// clear other
		other.onParamClear();
	}	
	// onChange!
	for (auto& it : self)
		onParamChange(it.first, &it.second);
	return self;
}

Parameters::ForEach Parameters::range(const std::string& prefix) const {
	if(prefix.empty())
		return ForEach(begin(), end());
	string end(prefix);
	end.back() = prefix.back() + 1;
	return ForEach(lower_bound(prefix), lower_bound(end));
}

bool Parameters::getString(const string& key, std::string& value) const {
	const string* pValue = getParameter(key);
	if (!pValue)
		return false;
	value.assign(*pValue);
	return true;
}

const char* Parameters::getString(const string& key, const char* defaultValue) const {
	const string* pValue = getParameter(key);
	return pValue ? pValue->c_str() : defaultValue;
}

bool Parameters::getBoolean(const string& key, bool& value) const {
	const string* pValue = getParameter(key);
	if (!pValue)
		return false;
	value = !String::IsFalse(*pValue); // otherwise considerate the value as true
	return true;
}

const string* Parameters::getParameter(const string& key) const {
	const auto& it = params().find(key);
	if (it != params().end())
		return &it->second;
	return onParamUnfound(key);
}

Parameters& Parameters::clear(const string& prefix) {
	if (!count())
		return self;
	if (prefix.empty()) {
		_pMap.reset();
		onParamClear();
	} else {
		string end(prefix);
		end.back() = prefix.back() + 1;
		erase(_pMap->lower_bound(prefix), _pMap->lower_bound(end));
	}
	return self;
}

bool Parameters::erase(const string& key) {
	// erase
	const auto& it(params().find(key));
	if (it != params().end()) {
		// move key because "key" parameter can be a "it->first" too, and must stay valid for onParamChange call!
		string key(move(it->first));
		_pMap->erase(it);
		if (_pMap->empty())
			clear();
		else
			onParamChange(key, NULL);
		return true;
	}
	return false;
}
Parameters::const_iterator Parameters::erase(const_iterator first, const_iterator last) {
	if (first == begin() && last == end()) {
		clear();
		return end();
	}
	while (first != last) {
		string key(move(first->first));
		_pMap->erase(first++);
		onParamChange(key, NULL);
	}
	return first;
}


} // namespace Mona
