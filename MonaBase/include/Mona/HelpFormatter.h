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
#include "Mona/Options.h"
#include <ostream>

namespace Mona {

struct HelpFormatter : virtual Static {
	static std::ostream&  Format(std::ostream& ostr, const char* command, const Options& options = Options::Null()) { return Format(ostr, command, NULL, NULL, NULL, options); }
	static std::ostream&  Format(std::ostream& ostr, const char* command, const char* usage, const Options& options = Options::Null()) { return Format(ostr, command, usage, NULL, NULL, options); }
	static std::ostream&  Format(std::ostream& ostr, const char* command, const char* usage, const char* header, const Options& options = Options::Null()) { return Format(ostr, command, usage, header, NULL, options); }
	static std::ostream&  Format(std::ostream& ostr, const char* command, const char* usage, const char* header, const char* footer, const Options& options = Options::Null());
		/// Writes the formatted help text to the given stream.

private:

	static int CalcIndent(const Options& options);
	/// Calculates the indentation for the option descriptions
	/// from the given options.

	static void FormatOption(std::ostream& ostr, const Option& option, int indent);
	/// Formats an option, using the platform-specific
	/// prefixes.

	static void FormatText(std::ostream& ostr, const char* text, int indent, int firstIndent);
	/// Formats the given text.

	static void FormatWord(std::ostream& ostr, int& pos, std::string& word, int indent);
	/// Formats the given word.

};



} // namespace Mona
