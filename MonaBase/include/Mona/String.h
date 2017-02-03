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
#include "Mona/Buffer.h"
#include <functional>
#include <vector>
#include <map>
#include <set>

#undef max

namespace Mona {

#define EXPAND(VALUE)			VALUE"",(sizeof(VALUE)-1) // "" concatenation is here to check that it's a valid const string is not a pointer of char*
#define CONST_STRING(STRING)	operator const std::string&() const { return STRING;} \
								std::size_t length() const { return (STRING).length(); } \
								bool	  empty() const { return (STRING).empty(); } \
								const char& operator[] (std::size_t pos) const { return (STRING)[pos]; } \
								const char& back() const { return (STRING).back(); } \
								const char& front() const { return (STRING).front(); } \
								const char* c_str() const { return (STRING).c_str(); }

struct Exception;

typedef UInt8 SPLIT_OPTIONS;
enum {
	SPLIT_IGNORE_EMPTY = 1, /// ignore empty tokens
	SPLIT_TRIM = 2,  /// remove leading and trailing whitespace from tokens
};

/// Utility class for generation parse of strings
struct String : std::string {
	template <typename ...Args>
	String(Args&&... args) {
		Assign<std::string>(*this, std::forward<Args>(args)...);
	}

	static const std::string& Empty() { static std::string Empty; return Empty; }

	template<typename ValueType>
	struct Format : virtual Object {
		Format(const char* format, const ValueType& value) : value(value), format(format) {}
		const ValueType&	value;
		const char*			format;
	};


	template<typename KeyType>
	struct IComparator { bool operator()(const KeyType& a, const KeyType& b) const { return String::ICompare(a, b)<0; } };
	template<typename KeyType, typename ValueType>
	struct Map : std::map<KeyType, ValueType, IComparator<KeyType>> {
		using std::map<KeyType, ValueType, IComparator<KeyType>>::map;
	};
	template<typename ValueType>
	struct Set : std::set<ValueType, IComparator<ValueType>> {
		using std::set<ValueType, IComparator<ValueType>>::set;
	};

	/*!
	Encode value to UTF8 when required, if the value was already UTF8 compatible returns true, else false */
	static bool ToUTF8(char value, char (&buffer)[2]);
	/*!
	Encode value to UTF8, onEncoded returns concatenate piece of string encoded (to allow no data copy) */
	typedef std::function<void(const char* value, std::size_t size)> OnEncoded;
	static void ToUTF8(const char* value, const String::OnEncoded& onEncoded) { ToUTF8(value, std::string::npos, onEncoded); }
	static void ToUTF8(const std::string& value, const String::OnEncoded& onEncoded) { ToUTF8(value.data(), value.size(), onEncoded); }
	static void ToUTF8(const char* value, std::size_t size, const String::OnEncoded& onEncoded);
	
	typedef std::function<bool(UInt32 index,const char* value)> ForEach; /// String::Split function type handler
	static std::size_t Split(const std::string& value, const char* separators, const String::ForEach& forEach, SPLIT_OPTIONS options = 0) { return Split(value.data(), separators, forEach, options); }
	static std::size_t Split(const char* value, const char* separators, const String::ForEach& forEach, SPLIT_OPTIONS options = 0);
	static std::vector<std::string>& Split(const char* value, const char* separators, std::vector<std::string>& values, SPLIT_OPTIONS options = 0);
	static std::vector<std::string>& Split(const std::string& value, const char* separators, std::vector<std::string>& values, SPLIT_OPTIONS options = 0) { return Split(value.c_str(),separators,values,options); }

	static const char*	TrimLeft(const char* value, std::size_t size = std::string::npos);
	static char*		TrimRight(char* value);
	static std::size_t	TrimRight(const char* value, std::size_t size);
	static char*		Trim(char* value) { TrimLeft(value); return TrimRight(value); }
	static std::size_t	Trim(const char* value, std::size_t size) { TrimLeft(value, size); return TrimRight(value, size); }

	static std::string&	TrimLeft(std::string& value) { return value.erase(0, TrimLeft(value.data(), value.size()) - value.data()); }
	static std::string&	TrimRight(std::string& value) { while (!value.empty() && isspace(value.back())) value.pop_back(); return value; }
	static std::string&	Trim(std::string& value) { TrimLeft(value); return TrimRight(value); }
	
	static std::string&	ToLower(std::string& value) { for (auto it = value.begin(); it != value.end(); ++it) *it = tolower(*it); return value; }

	static int ICompare(const char* value1, const char* value2,  std::size_t size = std::string::npos);
	static int ICompare(const std::string& value1, const std::string& value2, std::size_t size = std::string::npos) { return ICompare(value1.empty() ? NULL : value1.c_str(), value2.empty() ? NULL : value2.c_str(), size); }
	static int ICompare(const std::string& value1, const char* value2,  std::size_t size = std::string::npos) { return ICompare(value1.empty() ? NULL : value1.c_str(), value2, size); }
	static int ICompare(const char* value1, const std::string& value2,  std::size_t size = std::string::npos) { return ICompare(value1, value2.empty() ? NULL : value2.c_str(), size); }

	template<typename Type>
	static bool ToNumber(const std::string& value, Type& result) { return ToNumber(value.data(), value.size(), result); }
	template<typename Type>
	static bool ToNumber(const char* value, Type& result) { return ToNumber(value, std::string::npos, result); }
	template<typename Type>
	static bool ToNumber(const char* value, std::size_t size, Type& result);

	template<typename Type>
	static Type ToNumber(Exception& ex, const std::string& value) { return ToNumber<Type>(ex, 0, value.data(),value.size()); }
	template<typename Type>
	static Type ToNumber(Exception& ex, const char* value, std::size_t size = std::string::npos) { return ToNumber<Type>(ex, 0, value, size ); }
	template<typename Type>
	static Type ToNumber(Exception& ex, Type failValue, const std::string& value) { return ToNumber<Type>(ex, failValue, value.data(),value.size()); }
	template<typename Type>
	static Type ToNumber(Exception& ex, Type failValue, const char* value, std::size_t size = std::string::npos);
	template<typename Type>
	static Type ToNumber(Type failValue, const std::string& value) { Exception ex; return ToNumber<Type>(ex, failValue, value.data(), value.size()); }
	template<typename Type>
	static Type ToNumber(Type failValue, const char* value, std::size_t size = std::string::npos) { Exception ex; return ToNumber<Type>(ex, failValue, value, size); }

	static bool IsTrue(const std::string& value) { return IsTrue(value.data(),value.size()); }
	static bool IsTrue(const char* value,std::size_t size=std::string::npos) { return ICompare(value, "1", size) == 0 || String::ICompare(value, "true", size) == 0 || String::ICompare(value, "yes", size) == 0 || String::ICompare(value, "on", size) == 0; }
	static bool IsFalse(const std::string& value) { return IsFalse(value.data(),value.size()); }
	static bool IsFalse(const char* value, std::size_t size = std::string::npos) { return ICompare(value, "0", size) == 0 || String::ICompare(value, "false", size) == 0 || String::ICompare(value, "no", size) == 0 || String::ICompare(value, "off", size) == 0 || String::ICompare(value, "null", size) == 0; }

	template <typename OutType, typename ...Args>
	static OutType& Assign(OutType& out, Args&&... args) {
		out.clear();
		return Append<OutType>(out, std::forward<Args>(args)...);
	}

	/// \brief match "std::string" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, const std::string& value, Args&&... args) {
		return Append<OutType>((OutType&)(OutType&)out.append(value.data(), value.size()), std::forward<Args>(args) ...);
	}
	template <typename OutType, typename ...Args>
	static OutType& Append(String& out, std::string&& value, Args&&... args) { return Append<String>(out, value, std::forward<Args>(args) ...); }
	template <typename OutType, typename ...Args>
	static std::string& Append(std::string& out, std::string&& value, Args&&... args) {
		if (!out.size())
			out = std::move(value);
		else
			(OutType&)out.append(value.data(), value.size());
		return Append<std::string>(out, std::forward<Args>(args)...);
	}

	/// \brief match "const char*" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, const char* value, Args&&... args) {
		return Append<OutType>((OutType&)out.append(value, strlen(value)), std::forward<Args>(args)...);
	}

#if defined(_WIN32)
	/// \brief match "wstring" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, const std::wstring& value, Args&&... args) {
		return Append<OutType>(value.c_str(), std::forward<Args>(args)...);
	}
	
	/// \brief match "const wchar_t*" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, const wchar_t* value, Args&&... args) {
		char buffer[PATH_MAX];
		ToUTF8(value, buffer);
		return Append<OutType>((OutType&)out.append(buffer, strlen(buffer)), std::forward<Args>(args)...);
	}
#endif

	// match le "char" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, char value, Args&&... args) {
		return Append<OutType>((OutType&)out.append(&value,1), std::forward<Args>(args)...);
	}

	// match le "signed char" cas
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, signed char value, Args&&... args) {
		char buffer[8];
		sprintf(buffer, "%hhd", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "short" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, short value, Args&&... args) {
		char buffer[8];
		sprintf(buffer, "%hd", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "int" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, int value, Args&&... args) {
		char buffer[16];
		sprintf(buffer, "%d", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "long" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, long value, Args&&... args) {
		char buffer[32];
		sprintf(buffer, "%ld", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "unsigned char" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, unsigned char value, Args&&... args) {
		char buffer[8];
		sprintf(buffer, "%hhu", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "unsigned short" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, unsigned short value, Args&&... args) {
		char buffer[8];
		sprintf(buffer, "%hu", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "unsigned int" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, unsigned int value, Args&&... args) {
		char buffer[16];
		sprintf(buffer, "%u", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "unsigned long" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, unsigned long value, Args&&... args) {
		char buffer[32];
		sprintf(buffer, "%lu", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "Int64" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, long long value, Args&&... args) {
		char buffer[64];
		sprintf(buffer, "%lld", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "UInt64" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, unsigned long long value, Args&&... args) {
		char buffer[64];
		sprintf(buffer, "%llu", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "float" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, float value, Args&&... args) {
		char buffer[64];
		sprintf(buffer, "%.8g", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "double" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, double value, Args&&... args) {
		char buffer[64];
		sprintf(buffer, "%.16g", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief match "bool" case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, bool value, Args&&... args) {
		if (value)
			return Append((OutType&)out.append(EXPAND("true")), std::forward<Args>(args)...);
		return Append<OutType>((OutType&)out.append(EXPAND("false")), std::forward<Args>(args)...);
	}

	/// \brief match pointer case
	template <typename OutType, typename ...Args>
	static OutType& Append(OutType& out, const void* value, Args&&... args)	{
		char buffer[64];
		sprintf(buffer,"%p", value);
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	/// \brief A usefull form which use snprintf to format out
	///
	/// \param out This is the std::string which to append text
	/// \param value A pair of format text associate with value (ex: pair<char*, double>("%.2f", 10))
	/// \param args Other arguments to append
	template <typename OutType, typename Type, typename ...Args>
	static OutType& Append(OutType& out, const Format<Type>& custom, Args&&... args) {
		char buffer[64];
		try {
            snprintf(buffer, sizeof(buffer), custom.format, custom.value);
		}
		catch (...) {
			return Append<OutType>(out, std::forward<Args>(args)...);
		}
		return Append<OutType>((OutType&)out.append(buffer,strlen(buffer)), std::forward<Args>(args)...);
	}

	
	template <typename OutType>
	static OutType& Append(OutType& out) { return out; }

private:

#if defined(_WIN32)
	static const char* ToUTF8(const wchar_t* value, char buffer[PATH_MAX]);
#endif
};

inline bool operator==( const std::string& left, const char* right) { return left.compare(right) == 0; }
inline bool operator==(const std::string& left, const std::string& right) { return left.compare(right) == 0; }
inline bool operator==(const char* left, const std::string& right) { return right.compare(left) == 0; }
inline bool operator!=(const std::string& left, const char* right) { return left.compare(right) != 0; }
inline bool operator!=(const std::string& left, const std::string& right) { return left.compare(right) != 0; }
inline bool operator!=(const char* left, const std::string& right) { return right.compare(left) != 0; }
inline bool operator<(const std::string& left, const char* right) { return left.compare(right) < 0; }
inline bool operator<(const std::string& left, const std::string& right) { return left.compare(right) < 0; }
inline bool operator<(const char* left, const std::string& right) { return right.compare(left) > 0; }
inline bool operator<=(const std::string& left, const char* right) { return left.compare(right) <= 0; }
inline bool operator<=(const std::string& left, const std::string& right) { return left.compare(right) <= 0; }
inline bool operator<=(const char* left, const std::string& right) { return right.compare(left) >= 0; }
inline bool operator>(const std::string& left, const char* right) { return left.compare(right) > 0; }
inline bool operator>(const std::string& left, const std::string& right) { return left.compare(right) > 0; }
inline bool operator>(const char* left, const std::string& right) { return right.compare(left) < 0; }
inline bool operator>=(const std::string& left, const char* right) { return left.compare(right) >= 0; }
inline bool operator>=(const std::string& left, const std::string& right) { return left.compare(right) >= 0; }
inline bool operator>=(const char* left, const std::string& right) { return right.compare(left) <= 0; }


} // namespace Mona

