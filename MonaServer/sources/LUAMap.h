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


template<typename ObjType, typename MapType = ObjType>
struct LUAMap : virtual Static {

	template<typename Push=Script>
	struct Mapper : virtual Object {
		Mapper(MapType& map, lua_State *pState) : pState(pState), map(map) {}

		UInt32 size(std::string* pPrefix) const {
			if (!pPrefix)
				return size();
			const auto& it = map.lower_bound(*pPrefix);
			pPrefix->back() += 1;
			return std::distance(it, map.lower_bound(*pPrefix));
		}

		bool get(const std::string& key) {
			static typename MapType::key_compare Less;
			const auto& it = map.lower_bound(key);
			if (it == map.end() || Less(key, it->first))
				return false;
			Push::PushValue(pState, it->second);
			return true;
		}

		bool next(const char* key, std::string* pPrefix) {
			static typename MapType::key_compare Less;
			auto it = key ? map.lower_bound(key) : map.begin();
			if (it == map.end() || (key && ++it == map.end()))
				return false;
			if (pPrefix) {
				if (!key) { // get the first prefixed key!
					it = map.lower_bound(*pPrefix);
					if (it == map.end())
						return false;
				} else {
					// if (Less(it->first, prefix)) break;  TRICK => NO check if the first data is inside to allow to iterate since the beginning until a limited prefix (and more efficient in other case)	
					++pPrefix->back();
					if (!Less(it->first, *pPrefix)) // check that we are always in the prefix range!
						return false;
				}
			}
			Script::PushString(pState, it->first);
			Push::PushValue(pState, it->second);
			return true;
		}

		/*!
		Erase an entry of the collection, return a postive or equal to 0 value on success, otherwise -1 */
		template<typename T = MapType, typename std::enable_if<std::is_const<T>::value, int>::type = 0>
		Int8 erase(const std::string& key) { return -1; }
		template<typename T = MapType, typename std::enable_if<!std::is_const<T>::value, int>::type = 0>
		Int8 erase(const std::string& key) { return Int8(map.erase(key)); }

		/*!
		Clear all or a prefix part of the collection, returns number of removed elements in a positive sign if full success, otherwise negative (removed elements+1) */
		template<typename T = MapType, typename std::enable_if<std::is_const<T>::value, int>::type = 0>
		int clear(std::string* pPrefix) { return -1; }
		template<typename T = MapType, typename std::enable_if<!std::is_const<T>::value, int>::type = 0>
		int clear(std::string* pPrefix) {
			if (!pPrefix) {
				UInt32 size = this->size();
				map.erase(map.begin(), map.end());
				return size;
			}
			const auto& it = map.lower_bound(*pPrefix);
			++pPrefix->back();
			const auto& itEnd = map.lower_bound(*pPrefix);
			return std::distance(it, map.erase(it, itEnd));
		}

		template<typename T = MapType, typename std::enable_if<std::is_const<T>::value, int>::type = 0>
		bool set(const std::string& key, ScriptReader& reader, DataReader& parameters) { return false; }
		template<typename T = MapType, typename std::enable_if<!std::is_const<T>::value, int>::type = 0>
		bool set(const std::string& key, ScriptReader& reader, DataReader& parameters) {
			std::string value;
			StringWriter<std::string> writer(value);
			reader.read(writer);
			Push::PushValue(pState, map.emplace(key, std::move(value)).first->second); // push set value!
			return true;
		}
	protected:
		MapType&   map;
		lua_State* pState;

	private:
		template<typename T = MapType, typename std::enable_if<std::is_convertible<typename std::remove_cv<T>::type, Parameters>::value, int>::type = 0>
		UInt32 size() const { return map.count(); }
		template<typename T = MapType, typename std::enable_if<!std::is_convertible<typename std::remove_cv<T>::type, Parameters>::value, int>::type = 0>
		UInt32 size() const { return map.size(); }
	};

	template<typename M = Mapper<>>
	static void Define(lua_State *pState, int indexObj) {
		SCRIPT_BEGIN(pState)
			SCRIPT_DEFINE("__tab", lua_pushvalue(pState, indexObj<0 ? (indexObj-1) : indexObj)); // to return itself on a double call to table operator
			SCRIPT_DEFINE_FUNCTION("__next", &(Next<M>));
			SCRIPT_DEFINE_FUNCTION("__call", &(Call<M>));
			SCRIPT_DEFINE_FUNCTION("__index", &(Index<M>));
		SCRIPT_END
	}

	template<typename M = Mapper<>>
	static int Call(lua_State* pState) {
		SCRIPT_CALLBACK(ObjType, obj)
			M m(obj, pState);
			// skip this if is the second argument (call with :)
			int hasThis = 0;
			if(SCRIPT_NEXT_READABLE && lua_getmetatable(pState, 1)) {
				if (lua_getmetatable(pState, 2)) {
					if(lua_equal(pState, -1, -2))
						hasThis = SCRIPT_READ_NEXT(1);
					lua_pop(pState, 1);
				}
				lua_pop(pState, 1);
			}

			int available = SCRIPT_NEXT_READABLE;
			std::string key;
			bool keying = ToKey(pState, 1, key);
			if (available > 0) {
				// SETTERS
				/// first argument is the value, the rest is setter parameter (can be used by HTTP SetCookie for example)
				ScriptReader reader(pState, UInt32(available), 1);
				ScriptReader parameters(pState, UInt32(available-1));
	
				if (reader.readNull()) {
					// erase value
					/// primitive object, erase just the related value!
					int count = keying && !std::is_const<MapType>::value ? m.erase(key) : -1;
					if (count < 0) {
						if (keying) {
							SCRIPT_ERROR("Impossible to erase ", key, " of ", typeof<ObjType>())
							keying = false;
						} else
							SCRIPT_ERROR("Use explicit table(params):clear() function to clear ", typeof<ObjType>());
						count = 0;
					}
					SCRIPT_WRITE_INT(count); // to match "clear" which returns size removed + just log error (error not detectable in LUA, too complicated to handle, just read the error)
				} else if (reader.nextType() >= ScriptReader::OTHER) {
					// complex type, add properties!
					struct Writer : MapWriter<Writer> {
						NULLABLE(_failed)
						Writer(M& m, std::string* pPrefix, ScriptReader& reader, DataReader& parameters) : _pPrefix(pPrefix), _failed(false), _reader(reader), _parameters(parameters), MapWriter<Writer>(self), _m(m) {
							if (!pPrefix)
								return;
							MapWriter<Writer>::beginObject();
							MapWriter<Writer>::writePropertyName(pPrefix->c_str());
						}
						~Writer() {
							if (_pPrefix)
								MapWriter<Writer>::endObject();
						}
						void emplace(const std::string& key, std::string&& value) {}
						void clear() {
							if (_m.clear(_pPrefix) < 0) {
								_failed = true;
								SCRIPT_BEGIN(_reader.lua())
									SCRIPT_ERROR("Impossible to clear ", typeof<ObjType>())
								SCRIPT_END
							}
						}
					private:
						bool setKey(const std::string& key) {
							if (!_m.set(key, _reader.nextType() < DataReader::OTHER ? _reader : ScriptReader::Null(), _parameters)) {
								_failed = true;
								SCRIPT_BEGIN(_reader.lua())
									SCRIPT_ERROR("Impossible to set ", key, " of ", typeof<ObjType>())
								SCRIPT_END
							} else
								lua_pop(_reader.lua(), 1); // remove new value pushed!
							_parameters.reset();
							return false; // block useless String build in MapWriter!
						}

						M&					_m;
						ScriptReader&		_reader;
						DataReader&			_parameters;
						bool				_failed;
						std::string*		_pPrefix;
					} writer(m, keying ? &key : NULL, reader, parameters);
					keying = false;
					lua_pushvalue(pState, reader.current()); /// myself = complex value!
					reader.read(writer);
					if (!writer) {
						lua_pop(pState, 1);
						SCRIPT_WRITE_NIL
					}
				} else {
					// primitive type
					if (!keying) { // call on root object => GET or SET
						StringWriter<std::string> writer(key);
						reader.read(writer);
						keying = available<2;
					} else // SET
						keying = false;
					if (!keying && (std::is_const<MapType>::value || !m.set(key, reader, parameters))) {
						SCRIPT_ERROR("Impossible to set ", key, " of ", typeof<ObjType>())
						SCRIPT_WRITE_NIL
					}
				}
				SCRIPT_READ_NEXT(reader.position() + parameters.position());

			} else if (keying) {
				if (hasThis) {
					std::size_t offset = key.rfind('.') + 1;
					if (key.compare(offset, std::string::npos, "len") == 0) {
						keying = false;
						// :len()
						SCRIPT_WRITE_INT(m.size(ToKey(pState, 2, key) ? &key : NULL));
					} else if (!std::is_const<MapType>::value && key.compare(offset, std::string::npos, "clear") == 0) {
						keying = false;
						// :clear()
						int count = m.clear(ToKey(pState, 2, key) ? &key : NULL);
						if (count < 0) {
							SCRIPT_ERROR("Impossible to clear right elements of ", typeof<ObjType>())
							count = -count+1;
						}
						SCRIPT_WRITE_INT(count);
					}
				}
			} else // No sub map and call without argument (no setter/getter), Script::Call normal!
				Script::Call<MapType>(pState);

			if (keying && !m.get(key)) {
				SCRIPT_WRITE_NIL
				if(!available) // else it's the call is in its form table("prop") => can return NIL without error
					SCRIPT_ERROR("attempt to index a nil value");
			}
	
		SCRIPT_CALLBACK_RETURN
	}


private:
	template<typename M = Mapper<>>
	static int Index(lua_State *pState) {
		if (!lua_gettop(pState)) {
			SCRIPT_BEGIN(pState)
				SCRIPT_TABLE_BEGIN(0)
					SCRIPT_DEFINE_STRING("len", "length()");
					if(!std::is_const<MapType>::value)
						SCRIPT_DEFINE_STRING("clear", "clear()")
				SCRIPT_TABLE_END
			SCRIPT_END
			return 1;
		}
		SCRIPT_CALLBACK(ObjType, obj)
			SCRIPT_READ_DATA(name, size);

			std::string key;
			if (ToKey(pState, 1, key))
				key += '.';
			key.append(STR name, size);

			// write key in 0 index (hidden)
			lua_newtable(pState);
			lua_pushlstring(pState, key.data(), key.size());
			lua_rawseti(pState, -2, 0);

			lua_getmetatable(pState, 1);
			lua_setmetatable(pState, -2);

		SCRIPT_CALLBACK_RETURN
	}

	template<typename M = Mapper<>>
	static int Next(lua_State* pState) {
		// 1- table
		// 2- key
		SCRIPT_CALLBACK(ObjType, obj);
			std::string prefix;
			M(obj, pState).next(SCRIPT_READ_STRING(NULL), ToKey(pState, 1, prefix) ? &prefix : NULL);
		SCRIPT_CALLBACK_RETURN;
	}


	static bool ToKey(lua_State *pState, int index, std::string& key) {
		// Key is in {[0] = KEY}
		lua_rawgeti(pState, index, 0);
		std::size_t size;
		const char* prefix = lua_tolstring(pState, -1, &size);
		lua_pop(pState, 1);
		if (!prefix)
			return false;
		key.assign(prefix, size);
		return true;
	}
};

}
