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

#include "Mona/Application.h"
#include "Mona/Logs.h"
#include "Test.h"
#include "Version.h"
#include <iostream>

using namespace Mona;
using namespace std;

struct TestApp : Application  {
	TestApp() : _loop(1) {}
private:
	const char* defineVersion() { return STRINGIZE(MONA_VERSION); }

	void defineOptions(Exception& ex, Options& options) {
		
        options.add(ex, "module", "m", "Specify the module to run.")
			.argument("module");

		options.add(ex, "loop", "l", "Specify the number of loop to execute for every test to run.")
			.argument("number of loop");

		// defines here your options applications
        Application::defineOptions(ex,options);
	}

	void runSelectedModule() {

        vector<std::string> lTests;
        PoolTest::PoolTestInstance().getListTests(lTests);

		// Print The list
		int index = 0;
		string tmp;
        for(string& test : lTests)
			cout << String::Assign(tmp, index++, " - ", test) << endl;

		// Ask for the index
		cout << endl << "Choose the index of the test to run (or type enter to run all) : ";
		string input;
		getline(cin, input);
		
		
		int number(0);
		if (!input.empty()) {
			Exception ex;
			if (String::ToNumber<int>(ex, input, number))
				return PoolTest::PoolTestInstance().run(lTests.at(number), _loop);
			WARN("Invalid test index, ", ex);
		}
        PoolTest::PoolTestInstance().runAll(_loop);
	}

///// MAIN
	int main() {
		getNumber("arguments.loop", _loop);
		const char* module;
		if (!(module=getString("arguments.module")))
			runSelectedModule();
		else if (String::ICompare(module,"all")==0)
			PoolTest::PoolTestInstance().runAll(_loop);
		else
			PoolTest::PoolTestInstance().run(module,_loop);

        NOTE("END OF TESTS");
        cout << "Press any key to exit";
        getchar();

		return EXIT_OK;
	}

private:
	UInt32	_loop;
};


int main(int argc, const char* argv[]) {
    return TestApp().run(argc, argv);
}
