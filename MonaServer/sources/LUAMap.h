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
#include "ScriptReader.h"
#include "Mona/MapWriter.h"
#include "Mona/StringWriter.h"

namespace Mona {


template<typename MapType>
struct LUAMap : virtual Static {

	template<typename Type = MapType>
	struct Mapper : virtual Object {
		typedef Type ObjType;

		Mapper(ObjType& obj, lua_State *pState) : pState(pState), obj(obj), map(obj) {}

		ObjType&   obj;
		MapType&   map;
		lua_State* pState;

		typename MapType::const_iterator	begin() const { return map.begin(); }
		typename MapType::const_iterator	end() const { return map.end(); }
		typename MapType::const_iterator	lower_bound(const std::string& key) const { return map.lower_bound(key); }
		void								pushValue(typename const MapType::const_iterator& it) { Script::PushValue(pState, it->second.data(), it->second.size()); }

		template<typename T = MapType, typename std::enable_if<has_count<T>::value, int>::type = 0>
		UInt32								size() const { return map.count(); }
		template<typename T = MapType, typename std::enable_if<!has_count<T>::value, int>::type = 0>
		UInt32								size() const { return map.size(); }

		template<typename T = MapType, typename std::enable_if<std::is_const<T>::value, int>::type = 0>
		typename bool						erase(typename const MapType::const_iterator& first, typename const MapType::const_iterator& last) { return false; }
		template<typename T = MapType, typename std::enable_if<!std::is_const<T>::value, int>::type = 0>
		typename bool						erase(typename const MapType::const_iterator& first, typename const MapType::const_iterator& last) { map.erase(first, last); return true; }
		/*!
		Assign the property key with value, must return end() if not settable */
		template<typename T = MapType, typename std::enable_if<std::is_const<T>::value, int>::type = 0>
		typename MapType::const_iterator	set(const std::string& key, std::string&& value, DataReader& parameters) { return end(); }
		template<typename T = MapType, typename std::enable_if<!std::is_const<T>::value, int>::type = 0>
		typename MapType::const_iterator	set(const std::string& key, std::string&& value, DataReader& parameters) { return map.emplace(key, std::move(value)).first; }
	protected:
		Mapper(ObjType& obj, MapType& map, lua_State *pState) : pState(pState), obj(obj), map(map) {}
	};

	template<typename M = Mapper<MapType>>
	static int Pairs(lua_State *pState) {
		SCRIPT_CALLBACK(M::ObjType, obj);
			M m(obj, pState);
			string prefix;
			if (ToKey(pState, 1, prefix)) {
				auto it = m.lower_bound(prefix);
				prefix.back() += 1;
				auto itEnd = m.lower_bound(prefix);
				if (itEnd != m.begin()) {
					--itEnd;
					lua_pushlstring(pState, itEnd->first.data(), itEnd->first.size());
					lua_pushcclosure(pState, &(Next<M>), 1);
				} else
					lua_pushcfunction(pState, &(Next<M>));
				lua_pushvalue(pState, 1);
				if(it!= m.begin())
					SCRIPT_WRITE_STRING((--it)->first);
			} else { // iterate full collection!
				lua_pushcfunction(pState, &(Next<M>));
				lua_pushvalue(pState, 1);
			}
		SCRIPT_CALLBACK_RETURN;
	}

	template<typename M = Mapper<MapType>>
	static int Call(lua_State *pState) {
		SCRIPT_CALLBACK(M::ObjType, obj)
			M m(obj, pState);
			// skip this if is the second argument (call with :)
			int hasThis = 0;
			lua_rawgeti(pState, 1, 1); // {[1] = this}
			lua_pushvalue(pState, 2);
			while(lua_istable(pState, -1)) {
				if (lua_equal(pState, -1, -2)) {
					hasThis = SCRIPT_READ_NEXT(1);
					break;
				}
				/// is maybe a sub index table (index of index of index ...)
				if (!lua_getmetatable(pState, -1))
					break;
				lua_pushliteral(pState, "__index");
				lua_rawget(pState, -2);
				lua_replace(pState, -2); //replace metatable
				lua_replace(pState, -2); //replace old table
			};
			lua_pop(pState, 2);

			int available = SCRIPT_NEXT_READABLE;
			string key;
			bool keying = ToKey(pState, 1, key);
			if (available > 0) {
				// SETTERS
				/// first argument is the value, the rest is setter parameter (can be used by HTTP SetCookie for example)
				ScriptReader reader(pState, UInt32(available));
				ScriptReader parameters(pState, UInt32(available-1));

				bool failed = false;
				if (reader.readNull()) {
					// erase value
					/// primitive object, erase just the related value!
					if(keying) {
						const auto& it = m.lower_bound(key);
						static MapType::key_compare Less;
						if (it != m.end() && !Less(key, it->first)) {
							auto itNext = it;
							m.pushValue(it); // push old value!
							if (m.erase(it, ++itNext)) {
								lua_pop(pState, 1);
								SCRIPT_WRITE_NIL
							} else
								SCRIPT_ERROR("Impossible to erase ", key, " of ", typeof<M::ObjType>());	
						} else
							SCRIPT_WRITE_NIL
					} else {
						SCRIPT_ERROR("Miss property name to erase of ", typeof<M::ObjType>());
						SCRIPT_WRITE_NIL
					}
				} else if (reader.nextType() >= ScriptReader::OTHER) {
					// complex type, add properties!
					struct Writer : MapWriter<Writer> {
						Writer(M& m, DataReader& parameters) : _parameters(parameters), MapWriter<Writer>(self), _m(m) {}
						void emplace(const string& key, string&& value) {
							_parameters.reset();
							const auto& it = _m.set(key, move(value), _parameters);
							if (it == _m.end()) { // call _m.end() after _m.set because can create a incompatible iterator exception
								SCRIPT_BEGIN(_m.pState)
									SCRIPT_ERROR("Impossible to set ", key, " of ", typeof<M::ObjType>())
								SCRIPT_END
							}
						}
					private:
						M&			_m;
						DataReader& _parameters;
					} writer(m, parameters);
					if (keying) {
						writer.beginObject();
						writer.writePropertyName(key.c_str());
					}
					// one param (not multiple!)
					reader.read(writer, 1);
					if (keying)
						writer.endObject();
					lua_pushvalue(pState, 1); /// myself = complex value!
				} else if(keying) {
					// primitive type, convert it to string!
					string value;
					StringWriter<std::string> writer(value);
					reader.read(writer, 1);
					const auto& it = m.set(key, std::move(value), parameters);
					if (it == m.end()) {
						SCRIPT_ERROR("Impossible to set ", key, " of ", typeof<M::ObjType>())
						SCRIPT_WRITE_NIL
					} else
						m.pushValue(it);
				} else {
					SCRIPT_ERROR("Miss property name to add value in ", typeof<M::ObjType>());
					SCRIPT_WRITE_NIL
				}
				SCRIPT_READ_NEXT(reader.position() + parameters.position());

			} else if (keying) {
				if (hasThis) {
					std::size_t offset = key.rfind('.') + 1;
					if (key.compare(offset, std::string::npos, "len") == 0) {
						keying = false;
						// :len()
						if (ToKey(pState, 2, key)) {
							const auto& it = m.lower_bound(key);
							key.back() += 1;
							SCRIPT_WRITE_INT(std::distance(it, m.lower_bound(key)));
						} else
							SCRIPT_WRITE_INT(m.size());

					} else if (!std::is_const<MapType>::value && key.compare(offset, std::string::npos, "clear") == 0) {
						keying = false;
						// :clear(prefix='')
						UInt32 count = m.size();
						if (ToKey(pState, 2, key)) {
							if (!m.erase(m.lower_bound(key), m.lower_bound(String(String::Data(key.data(), key.size() - 1), char(key.back() + 1))))) {
								SCRIPT_ERROR("Impossible to erase ", key, " of ", typeof<M::ObjType>());
								SCRIPT_WRITE_NIL // to detect error (=== NIL)
							} else
								SCRIPT_WRITE_INT(count - m.size());
						} else if (!m.erase(m.begin(), m.end())) {
							SCRIPT_ERROR("Impossible to clear ", typeof<M::ObjType>());
							SCRIPT_WRITE_NIL // to detect error (=== NIL)
						} else
							SCRIPT_WRITE_INT(count - m.size())	
					}
				}

				if (keying) {
					const auto& it = m.lower_bound(key);
					static MapType::key_compare Less;
					if (it == m.end() || Less(key, it->first)) {
						SCRIPT_WRITE_NIL
						SCRIPT_ERROR("attempt to index a nil value");
					} else
						m.pushValue(it);
				}

			} else // No sub map and call without argument (no setter), Script::Call normal!
				Script::Call<MapType>(pState);
	
		SCRIPT_CALLBACK_RETURN
	}

	template<typename M = Mapper<MapType>>
	static int Index(lua_State *pState) {
		if (!lua_gettop(pState)) {
			SCRIPT_TABLE_BEGIN(pState)
				SCRIPT_DEFINE_STRING("len", "length()");
				if(!std::is_const<MapType>::value)
					SCRIPT_DEFINE_STRING("clear", "clear()")
			SCRIPT_TABLE_END
			return 1;
		}
		SCRIPT_CALLBACK(M::ObjType, obj)
			SCRIPT_READ_DATA(name, size);

			std::string key;
			if (ToKey(pState, 1, key))
				key += '.';
			key.append(STR name, size);

			// write this + key
			lua_newtable(pState);
			lua_pushvalue(pState, 1); // this
			lua_rawseti(pState, -2, 1);
			lua_pushlstring(pState, key.data(), key.size()); // key
			lua_rawseti(pState, -2, 2);

			lua_getmetatable(pState, 1);
			lua_setmetatable(pState, -2);

		SCRIPT_CALLBACK_RETURN
	}

private:
	static bool ToKey(lua_State *pState, int index, std::string& key) {
		// Key is in {[2] = KEY}
		lua_rawgeti(pState, index, 2);
		size_t size;
		const char* prefix = lua_tolstring(pState, -1, &size);
		lua_pop(pState, 1);
		if (!prefix)
			return false;
		key.assign(prefix, size);
		return true;
	}

	template<typename M>
	static int Next(lua_State* pState) {
		SCRIPT_CALLBACK(M::ObjType, obj);
			M m(obj, pState);
			SCRIPT_READ_DATA(key, keySize);
			size_t endSize;
			const char* end = lua_tolstring(pState, lua_upvalueindex(1), &endSize);
			if (!end || !key || endSize!=keySize || memcmp(key, end, endSize) != 0) {
				MapType::const_iterator it;
				if (key) {
					it = m.lower_bound(STR key);
					if (it != m.end())
						++it;
				} else
					it = m.begin();
				if (it != m.end()) {
					SCRIPT_WRITE_STRING(it->first);
					m.pushValue(it);
				}
			}
		SCRIPT_CALLBACK_RETURN;
	}

};

}
