/*
This code is in part based on code from the POCO C++ Libraries,
licensed under the Boost software license :
https://www.boost.org/LICENSE_1_0.txt

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
#include "Mona/String.h"
#include <functional>

namespace Mona {


struct Option : virtual Object {
	CONST_STRING(fullName().empty() ? shortName() : fullName());
	NULLABLE(_fullName.empty() && _shortName.empty())

	static const char* Parse(const char* argument) { return *argument && *argument == '-' && *++argument && (*argument != '-' || *++argument) ? argument : NULL; }

	Option(const char* fullName, const char* shortName);
		/// Creates an option with the given properties.

	Option(const char* fullName, const char* shortName, std::string&& description, bool required = false);
		/// Creates an option with the given properties.

	Option(const char* fullName, const char* shortName, std::string&& description, bool required, const std::string& argName, bool argRequired = false);
		/// Creates an option with the given properties.

	typedef std::function<bool(Exception& ex, const char* value)> Handler;

	Option& handler(const Handler& function);
		/// Sets the handler option
	
	Option& description(const std::string& text);
		/// Sets the description of the option.
		
	Option& required(bool flag);
		/// Sets whether the option is required (flag == true)
		/// or optional (flag == false).

	Option& repeatable(bool flag);
		/// Sets whether the option can be specified more than once
		/// (flag == true) or at most once (flag == false).
		
	Option& argument(const std::string& name, bool required = true);
		/// Specifies that the option takes an (optional or required)
		/// argument.
		
	Option& noArgument();
		/// Specifies that the option does not take an argument (default).

	const std::string& shortName() const { return _shortName; }
		/// Returns the short name of the option.
		
	const std::string& fullName() const { return _fullName; }
		/// Returns the full name of the option.
		
	const std::string& description() const { return _description; }
		/// Returns the description of the option.
		
	bool required() const { return _required; }
		/// Returns true if the option is required, false if not.
	
	bool repeatable() const { return _repeatable; }
		/// Returns true if the option can be specified more than
		/// once, or false if at most once.
	
	bool takesArgument() const { return !_argName.empty(); }
		/// Returns true if the options takes an (optional) argument.
		
	bool argumentRequired() const { return _argRequired; }
		/// Returns true if the argument is required.

	const std::string& argumentName() const { return _argName; }
		/// Returns the argument name, if specified.


	bool operator==(const Option& other) const { return String::ICompare(_fullName, other._fullName) == 0 || String::ICompare(_shortName, other._shortName) == 0; }
	bool operator!=(const Option& other) const { return !operator==(other); }
	bool operator<(const Option& other) const { int cmp = String::ICompare(_shortName, other._shortName); return cmp ? String::ICompare(_fullName, other._fullName) < 0 : 0; }
	bool operator>(const Option& other) const { int cmp = String::ICompare(_shortName, other._shortName); return cmp ? String::ICompare(_fullName, other._fullName) > 0 : 0; }

	static Option& Null() { static Option Option(NULL, NULL); return Option; }
private:
	std::string		_shortName;
	std::string		_fullName;
	std::string		_description;
	bool			_required;
	bool			_repeatable;
	std::string		_argName;
	bool			_argRequired;

	Handler	_handler;

	friend struct Options;
};




} // namespace Mona
