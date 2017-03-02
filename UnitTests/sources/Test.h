/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#pragma once

#include "Mona/Exceptions.h"
#include "Mona/Stopwatch.h"
#include <memory>

/// \brief The fixture for testing class Foo.
struct Test : virtual Mona::Object {

	Test(const std::string& type) : _name(type.data(), type.size()-4) {}

	void run(Mona::UInt32 loop);

protected:
	Mona::UInt32	_loop;
private:
	virtual void TestFunction() = 0;
		/// \brief The test function to overload


	std::string		_name; /// fullname of the test
	Mona::Stopwatch	_chrono;
};

/// \class Container of Test classes
struct PoolTest : virtual Mona::Object {

	template<typename TestType>
    bool makeAndRegister() {
		const std::string& type = Mona::typeof<TestType>();
		_mapTests.emplace(std::piecewise_construct, std::forward_as_tuple(std::string(type.data(), type.find("::"))), std::forward_as_tuple(new TestType(type)));
		return true;
	}
		/// \brief create the test and add it to the PoolTest

    void getListTests(std::vector<std::string>& lTests);
		/// \brief get a list of test module names

	void runAll(Mona::UInt32 loop);
		/// \brief Run all tests

	void run(const std::string& mod,Mona::UInt32 loop);
		/// \brief Run the test with 'mod' name

	static PoolTest& PoolTestInstance();
		/// \brief PoolTest Instance accessor

private:
    std::multimap<const std::string, std::unique_ptr<Test>, Mona::String::IComparator> _mapTests;
		/// multimap of Test name to Tests functions
			
	PoolTest(){}
		/// \brief PoolTest Constructor

	virtual ~PoolTest(){}
		/// \brief destructor of PoolTest

};


#define LOOP _loop

/// Macro for assert true function
#define CHECK(CONDITION) FATAL_CHECK(CONDITION)

#if defined(_DEBUG)
#define DEBUG_CHECK(CONDITION) CHECK(CONDITION)
#else
#define DEBUG_CHECK(CONDITION) {}
#endif

/// Macro for adding new tests in a Test cpp
#define ADD_TEST(NAME) struct NAME##TEST : Test { \
	NAME##TEST(const std::string& type) : Test(type) {}\
	void TestFunction();\
private:\
	static const bool _TestCreated;\
};\
const bool NAME##TEST::_TestCreated = PoolTest::PoolTestInstance().makeAndRegister<NAME##TEST>();\
void NAME##TEST::TestFunction()

#if defined(_DEBUG)
#define ADD_DEBUG_TEST(NAME) ADD_TEST(NAME)
#else
#define ADD_DEBUG_TEST(NAME) void TEST##NAME()
#endif
