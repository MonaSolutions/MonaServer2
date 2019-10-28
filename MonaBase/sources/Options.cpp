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

#include "Mona/Options.h"


using namespace std;

namespace Mona {


const Option& Options::get(const string& name) const {
	auto result = _options.find(Option(name.c_str(), ""));
	if (result == _options.end())
		return Option::Null();
	return *result;
}

bool Options::process(Exception& ex, int argc, const char* argv[], const ForEach& forEach) {
	set<string, String::IComparator> alreadyReaden;
	for (int i = 1; i < argc; ++i) {
		if (!process(ex, argv[i], forEach, alreadyReaden))
			return false;
	}
	// check if all required option have been gotten!
	for (const Option& option : _options) {
		if (option.required() && alreadyReaden.find(option) == alreadyReaden.end()) {
			ex.set<Ex::Application::Argument>("Option ", option, " required");
			return false;
		}
	}
	return true;
}

bool Options::process(Exception& ex, const char* argument, const ForEach& forEach, set<string, String::IComparator>& alreadyReaden) {
	
	const char* itArg = Option::Parse(argument);
	if (!itArg) {
		ex.set<Ex::Application::Argument>("Malformed ", argument, " argument");
		return false;
	}
	argument = itArg;
	while (*itArg && *itArg != ':' && *itArg != '=')
		++itArg;

	if (argument == itArg) {
		ex.set<Ex::Application::Argument>("Empty option");
		return false;
	}
		

	string name(argument, itArg);
	const Option* pOption = NULL;
	auto itOption = _options.find(Option(name.c_str(),""));
	if (itOption == _options.end()) {
		Option shortOption("", name.c_str());
		for (itOption = _options.begin(); itOption != _options.end(); ++itOption) {
			if (*itOption == shortOption) {
				name = *(pOption = &*itOption); // fix name with full name!
				break;
			}
		}
		if (!pOption && !ignoreUnknown) {
			ex.set<Ex::Application::Argument>("Unknown ", name, " option");
			return false;
		}
	} else
		pOption = &*itOption;

	if (!alreadyReaden.emplace(name).second && (!pOption || !pOption->repeatable())) {
		ex.set<Ex::Application::Argument>("Option ", name, " duplicated");
		return false;
	}

	argument = !*itArg ? NULL : ++itArg;
	if(pOption) {
		if (!argument && pOption->argumentRequired()) {
			ex.set<Ex::Application::Argument>(*pOption, " requires ", pOption->argumentName());
			return false;
		}
		if (pOption->_handler && !pOption->_handler(ex, argument))
			return false;
	}
	if(forEach)
		forEach(name, argument);
	return true;
}



} // namespace Mona
