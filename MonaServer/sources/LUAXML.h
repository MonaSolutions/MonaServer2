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

#include "Script.h"
#include "Mona/XMLParser.h"

namespace Mona {

struct LUAXML : private XMLParser {
/*
XML

<?xml version="1.0"?>
<document>
  <article>
    <p>This is the first paragraph.</p>
    <h2 class='opt'>Title with opt style</h2>
  </article>
  <article>
    <p>Some <b>important</b> text.</p>
  </article>
</document>

LUA

{ xml = {version = 1.0},
	{__name = 'document',
	  {__name = 'article',
		{__name = 'p', 'This is the first paragraph.'},
		{__name = 'h2', class = 'opt', 'Title with opt style'},
	  },
	  {__name = 'article',
		{__name = 'p', 'Some ', {__name = 'b', 'important'}, ' text.'},
	  },
	}
}

{ xml = {
	{ document = {
		{ article = {
			{ p= {'This is the first paragraph.'} },
			{ h2= {'Title with opt style'}, class = 'opt'}
		}},
		{ article = {
			{ p= {'Some', {b='important'}, 'text' }}
		}}
	}},
	version = 1.0
}}

Access to "This is the first paragraph" =>
1 - variable[1][1][1][1]
2 - variable.document.article.p.__value

*/

	static bool XMLToLUA(Exception& ex, lua_State* pState, const char* data, UInt32 size) { return LUAXML(pState, data, size).parse(ex) != XMLParser::RESULT_ERROR; }
	static bool LUAToXML(Exception& ex, lua_State* pState, int index);

private:
	static bool LUAToXMLElement(Exception& ex, lua_State* pState, BinaryWriter& writer);


	LUAXML(lua_State* pState, const char* data, UInt32 size) : _pState(pState), XMLParser(data, size) {}

	bool onStartXMLDocument() { lua_newtable(_pState); _firstIndex = lua_gettop(_pState); return true; }
	bool onStartXMLElement(const char* name, Parameters& attributes);
	bool onXMLInfos(const char* name, Parameters& attributes);
	bool onInnerXMLElement(const char* name, const char* data, UInt32 size);
	bool onEndXMLElement(const char* name);
	void onEndXMLDocument(const char* error) { addMetatable(); }

	void addMetatable();

	
	lua_State*			_pState;
	int					_firstIndex;
};

}
