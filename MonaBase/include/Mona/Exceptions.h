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

namespace Mona {

	struct Ex : String, virtual Object {
		NULLABLE(empty())
		struct Application; struct Extern; struct Format; struct Intern; struct Net; struct Permission; struct Protocol; struct System; struct Unavailable; struct Unfound; struct Unsupported;
	};
	/*!
	Application exception, error from user side */
	struct Ex::Application						: Ex { struct Argument; struct Config; struct Error; struct Invalid; };
		struct Ex::Application::Argument			: Application {}; // Application argument given error
		struct Ex::Application::Config				: Application {}; // Application configuration error
		struct Ex::Application::Error				: Application {}; // error during application execution
		struct Ex::Application::Invalid				: Application {}; // Invalid
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
	NULLABLE(!_pEx)
	CONST_STRING(toString());

	Exception(std::nullptr_t = nullptr) {}
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

	template<typename ExType, typename ...Args>
	ExType& set(Args&&... args) {
		ExType& ex = _pEx.set<ExType>();
		if (String::Assign<std::string>(ex, std::forward<Args>(args)...).empty())
			String::Assign(ex, typeof<ExType>()," exception");
		return ex;
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


#define		STATIC_ASSERT(...)				{ static_assert(__VA_ARGS__, #__VA_ARGS__); }

#if defined(_DEBUG)

#if defined(_WIN32)
#define		DEBUG_ASSERT(ASSERT)			{ _ASSERTE(ASSERT); }
#define		FATAL_ERROR(...)				{ if (_CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, Mona::String(__VA_ARGS__).c_str()) == 1) _CrtDbgBreak(); }
#else
#define		DEBUG_ASSERT(ASSERT)			{ assert(ASSERT); }
#if defined(__ANDROID__)
#define		FATAL_ERROR(...)				{  __assert(__FILE__,__LINE__, Mona::String(__VA_ARGS__).c_str()); }
#elif defined(__APPLE__)
#define		FATAL_ERROR(...)				{  __assert_rtn(NULL, __FILE__,__LINE__, Mona::String(__VA_ARGS__).c_str()); }
#elif defined(_BSD)
#define		FATAL_ERROR(...)				{  __assert(NULL, __FILE__,__LINE__, Mona::String(__VA_ARGS__).c_str()); }
#else
#define		FATAL_ERROR(...)				{  __assert_fail(Mona::String(__VA_ARGS__).c_str(),__FILE__,__LINE__,NULL); }
#endif
#endif

#else

#define		DEBUG_ASSERT(ASSERT)		{}
#define		FATAL_ERROR(...)			{ throw std::runtime_error(Mona::String(__VA_ARGS__,", " __FILE__ "[" LINE_STRING "]"));}

#endif


} // namespace Mona
