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
#include "Mona/Option.h"
#include "Mona/Exceptions.h"
#include <set>

namespace Mona {

struct Options : virtual Object {
	NULLABLE(!count())

	typedef std::set<Option>::const_iterator const_iterator;

	Options() : ignoreUnknown(false) {}

	bool ignoreUnknown;

	template <typename ...Args>
	Option& add(Exception& ex, const char* fullName, const char* shortName, Args&&... args) {
        if (strlen(fullName)==0) {
			ex.set<Ex::Application::Argument>("Invalid option (fullName is empty)");
			return Option::Null();
		}
        if (strlen(shortName) == 0) {
			ex.set<Ex::Application::Argument>("Invalid option (shortName is empty)");
			return Option::Null();
		}
		const auto& result = _options.emplace(fullName, shortName, std::forward<Args>(args)...);
		if (!result.second) {
			ex.set<Ex::Application::Argument>("Option ", fullName, " (", shortName, ") duplicated with ", result.first->fullName(), " (", result.first->shortName(), ")");
			return Option::Null();
		}
		return const_cast<Option&>(*result.first);
	}

	void			remove(const std::string& name) { _options.erase(Option(name.c_str(), "")); }
	void			clear() { _options.clear(); }

	const Option&	get(const std::string& name) const;

	const_iterator	begin() const { return _options.begin(); }
	const_iterator	end() const { return _options.end(); }

	UInt32			count() const { return _options.size(); }
	bool			empty() const { return _options.empty(); }

	typedef std::function<void(const std::string&, const char*)> ForEach;
    bool			process(Exception& ex, int argc, const char* argv[], const ForEach& forEach = nullptr);

	static const Options& Null() { static Options Options; return Options; }
private:
	bool			process(Exception& ex, const char* argument, const ForEach& forEach, std::set<std::string, String::IComparator>& alreadyReaden);
	void			handleOption(const std::string& name,const std::string& value) {}
	
	std::set<Option>	_options;
};



} // namespace Mona
