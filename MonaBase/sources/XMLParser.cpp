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

#include "Mona/XMLParser.h"

using namespace std;

namespace Mona {

void XMLParser::reset() {
	_reseted = true;
	_started=false;
	_ex = NULL;
	_current = _start;
	_tags.clear();
}

void XMLParser::reset(const XMLState& state) {
	if (!state)
		return reset();
	_reseted = true;
	_started = state._started;
	_ex = state._ex;
	_current = state._current;
	_tags = state._tags;
}

void XMLParser::save(XMLState& state) {
	state._started = _started;
	state._current = _current;
	state._tags    = _tags;
	state._ex = _ex;
}

XMLParser::RESULT XMLParser::parse(Exception& ex) {
	if (_running) {
		ex.set<Ex::Intern>("Don't call recursively XMLParser::parse function");
		return RESULT_ERROR;
	}
	if (_ex) {
		ex = _ex;
		return RESULT_ERROR;
	}
	_running = true;
	RESULT result = parse();
	if (result!=RESULT_PAUSED)
		onEndXMLDocument(_ex ? (ex=_ex).c_str() : NULL);
	_running = false;
	return result;
}

XMLParser::RESULT XMLParser::parse() {

RESET:
	_reseted = false;
	if (_ex)
		return RESULT_ERROR;

	UInt32 size(0);
	char*  name(NULL);

	while (!_tags.empty()) {
		Tag& tag(_tags.back());
		if (!tag.full)
			break;
		String::Scoped scoped((name=tag.name)+tag.size);
		_tags.pop_back();
		if(!onEndXMLElement(name))
			return  (_current == _end && _tags.empty()) ? RESULT_DONE : RESULT_PAUSED;
		if (_reseted)
			goto RESET;
	}

	bool isInner(false);
	UInt32 innerSpaceRemovables(0);

	while (_current<_end) {

		// start element or end element
		if (*_current == '<') {

			if (!_started) {
				_started = true; // before to allow a call to save
				if (!onStartXMLDocument())
					return RESULT_PAUSED;
				if (_reseted)
					goto RESET;
			}

			// process inner!
			if (isInner && !((_end - _current) >= 9 && memcmp(_current, "<![CDATA[", 9) == 0)) {
	
				// skip end space if not CDATA
				_bufferInner.resize(_bufferInner.size() - innerSpaceRemovables,true);
				_bufferInner.append(EXPAND("\0")); // string null termination
		

				Tag& tag(_tags.back());

				String::Scoped scoped(tag.name+tag.size);
				isInner = false;
				innerSpaceRemovables = 0;
				if(!onInnerXMLElement(tag.name, STR _bufferInner.data(), _bufferInner.size() - 1))
					return RESULT_PAUSED;
				if (_reseted)
					goto RESET;
			}

			if (++_current == _end) {
				_ex.set<Ex::Format>("XML element without tag name");
				return RESULT_ERROR;
			}

			// just after <

			bool next(true);
				
			if (*_current == '/') {
				//// END ELEMENT ////

				if (_tags.empty()) {
					_ex.set<Ex::Format>("XML end element without starting before");
					return RESULT_ERROR;
				}

				Tag& tag(_tags.back());
				name = tag.name;
				size = tag.size;

				if ((_current+size) >= _end || memcmp(name,++_current,size)!=0) {
					_ex.set<Ex::Format>("XML end element is not the '",string(name,size),"' expected");
					return RESULT_ERROR;
				}
				_current += size;

				// skip space
				while (_current < _end && isspace(*_current))
					++_current;
			
				if (_current == _end || *_current!='>') {
					_ex.set<Ex::Format>("XML end '",string(name,size),"' element without > termination");
					return RESULT_ERROR;
				}
	
				// on '>'

				// skip space
				++_current;
				if (!isInner) while (_current < _end && isspace(*_current)) ++_current;


				String::Scoped scoped(name + size);
				_tags.pop_back();
				next = onEndXMLElement(name);

			} else if (*_current == '!') {
				//// COMMENT or CDATA ////

				if (++_current == _end) {
					_ex.set<Ex::Format>("XML comment or CDATA malformed");
					return RESULT_ERROR;
				}

				if (isInner || ((_end - _current) >= 7 && memcmp(_current, "[CDATA[", 7) == 0)) {
					/// CDATA ///

					if (_tags.empty()) {
						_ex.set<Ex::Format>("No XML CDATA inner value possible without a XML parent element");
						return RESULT_ERROR;
					}

					_current += 7;
					// can be end

					if (!isInner) {
						isInner = true;
						_bufferInner.clear();
					} else
						innerSpaceRemovables = 0;
					while (_current < _end) {
						if (*_current == ']' && ++_current < _end && *_current == ']' && ++_current < _end && *_current == '>')
							break;
						_bufferInner.append(_current++,1);
					}
					
					if (_current == _end) {
						_ex.set<Ex::Format>("XML CDATA without ]]> termination");
						return RESULT_ERROR;
					}
				
					// on '>'

				} else {
					/// COMMENT ///

					char delimiter1 = *_current;
					if (++_current == _end) {
						_ex.set<Ex::Format>("XML comment malformed");
						return RESULT_ERROR;
					}
					char delimiter2 = *_current;

					while (++_current < _end) {
						if (*_current == delimiter1 && ++_current < _end && *_current == delimiter2 && ++_current < _end && *_current == '>')
							break;
					}

					if (_current == _end) {
						_ex.set<Ex::Format>("XML comment without > termination");
						return RESULT_ERROR;
					}

					// on '>'
				}
				
				// skip space
				++_current;
				if (!isInner) while (_current < _end && isspace(*_current)) ++_current;

			} else {

				//// START ELEMENT ////

				bool isInfos(*_current == '?');
				if (isInfos && ++_current == _end) {
					_ex.set<Ex::Format>("XML info element without name");
					return RESULT_ERROR;
				}

				Tag tag;
				name = tag.name = (char*)parseXMLName(isInfos ? "?" : "/>", tag.size);
				if (!name)
					return RESULT_ERROR;
				size = tag.size;
		
				/// space or > or /

				/// read attributes
				_attributes.clear();
				while (_current < _end) {

					// skip space
					while (isspace(*_current)) {
							if (++_current==_end)  {
							_ex.set<Ex::Format>("XML element '",string(name,size),"' without termination");
							return RESULT_ERROR;
						}
					}
				
					if (isInfos && (*_current == '?')) {
						if (++_current == _end || *_current!='>') {
							_ex.set<Ex::Format>("XML info '",string(name,size),"' without ?> termination");
							return RESULT_ERROR;
						}
						break;
					} else if (*_current == '/') {
						if (++_current == _end || *_current!='>') {
							_ex.set<Ex::Format>("XML element '",string(name,size),"' without termination");
							return RESULT_ERROR;
						}
						tag.full = true;
						break;
					} else if (*_current == '>')
						break;
	
					// read name attribute
					UInt32 sizeKey(0);
					const char* key(parseXMLName("=", sizeKey));
					if (!key)
						return RESULT_ERROR;

					/// space or =

					// skip space
					while(isspace(*_current)) {
						if (++_current == _end) {
							_ex.set<Ex::Format>("XML attribute '",string(key,sizeKey),"' without value");
							return RESULT_ERROR;
						}
						if (*_current == '=')
							break;
					}

					// =

					if (++_current==_end) {
						_ex.set<Ex::Format>("XML attribute '",string(key,sizeKey),"' without value");
						return RESULT_ERROR;
					}

					// skip space
					while(isspace(*_current)) {
						if (++_current == _end) {
							_ex.set<Ex::Format>("XML attribute '",string(key,sizeKey),"' without value");
							return RESULT_ERROR;
						}
					}

					/// just after = and space

					char delimiter(*_current);

					if ((delimiter != '"' && delimiter != '\'') || ++_current==_end) {
						_ex.set<Ex::Format>("XML attribute '",string(key,sizeKey),"' without value");
						return RESULT_ERROR;
					}

			

					/// just after " start of attribute value

					// read value attribute
					UInt32 sizeValue(0);
					const char* value(_current);
					while (*_current != delimiter) {
						if (*_current== '\\') {
							if (++_current == _end) {
								_ex.set<Ex::Format>("XML attribute '",string(key,sizeKey),"' with malformed value");
								return RESULT_ERROR;
							}
							++sizeValue;
						}
						if(++_current==_end) {
							_ex.set<Ex::Format>("XML attribute '",string(key,sizeKey),"' with malformed value");
							return RESULT_ERROR;
						}
						++sizeValue;
					}
						
					++_current;

					/// just after " end of attribute value

					// store name=value
					String::Scoped scoped(key + sizeKey);
					_attributes.setString(key, value, sizeValue);
				}



				if (_current==_end)  {
					_ex.set<Ex::Format>("XML element '",string(name,size),"' without > termination");
					return RESULT_ERROR;
				}
			
				// on '>' ('/>' or  '?>' or '>')

				// skip space
				++_current;
				if (!isInner) while (_current < _end && isspace(*_current)) ++_current;

				String::Scoped scoped(name + size);
				if (isInfos)
					next = onXMLInfos(name, (Parameters&)_attributes);
				else {
					_tags.emplace_back(tag); // before to allow a call to save
					next = onStartXMLElement(name, (Parameters&)_attributes);
					if (next && tag.full) {
						_tags.pop_back();  // before to allow a call to save
						next = onEndXMLElement(name);
					}
				}
			}


			if (!next)
				return (_current == _end && _tags.empty()) ? RESULT_DONE : RESULT_PAUSED;
			if (_reseted)
				goto RESET;
	
			// after '>' (element end or element start) and after space, and can be _end

		} else {
			if (isInner) { // inner value
				_bufferInner.append(_current,1);
				if (isspace(*_current))
					++innerSpaceRemovables;
				else
					innerSpaceRemovables = 0;
			} else if (!isspace(*_current)) {
				if (_tags.empty()) {
					_ex.set<Ex::Format>("No XML inner value possible without a XML parent element");
					return RESULT_ERROR;
				}
				isInner = true;
				_bufferInner.resize(1, false);
				*_bufferInner.data() = *_current;
			}	
			++_current;
		}
			
	}

	if (!_started) {
		_started = true;  // before to allow a call to save
		if (!onStartXMLDocument())
			return RESULT_PAUSED;
		if (_reseted)
			goto RESET;
	} else if (!_tags.empty()) {
		_ex.set<Ex::Format>("No XML end '", string(_tags.back().name, _tags.back().size), "' element");
		return RESULT_ERROR;
	}

	return RESULT_DONE; // no more data!
}


const char* XMLParser::parseXMLName(const char* endMarkers, UInt32& size) {

	if (!isalpha(*_current) && *_current!='_')  {
		_ex.set<Ex::Format>("XML name must start with an alphabetic character");
		return NULL;
	}
	const char* name(_current++);
	while (_current < _end && isxml(*_current))
		++_current;
	size = (_current - name);
	if (_current==_end || (!strchr(endMarkers,*_current) && !isspace(*_current)))  {
		_ex.set<Ex::Format>("XML name '",string(name,size),"' without termination");
		return NULL;
	}
	return name;
}

} // namespace Mona
