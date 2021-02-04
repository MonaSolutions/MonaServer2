/*
This code is in part based on code from the POCO C++ Libraries,
licensed under the Boost software license :
https://www.boost.org/LICENSE_1_0.txt

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

#pragma once

#include "Mona/Mona.h"
#include "Mona/Timer.h"

namespace Mona {

/*!
Class to manage expirable named and typed resources */
class Resources : virtual Object {
	struct Resource : virtual Object {
		Resource(const char* name, const std::type_info& type) : name(name), type(type), pObject(NULL) {}
		const std::string			name;
		const std::type_info&		type;
		mutable Timer::OnTimer		onTimer;
		mutable void*				pObject;
	};
public:
	typedef Event<void(const std::string& name, const std::string& type, UInt32 lifeTime)>	ON(Create);
	typedef Event<void(const std::string& name, const std::string& type)>					ON(Release);
	NULLABLE(!count())

	Resources(const Timer& timer) : timer(timer) {}
	~Resources() { clear(); }

	const Timer& timer;
	UInt32 count() const { return _resources.size(); }

	template<typename Type>
	Type* use(const std::string& name) { return use<Type>(name.c_str()); }
	template<typename Type>
	Type* use(const char* name) {
		const auto& it = _resources.find(Resource(name, typeid(Type)));
		if (it == _resources.end())
			return NULL;
		return (Type*)it->pObject;
	}

	template <typename Type, typename ...Args>
	Type& create(const std::string& name, UInt32 lifeTime, Args&&... args) { return create<Type>(name.c_str(), lifeTime, std::forward<Args>(args)...); }
	template<typename Type, typename ...Args>
	Type& create(const char* name, UInt32 lifeTime, Args&&... args) {
		const Resource& resource = *_resources.emplace(name, typeid(Type)).first;
		if (!resource.pObject) {
			resource.onTimer = [&](UInt32 delay) {
				delete (Type*)resource.pObject;
				onRelease(resource.name, TypeOf<Type>());
				_resources.erase(resource);
				return 0;
			};
		} else
			delete (Type*)resource.pObject;
		// create the object
		resource.pObject = new typename std::remove_cv<Type>::type(std::forward<Args>(args)...);
		// set the timer
		timer.set(resource.onTimer, lifeTime);
		onCreate(resource.name, TypeOf<Type>(), lifeTime);
		return *(Type*)resource.pObject;
	}

	template<typename Type>
	bool release(const std::string& name) { return release<Type>(name.c_str()); }
	template<typename Type>
	bool release(const char* name) {
		const auto& it = _resources.find(Resource(name, typeid(Type)));
		if (it == _resources.end())
			return false;
		timer.set(it->onTimer, 0);
		// to erase the object + remove of the collection
		it->onTimer(0);
		return true;
	}

	void clear() {
		// delete resources
		while (!_resources.empty()) {
			timer.set(_resources.begin()->onTimer, 0);
			// to erase the object + remove of the collection
			_resources.begin()->onTimer(0);
		}
		_resources.clear();
	}

private:
	struct Comparator {
		bool operator()(const Resource& r1, const Resource& r2) const {
			int cmp = r1.name.compare(r2.name);
			if (cmp)
				return cmp<0;
			return r1.type.hash_code() < r2.type.hash_code();
		}
	};
	std::set<Resource, Comparator>	_resources;
};

} // namespace Mona
