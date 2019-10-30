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

#include "Mona/UnitTest.h"
using namespace std;


namespace Mona {

void UnitTest::Test::run(UInt32 loop) {
	_chrono.restart();
	DEBUG(_name," now running ", loop, " iterations...");
	for (_loop = 0; _loop < loop; ++_loop)
		TestFunction();
	_chrono.stop();
	if (loop>1)
		NOTE(_name, " OK (x", _loop, " ", _chrono.elapsed(), "ms)")
	else
		NOTE(_name, " OK (", _chrono.elapsed(), "ms)");
}

int UnitTest::main() {
	getNumber("arguments.loop", _loop);
	const char* module;
	if (!(module = getString("arguments.module")))
		runSelectedModule();
	else if (String::ICompare(module, "all") == 0)
		runAll(_loop);
	else
		runOne(module, _loop);

	NOTE("END OF TESTS");
	cout << "Press any key to exit";
	getchar();

	return EXIT_OK;
}

void UnitTest::defineOptions(Exception& ex, Options& options) {

	options.add(ex, "module", "m", "Specify the module to run.")
		.argument("module");

	options.add(ex, "loop", "x", "Specify the number of loop to execute for every test to run.")
		.argument("number of loop");

	// defines here your options applications
	Application::defineOptions(ex, options);
}

void UnitTest::runSelectedModule() {

	// Print The list
	int index = 0;
	string tmp;
	for (auto itTest = Tests().begin(), end = Tests().end(); itTest != end; itTest = Tests().upper_bound(itTest->first))
		cout << String::Assign(tmp, index++, " - ", itTest->first) << endl;

	// Ask for the index
	cout << endl << "Choose the index of the test to run (or type enter to run all) : ";
	string input;
	getline(cin, input);

	cout << endl;

	if (!input.empty())
		return runAt(input, _loop);
	runAll(_loop);
}

void UnitTest::runAll(UInt32 loop) {
	for (auto& itTest : Tests())
		itTest.second->run(loop);
}

void UnitTest::runOne(const string& mod, UInt32 loop) {
	// Try to find module by name
	auto itTest = Tests().equal_range(mod);
	if (itTest.first == itTest.second) {
		itTest = Tests().equal_range(mod + "Test"); // try adding missing Test suffix
		if (itTest.first == itTest.second)
			return runAt(mod, loop); // try index or return an error if not found
	}

	// Run all tests of the module
	for (auto& it = itTest.first; it != itTest.second; it++)
		it->second->run(loop);
}

void UnitTest::runAt(const string& mod, UInt32 loop) {
	// Subtest?
	Exception ex;
	string test(mod), subTest;
	size_t pos = test.find("::");
	if (pos != string::npos) {
		subTest.assign(test, pos + 2, string::npos);
		test.resize(pos);
		if (!String::ToNumber<size_t>(ex, subTest, pos))
			pos = string::npos; // by default if not convertible execute all tests
	}

	UInt32 number;
	if (String::ToNumber<UInt32>(ex, test, number)) {

		for (auto itTest = Tests().begin(), end = Tests().end(); itTest != end; itTest = Tests().upper_bound(itTest->first)) {
			if (!number--) { // Module at index number found
				auto range = Tests().equal_range(itTest->first);
				for (auto& it = range.first; it != range.second; it++) {
					if (pos == string::npos) // Run all tests of the module
						it->second->run(loop);
					else if (!pos--)
						return it->second->run(loop); // Run a specific subtest
				}
				if (pos == string::npos)
					return; // otherwise subtest not found
			}
		}
	}
	ERROR("Impossible to find module at index ", mod, ".");
}

} // namespace Mona
