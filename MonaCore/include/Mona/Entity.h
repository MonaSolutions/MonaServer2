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

#include "Mona/Mona.h"
#include "Mona/Util.h"

namespace Mona {

struct Entity : virtual Object {
	struct Comparator { bool operator()(const UInt8* a, const UInt8* b) const { return memcmp(a, b, SIZE)<0; } };
	template<typename EntityType>
	struct Map : std::map<const UInt8*, EntityType*, Comparator> {
		using std::map<const UInt8*, EntityType*, Comparator>::map;
		typedef typename std::map<const UInt8*, EntityType*, Comparator>::iterator		 iterator;
		typedef typename std::map<const UInt8*, EntityType*, Comparator>::const_iterator const_iterator;
		std::pair<iterator, bool>	emplace(EntityType& entity) { return std::map<const UInt8*, EntityType*, Comparator>::emplace(entity, &entity); }
		iterator					emplace_hint(const_iterator it, EntityType& entity) { return std::map<const UInt8*, EntityType*, Comparator>::emplace_hint(it, entity, &entity); }
	private:
		void insert() = delete;
		void operator[](const UInt8*) = delete;
	};

	enum {
		SIZE = 32
	};
	Entity() : id() { Util::Random(BIN id, SIZE); }
	Entity(const UInt8* id) : id() { std::memcpy(BIN this->id, id, SIZE);	}

	bool operator==(const Entity& other) const { return std::memcmp(id, other.id, SIZE) == 0; }
	bool operator==(const UInt8* id) const { return std::memcmp(this->id, id, SIZE) == 0; }
	bool operator!=(const Entity& other) const { return std::memcmp(id, other.id, SIZE) != 0; }
	bool operator!=(const UInt8* id) const { return std::memcmp(this->id, id, SIZE) != 0; }
	bool operator <  (const Entity& other) const { return std::memcmp(this->id, other.id, SIZE) < 0; }
	bool operator <  (const UInt8* id) const { return std::memcmp(this->id, id, SIZE) < 0; }
	bool operator <= (const Entity& other) const { return std::memcmp(this->id, other.id, SIZE) <= 0; }
	bool operator <= (const UInt8* id) const { return std::memcmp(this->id, id, SIZE) <= 0; }
	bool operator >  (const Entity& other) const { return std::memcmp(this->id, other.id, SIZE) > 0; }
	bool operator >  (const UInt8* id) const { return std::memcmp(this->id, id, SIZE) > 0; }
	bool operator >= (const Entity& other) const { return std::memcmp(this->id, other.id, SIZE) >= 0; }
	bool operator >= (const UInt8* id) const { return std::memcmp(this->id, id, SIZE) >= 0; }

	const UInt8 id[SIZE];
	operator const UInt8*() const { return id; }
};


} // namespace Mona
