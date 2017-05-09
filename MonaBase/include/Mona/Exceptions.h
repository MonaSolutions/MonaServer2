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
#include "Mona/String.h"
#include "assert.h"
#include <memory>

namespace Mona {

	struct Ex : std::string, virtual NullableObject { struct Application; struct Extern; struct Format; struct Intern; struct Net; struct Permission; struct Protocol; struct System; struct Unavailable; struct Unfound; struct Unsupported;
		explicit operator bool() const { return !empty(); }
	};
	/*!
	Application exception, error from user side */
	struct Ex::Application						: Ex { struct Argument; struct Config; struct Error; struct Invalid; struct Unfound; };
		struct Ex::Application::Argument			: Application {}; // Application argument given error
		struct Ex::Application::Config				: Application {}; // Application configuration error
		struct Ex::Application::Error				: Application {}; // error during application execution
		struct Ex::Application::Invalid				: Application {}; // Invalid
		struct Ex::Application::Unfound				: Application {}; // Application non existent
	/*!
	Extern exception, error from library dependency or from outside code like STD */
	struct Ex::Extern							: Ex { struct Crypto; struct Math; struct Net; };
		struct Ex::Extern::Crypto					: Extern {};
		struct Ex::Extern::Math						: Extern {};
	/*!
	Formatting exception */
	struct Ex::Format							: Ex {};
	/*!
	Intern exception, code/coder error */
	struct Ex::Intern							: Ex {};
	/*!
	Net exception */
	struct Ex::Net								: Ex { struct Address; struct Socket; struct System; };
		struct Ex::Net::Address						: Net { struct Ip; struct Port; };
			struct Ex::Net::Address::Ip					: Address {};
			struct Ex::Net::Address::Port				: Address {};
		struct Ex::Net::Socket						: Net {
			int code = 0;
		};
		struct Ex::Net::System						: Net {};
	/*!
	Access permission exception */
	struct Ex::Permission						: Ex {};
	/*!
	Exchange protocol exception */
	struct Ex::Protocol							: Ex {};
	/*!
	System host exception */
	struct Ex::System							: Ex { struct File; struct Memory; struct Registry; struct Service; struct Thread; };
		struct Ex::System::File						: System {};
		struct Ex::System::Memory					: System {};
		struct Ex::System::Registry					: System {};
		struct Ex::System::Service					: System {};
		struct Ex::System::Thread					: System {};
	/*!
	Ressource Unfound */
	struct Ex::Unfound : Ex {};
	/*!
	Unavailable feature */
	struct Ex::Unavailable : Ex {};
	/*!
	Unsupported feature */
	struct Ex::Unsupported : Ex {};


struct Exception : virtual Object {
	CONST_STRING(toString());

	Exception() {}
	Exception(const Exception& other) : _pEx(other._pEx) {}
	Exception(Exception&& other) : _pEx(std::move(other._pEx)) {}

	Exception& operator=(const Exception& other) {
		_pEx = other._pEx;
		return *this;
	}
	Exception& operator=(Exception&& other) {
		_pEx = std::move(other._pEx);
		return *this;
	}
	Exception& operator=(std::nullptr_t) { return reset(); }

	explicit operator bool() const { return _pEx.operator bool(); } // explicit otherwise can be converted to a number, and confuse overload function name(ex) and name(number)

	template<typename ExType, typename ...Args>
	ExType& set(Args&&... args) {
		ExType* pEx(new ExType());
		_pEx.reset(pEx);
		if (String::Assign<std::string>(*pEx, std::forward<Args>(args)...).empty())
			String::Assign(*pEx, typeof<ExType>()," exception");
		return *pEx;
	}
	Exception& reset() { _pEx.reset();  return *this; }

	template<typename ExType>
	const ExType& cast() const { return dynamic_cast<ExType*>(_pEx.get()) ? (ExType&)*_pEx : failCast<ExType>(); }

private:
	template<typename ExType>
	const ExType& failCast() const { static ExType Ex; return Ex; }
	const std::string& toString() const { static std::string NoError("No error"); return _pEx ? *_pEx : NoError; }

	shared<Ex>	_pEx;
};

#if defined(_DEBUG)
#define		FATAL_CHECK(CONDITION)			{ assert(CONDITION); }
#if defined(_WIN32)
#define		FATAL_ERROR(...)				{ if (_CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, Mona::String(__VA_ARGS__).c_str()) == 1) _CrtDbgBreak(); }
#elif defined(_BSD)
#define		FATAL_ERROR(...)				{  __assert(NULL, __FILE__,__LINE__, Mona::String(__VA_ARGS__).c_str()); }
#elif defined(__ANDROID__)
#define		FATAL_ERROR(...)				{  __assert_rtn(NULL, __FILE__,__LINE__, Mona::String(__VA_ARGS__).c_str()); }
#else
#define		FATAL_ERROR(...)				{  __assert_fail(Mona::String(__VA_ARGS__).c_str(),__FILE__,__LINE__,NULL); }
#endif

#else

#define		FATAL_CHECK(CONDITION)		{ if(!(CONDITION)) {throw std::runtime_error( #CONDITION " assertion, " __FILE__ "[" LINE_STRING "]");} }
#define		FATAL_ERROR(...)			{ throw std::runtime_error(Mona::String(__VA_ARGS__,", " __FILE__ "[" LINE_STRING "]"));}

#endif

} // namespace Mona
