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

#include "Test.h"
#include "Mona/Parameters.h"

using namespace std;
using namespace Mona;

namespace ParametersTest {

static const string _Key("key");
static string		_Value;


ADD_TEST(MapParameters) {
	Parameters params;

	bool   bValue;
	double dValue;

	// Set STRING
	params.setString(_Key, "1");
	CHECK(params.getString(_Key, _Value) && _Value == "1");
	CHECK(params.getBoolean(_Key,bValue) && bValue);
	CHECK(params.getBoolean(_Key));
	CHECK(params.getNumber(_Key,dValue) && dValue==1);
	CHECK(params.getNumber(_Key)==1);
	params.setString(_Key, "1",1);
	CHECK(params.getString(_Key, _Value) && _Value == "1");
	

	// Set BOOL
	params.setBoolean(_Key, true);
	CHECK(params.getString(_Key, _Value) && _Value == "true");
	CHECK(params.getBoolean(_Key,bValue) && bValue);
	CHECK(params.getBoolean(_Key));

	params.setBoolean(_Key, false);
	CHECK(params.getString(_Key, _Value) && _Value == "false");
	CHECK(params.getBoolean(_Key,bValue) && !bValue);
	CHECK(!params.getBoolean<true>(_Key));

	// Set Number
	params.setNumber(_Key, 1);
	CHECK(params.getString(_Key, _Value) && _Value == "1");
	CHECK(params.getBoolean(_Key,bValue) && bValue);
	CHECK(params.getBoolean(_Key));
	CHECK(params.getNumber(_Key,dValue) && dValue==1);
	CHECK(params.getNumber(_Key)==1);

	params.setNumber(_Key, 0);
	CHECK(params.getString(_Key, _Value) && _Value == "0");
	CHECK(params.getBoolean(_Key,bValue) && !bValue);
	CHECK(!params.getBoolean<true>(_Key));
	CHECK(params.getNumber(_Key,dValue) && dValue==0);
	CHECK(params.getNumber(_Key)==0);

	// Erase
	CHECK(params.count() == 1);
	params.erase(_Key);
	CHECK(params.count() == 0);
	params.setString("hello", EXPAND("mona"));
	CHECK(params.count() == 1);
	params.clear("hel");
	CHECK(params.count() == 0);
	params.setString("hello", EXPAND("mona"));
	CHECK(params.count() == 1);
	params.clear();
	CHECK(params.count() == 0);
}

}
