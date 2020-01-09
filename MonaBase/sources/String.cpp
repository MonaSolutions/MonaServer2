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

#include "Mona/String.h"
#include "Mona/Exceptions.h"
#include <limits>
#include <cctype>

using namespace std;

namespace Mona {

#if defined(_WIN32) && (_MSC_VER < 1900)
	 // for setting number of exponent digits to 2
	static int output_exp_old_format(_set_output_format(_TWO_DIGIT_EXPONENT));
#endif

size_t String::Split(const char* value, size_t size, const char* separators, const String::ForEach& forEach, SPLIT_OPTIONS options) {
	const char* it1(value);
	const char* it2(NULL);
	const char* it3(NULL);
	size_t count(0);

	for(;;) {
		if (options & SPLIT_TRIM) {
			while ((*it1 && size) && isspace(*it1)) {
				++it1;
				--size;
			}
		}
		it2 = it1;
		while ((*it2 && size) && !strchr(separators, *it2)) {
			++it2;
			--size;
		}
		it3 = it2;
		if ((options & SPLIT_TRIM) && it3 != it1) {
			--it3;
			while (it3 != it1 && isspace(*it3))
				--it3;
			if (!isspace(*it3))
				++it3;
		}
		if (it3 != it1 || !(options&SPLIT_IGNORE_EMPTY)) {
			String::Scoped scoped(it3);
			if(!forEach(count++, &*it1))
				return string::npos;
		}
		it1 = it2;
		if (!*it1 || !size--)
			break;
		++it1;
	};
	return count;
}

int String::ICompare(const char* data, size_t size, const char* value, size_t count) {
	if (data == value)
		return 0;
	if (!data)
		return -1;
	if (!value)
		return 1;

	int d, v;
	do {
		if (!count--)
			return 0; // no difference until here!
		if (((v = (unsigned char)(*(value++))) >= 'A') && (v <= 'Z'))
			v -= 'A' - 'a';
		if (!size--)
			return -v;
		if (((d = (unsigned char)(*(data++))) >= 'A') && (d <= 'Z'))
			d -= 'A' - 'a';
	} while (d && (d == v));
	return d - v;
}

size_t String::TrimLeft(const char*& value, size_t size) {
	if (size == string::npos)
		size = strlen(value);
	while (size && isspace(*value)) {
		++value;
		--size;
	}
	return size;
}
size_t String::TrimRight(const char* value, size_t size) {
	const char* begin(value);
	if (size == string::npos)
		size = strlen(begin);
	value += size;
	while (value != begin && isspace(*--value))
		--size;
	return size;
}

template<typename Type>
bool String::ToNumber(const char* value, size_t size, Type& result, Math base)  {
	Exception ex;
	return ToNumber<Type>(ex, value, size, result, base);
}

template<typename Type>
bool String::ToNumber(Exception& ex, const char* value, size_t size, Type& result, Math base) {
	STATIC_ASSERT(is_arithmetic<Type>::value);
	if (base > 36) {
		ex.set<Ex::Format>(base, " is impossible to represent with ascii table, maximum base is 36");
		return false;
	}
	bool beginning = true, negative = false;
	long double number(0);
	UInt64 comma(0);

	const char* current(value);
	while(*current && size-->0) {

		if (iscntrl(*current) || *current==' ') {
			if (beginning) {
				++current;
				continue;
			}
			ex.set<Ex::Format>(value, " is not a correct number");
			return false;
		}

		switch (*current) {
			case '-':
				negative = true;
			case '+':
				if (!beginning) {
					ex.set<Ex::Format>(value, " is not a correct number");
					return false;
				}
				beginning = false;
				++current;
				continue;
			case '.':
			case ',':
				if (beginning || comma) {
					ex.set<Ex::Format>(value, " is not a correct number");
					return false;
				}
				comma = 1;
				++current;
				continue;
		}

		if (beginning)
			beginning = false;

		Int8 value = *current - '0';
		if (value > 9) {
			// is letter!
			if (value >= 49)
				value -= 39; // is lower letter
			else if(value>=17)
				value -= 7; // is upper letter
			else {
				ex.set<Ex::Format>(*current, " is not a correct digit");
				return false;
			}
		}
		if(value>=base) {
			ex.set<Ex::Format>(*current, " is not a correct digit in base ", base);
			return false;
		}

		number = number * base + value;
		comma *= base;
		++current;
	}

	if (beginning) {
		ex.set<Ex::Format>("Empty string is not a number");
		return false;
	}

	if (comma)
		number /= comma;

	if (number > numeric_limits<Type>::max()) {
		ex.set<Ex::Format>(value, " exceeds maximum number capacity");
		return false;
	}

	if (negative)
		number *= -1;

	result = (Type)number;
	return true;
}


UInt32 String::FromURI(const char* value, std::size_t count, const ForEachDecodedChar& forEach) {

	const char* begin(value);

	while (count && (count != string::npos || *value)) {

		if (*value == '%') {
			// %
			++value;
			if (count != string::npos)
				--count;
			if (!count || (count == string::npos && !*value)) {
				// syntax error
				if (!forEach('%', false))
					--value;
				return value - begin;
			}

			char hi = toupper(*value);
			++value;
			if (count != string::npos)
				--count;
			if (!count || (count == string::npos && !*value)) {
				// syntax error
				if (forEach('%', false)) {
					if (!forEach(hi, false))
						--value;
				} else
					value -= 2;
				return value - begin;
			}
			char lo = toupper(*value++);
			if (count != string::npos)
				--count;
			if (!isxdigit(lo) || !isxdigit(hi)) {
				// syntax error
				if (forEach('%', false)) {
					if (forEach(hi, false)) {
						if (forEach(lo, false))
							continue;
					} else
						--value;
				} else
					value -= 2;
				return value - begin;
			}
			if (forEach(char((hi - (hi <= '9' ? '0' : '7')) << 4) | ((lo - (lo <= '9' ? '0' : '7')) & 0x0F), true))
				continue;
			return value - 3 - begin;
		}
		if (!forEach(*value, false))
			break;
		++value;
		if (count != string::npos)
			--count;
	}

	return value - begin;
}


#if defined(_WIN32)
const char* String::ToUTF8(const wchar_t* value,char buffer[PATH_MAX]) {
	WideCharToMultiByte(CP_UTF8, 0, value, -1, buffer, PATH_MAX, NULL, NULL);
	return buffer;
}
#endif


void String::ToUTF8(const char* value, size_t size, const String::OnEncoded& onEncoded) {
	const char* begin(value);
	char newValue[2];

	for(;;) {
		if (size != string::npos) {
			if (!size--)
				break;
		} else if (!*value)
			break;

		if (ToUTF8(*value, newValue)) {
			++value;
			continue;
		}

		if (value > begin)
			onEncoded(begin, value - begin);
		onEncoded(STR newValue, 2);

		begin = ++value;
	}

	if (value > begin)
		onEncoded(begin, value - begin);
}

const char* String::ShortPath(const string& path) {
	const char* cur(path.c_str() + path.size());
	const char* name = NULL;
	while (cur-- > path.c_str()) {
		if (*cur == '/' || *cur == '\\') {
			if (name) // end!
				break;
			name = cur;
		}
	}
	++cur;
	if (name && (String::ICompare(cur, name - cur, "sources") == 0 || String::ICompare(cur, name - cur, "mona") == 0))
		return name + 1;
	return cur;
}

bool String::ToUTF8(char value, char (&buffer)[2]) {
	if (value >=0)
		return true;
	buffer[0] = (unsigned char)(((value >> 6) & 0x1F) | 0xC0);
	buffer[1] = (unsigned char)((value & 0x3F) | 0x80);
	return false;
}

template bool  String::ToNumber(const char*, size_t, float&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, float&, Math base);
template bool  String::ToNumber(const char*, size_t, double&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, double&, Math base);
template bool  String::ToNumber(const char*, size_t, unsigned char&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, unsigned char&, Math base);
template bool  String::ToNumber(const char*, size_t, char&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, char&, Math base);
template bool  String::ToNumber(const char*, size_t, short&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, short&, Math base);
template bool  String::ToNumber(const char*, size_t, unsigned short&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, unsigned short&, Math base);
template bool  String::ToNumber(const char*, size_t, int&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, int&, Math base);
template bool  String::ToNumber(const char*, size_t, unsigned int&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, unsigned int&, Math base);
template bool  String::ToNumber(const char*, size_t, long&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, long&, Math base);
template bool  String::ToNumber(const char*, size_t, unsigned long&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, unsigned long&, Math base);
template bool  String::ToNumber(const char*, size_t, long long&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, long long&, Math base);
template bool  String::ToNumber(const char*, size_t, unsigned long long&, Math base);
template bool  String::ToNumber(Exception& ex, const char*, size_t, unsigned long long&, Math base);

} // namespace Mona
