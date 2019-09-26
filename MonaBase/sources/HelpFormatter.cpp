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


#include "Mona/HelpFormatter.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

#define TAB_WIDTH		4
#define WIDTH			78

#define LONG_PREFIX		"--"
#define SHORT_PREFIX	"-"


int HelpFormatter::CalcIndent(const Options& options) {
	int indent = 0;
	for (const Option& option : options) {
		int shortLen = option.shortName().length();
		int fullLen = option.fullName().length();
		int n = TAB_WIDTH;
#if !defined(_WIN32)
        n += shortLen + sizeof(SHORT_PREFIX) + 2;
		if (option.takesArgument())
			n += option.argumentName().length() + (option.argumentRequired() ? 0 : 2);
#endif
		n += fullLen + sizeof(LONG_PREFIX) + 1;
		if (option.takesArgument())
			n += 1 + option.argumentName().length() + (option.argumentRequired() ? 0 : 2);
		if (n > indent)
			indent = n;
	}
	return indent;
}

ostream& HelpFormatter::Format(ostream& ostr, const Description& description) {
	int indent = CalcIndent(description.options);
	if (description.header) {
		FormatText(ostr, description.header, 0, indent);
		ostr << "\n\n";
	}
	ostr << "usage: " << description.usage << '\n';
	if (description.options)
		ostr << "options:\n";
	for (const Option& option : description.options) {
		FormatOption(ostr, option, indent);
		FormatText(ostr, option.description().c_str(), indent, indent);
		ostr << '\n';
	}
	if (description.footer) {
		ostr << '\n';
		FormatText(ostr, description.footer, 0, indent);
		ostr << '\n';
	}
	return ostr;
}

void HelpFormatter::FormatOption(ostream& ostr, const Option& option, int indent) {

	int n = TAB_WIDTH;
	for(UInt8 i=0; i<TAB_WIDTH; ++i)
		ostr << ' ';

    ostr << SHORT_PREFIX << option.shortName();
    n += sizeof(SHORT_PREFIX) + option.shortName().length();
	if (option.takesArgument()) {
		if (!option.argumentRequired()) {
			ostr << '[';
			++n;
		}
		ostr << option.argumentName();
		n += option.argumentName().length();
		if (!option.argumentRequired()) {
			ostr << ']';
			++n;
		}
	}

	ostr << ", " << LONG_PREFIX << option.fullName();
	n += sizeof(LONG_PREFIX)+option.fullName().length();
	if (option.takesArgument()) {
		if (!option.argumentRequired()) {
			ostr << '[';
			++n;
		}
		ostr << '=';
		++n;
		ostr << option.argumentName();
		n += option.argumentName().length();
		if (!option.argumentRequired()) {
			ostr << ']';
			++n;
		}
	}

	do {
		ostr << ' ';
	} while (++n < indent);
}


void HelpFormatter::FormatText(ostream& ostr, const char* text, int indent, int firstIndent) {
	int pos = firstIndent;
	size_t maxWordLen = WIDTH - indent;
	string word;
	while (*text) {
		switch (*text) {
			case '\n':
				FormatWord(ostr, pos, word, indent);
				ostr << '\n';
				pos = 0;
				while (pos < indent) {
					ostr << ' ';
					++pos;
				}
				break;
			case '\t':
				FormatWord(ostr, pos, word, indent);
				if (pos < WIDTH) ++pos;
				while (pos < WIDTH && pos % TAB_WIDTH != 0) {
					ostr << ' ';
					++pos;
				}
				break;
			case ' ':
				FormatWord(ostr, pos, word, indent);
				if (pos < WIDTH) {
					ostr << ' ';
					++pos;
				}
				break;
			default:
				if (word.length() == maxWordLen)
					FormatWord(ostr, pos, word, indent);
				else
					word += *text;

		}
		++text;
	}
	FormatWord(ostr, pos, word, indent);
}

void HelpFormatter::FormatWord(ostream& ostr, int& pos, string& word, int indent) {
	if ((pos + word.length()) > WIDTH) {
		ostr << '\n';
		pos = 0;
		while (pos < indent) {
			ostr << ' ';
			++pos;
		}
	}
	ostr << word;
	pos += word.length();
	word.clear();
}


} // namespace Mona
