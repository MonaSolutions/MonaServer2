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
#include "Mona/ServerAPI.h"

namespace Mona {


struct Publish : virtual Object {
	NULLABLE

	Publish(ServerAPI& api, const char* name);
	~Publish();

	operator bool() const { return _pPublishing.unique() ? _pPublishing->operator bool() : true; }

	bool audio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { return queue<Write<Media::Audio>>(tag, packet, track); }
	bool video(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { return queue<Write<Media::Video>>(tag, packet, track); }
	bool data(Media::Data::Type type, const Packet& packet, UInt8 track = 0)		 { return queue<Write<Media::Data>>(type, packet, track); }
	bool properties(DataReader& reader, UInt8 track = 1)							 { return queue<Write<Media::Data>>(reader, track); }
	bool lost(UInt32 lost)															 { return queue<Lost>(Media::TYPE_NONE, lost); }
	bool lost(Media::Type type, UInt32 lost, UInt8 track = 0)						 { return queue<Lost>(type, lost, track); }

	bool reset();
	bool flush(UInt16 ping=0);



private:
	struct Publishing : Runner, virtual Object {
		NULLABLE
		Publishing(ServerAPI& api, const char* name) : Runner("Publishing"), _name(name), _pPublication(NULL), api(api) {}
		operator Publication&() { return *_pPublication; }
		operator bool() const { return _pPublication ? true : false; }
		ServerAPI& api;
	private:
		bool run(Exception& ex) { return (_pPublication = api.publish(ex, _name)) ? true : false; }
		std::string	 _name;
		Publication* _pPublication;
	};

	struct Action : Runner, virtual Object {
		Action(const char* name, const shared<Publishing>& pPublishing) : Runner(name), _pPublishing(pPublishing) {}
	protected:
		ServerAPI& api() { return _pPublishing->api; }
	private:
		virtual void run(Publication& publication) = 0;
		bool run(Exception& ex) { if (*_pPublishing) run(*_pPublishing); return true; }
		shared<Publishing> _pPublishing;
	};

	template<typename MediaType>
	struct Write : Action, MediaType, virtual Object {
		template<typename ...Args>
		Write(const shared<Publishing>& pPublishing, Args&&... args) : Action(typeof<Write<MediaType>>().c_str(), pPublishing), MediaType(args ...) {}
	private:
		void run(Publication& publication) { publication.writeMedia(*this); }
	};

	struct Lost : Action, virtual Object {
		Lost(const shared<Publishing>& pPublishing, Media::Type type, UInt32 lost, UInt8 track = 0) : Action("Publish::Lost", pPublishing), _type(type), _lost(lost), _track(track) {}
	private:
		void run(Publication& publication) { publication.reportLost(_type, _lost, _track); }
		Media::Type		_type;
		UInt8			_track;
		UInt32			_lost;
	};


	template<typename Type, typename ...Args>
	bool queue(Args&&... args) {
		if (!operator bool())
			return false;
		if (_pPublishing->api.queue(std::make_shared<Type>(_pPublishing, args ...)))
			return true;
		_pPublishing.reset(); // server stopped!
		return false;
	}

	mutable shared<Publishing>	_pPublishing;
};



} // namespace Mona
