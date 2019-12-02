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

#include "Mona/Application.h"
#include "Mona/Stopwatch.h"

namespace Mona {


struct UnitTest : private Application {
	struct Test : virtual Object {
		Test(const std::string& type) : _name(type.data(), type.size() - 4) {}

		void run(Mona::UInt32 loop);

	protected:
		UInt32	_loop;
	private:
		virtual void TestFunction() {}
			/// \brief The test function to overload

		std::string	_name; /// fullname of the test
		Stopwatch	_chrono;
	};

	template<typename TestType>
	static bool AddTest() { 
		const std::string& type = typeof<TestType>();
		Tests().emplace(SET, std::forward_as_tuple(type.data(), type.find("::")), std::forward_as_tuple(std::make_unique<TestType>(type)));
		return true;
	}
	static int Run(const std::string& version, int argc, const char* argv[]) {
		static UnitTest App(version);
		return App.run(argc, argv);
	};

private:
	UnitTest(const std::string& version) : _version(version), _loop(1) { setString("description", "Unit tests"); }

	static std::multimap<const std::string, unique<Test>, String::IComparator>& Tests() {
		static std::multimap<const std::string, unique<Test>, String::IComparator> _Tests;
		return _Tests;
	}

	///// MAIN
	int main();

	const char* defineVersion() { return _version.c_str(); }

	void defineOptions(Exception& ex, Options& options);

	void runSelectedModule();

	// POOLTESTS
	void runAll(Mona::UInt32 loop);
		/// \brief Run all tests

	void runOne(const std::string& mod, Mona::UInt32 loop);
		/// \brief Run the test with 'mod' name

	void runAt(const std::string& mod, Mona::UInt32 loop);
		/// \brief Try to run the test at index mod (can be in the form XX for 1 module or XX::YY to select 1 specific test index from a module)

	UInt32				_loop;
	const std::string	_version;
};

/// Macro for assert true function

#if defined(_DEBUG)
#define CHECK(ASSERT)		DEBUG_ASSERT(ASSERT)
#define DEBUG_CHECK(ASSERT)	DEBUG_ASSERT(ASSERT)
#else
#define CHECK(ASSERT)		{ if(!(ASSERT)) throw std::runtime_error( #ASSERT " assertion, " __FILE__ "[" LINE_STRING "]"); }
#define DEBUG_CHECK(ASSERT) {}
#endif

/// Macro for adding new tests in a Test cpp
#define ADD_TEST(NAME) struct NAME##TEST : UnitTest::Test { \
	NAME##TEST(const std::string& type) : UnitTest::Test(type) {}\
	void TestFunction();\
private:\
	static const bool _TestCreated;\
};\
const bool NAME##TEST::_TestCreated = UnitTest::AddTest<NAME##TEST>();\
void NAME##TEST::TestFunction()

#if defined(_DEBUG)
#define ADD_DEBUG_TEST(NAME) ADD_TEST(NAME)
#else
#define ADD_DEBUG_TEST(NAME) void TEST##NAME()
#endif

} // namespace Mona
