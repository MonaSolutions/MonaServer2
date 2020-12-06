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

#include "Mona/UnitTest.h"
#include "Mona/Resources.h"
#include "Mona/Timer.h"

using namespace Mona;
using namespace std;

namespace ResourcesTest {

ADD_TEST(All) {
	Timer timer;
	Resources resources(timer);
	CHECK(!resources);
	resources.create<string>("name1", 0, "value1");
	resources.create<string>("name2", 500, "value2");
	CHECK(resources);
	CHECK(timer.raise());

	CHECK(resources.count() == 2);
	string* pValue = resources.use<string>("name1");
	CHECK(pValue && pValue->compare("value1") == 0);
	pValue = resources.use<string>("name2");
	CHECK(pValue && pValue->compare("value2") == 0);

	Thread::Sleep(700);
	CHECK(!timer.raise());

	CHECK(resources.count() == 1);
	pValue = resources.use<string>("name1");
	CHECK(pValue && pValue->compare("value1") == 0);
	pValue = resources.use<string>("name2");
	CHECK(!pValue);

	CHECK(resources.release<string>("name1"));
	pValue = resources.use<string>("name1");
	CHECK(!pValue);
	CHECK(resources.count() == 0);
}

}
