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
#include "Mona/Media.h"

namespace Mona {

/*!
Publish a publication by the code, externally to the server:
1 - Publish* pPublish = server.publish(name)
2 - Use its functions to write medias and flush
3 - Check *pPublish to test if publication is still active (not failed)
4 - Start again at point 2 or delete pPublish to terminate publication	*/
struct Publish : Media::Source, virtual Object {
	NULLABLE
	Publish(ServerAPI& api, const char* name);
	~Publish();
	/*!
	Publication name */
	const std::string& name() const { return _pPublishing->name; }
	/*!
	Test if publication is always valid (not failed) */
	operator bool() const { return _pPublishing->api.running() && *_pPublishing; }
	/*!
	Write audio packet */
	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track=1) { queue<Write<Media::Audio>>(tag, packet, track); }
	/*!
	Write video packet */
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track=1) { queue<Write<Media::Video>>(tag, packet, track); };
	/*!
	Write data packet, used for subtitle, CC or RPC when client compatible */
	void writeData(Media::Data::Type type, const Packet& packet, UInt8 track=0) { queue<Write<Media::Data>>(type, packet, track); }
	/*!
	Set metadata to the stream */
	void setProperties(Media::Data::Type type, const Packet& packet, UInt8 track=1) { queue<Write<Media::Data>>(type, packet, track, true); }
	/*!
	Report lost detection, allow to displays stats and repair the stream in waiting next key frame when happen */
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { queue<Lost>(type, lost, track); }
	/*!
	Flush writing, call one time writing queue empty (with optional publisher current ping to give publication latency information) */
	void flush(UInt16 ping=0);
	/*!
	Reset the stream, usefull when a new stream will start on same publication */
	void reset();

private:
	void flush() { flush(0); }

	struct Publishing : Runner, virtual Object {
		NULLABLE
		Publishing(ServerAPI& api, const char* name) : _failed(false), Runner("Publishing"), name(name), api(api) {}
		const std::string name;
		operator bool() const { return !_failed; }

		ServerAPI& api;
		explicit operator Publication*() { return _failed ? NULL : _pPublication; }
	private:
		bool run(Exception& ex);

		Publication* _pPublication;
		volatile bool _failed;
	};

	struct Action : Runner, virtual Object {
		Action(const char* name, const shared<Publishing>& pPublishing) : Runner(name), _pPublishing(pPublishing) {}
	protected:
		ServerAPI& api() { return _pPublishing->api; }
	private:
		virtual void run(Publication& publication) = 0;
		bool run(Exception& ex) { 
			Publication* pPublication(*_pPublishing);
			if(pPublication)
				run(*pPublication);
			return true;
		}
		shared<Publishing> _pPublishing;
	};

	template<typename MediaType>
	struct Write : Action, private MediaType, virtual Object {
		template<typename ...Args>
		Write(const shared<Publishing>& pPublishing, Args&&... args) : Action(typeof<Write<MediaType>>().c_str(), pPublishing), MediaType(args ...) {}
	private:
		void run(Publication& publication) { publication.writeMedia(self); }
	};

	struct Lost : Action, private Media::Base, virtual Object {
		Lost(const shared<Publishing>& pPublishing, Media::Type type, UInt32 lost, UInt8 track = 0) : Action("Publish::Lost", pPublishing), Media::Base(type, Packet::Null(), track), _lost(lost)  {}
	private:
		void run(Publication& publication) { publication.reportLost(type, _lost, track); }
		UInt32			_lost;
	};


	template<typename Type, typename ...Args>
	void queue(Args&&... args) {
		if (!*_pPublishing) {
			ERROR("Publication ", _pPublishing->name, " has failed, impossible to ", typeof<Type>());
			return;
		}
		_pPublishing->api.queue(new Type(_pPublishing, args ...));
	}

	shared<Publishing> _pPublishing;
};



} // namespace Mona
