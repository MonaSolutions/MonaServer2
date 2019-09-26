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
2 - Use pPublish methods to write medias and flush
3 - Check *pPublish to test if publication is still active (not failed)
4 - Start again at point 2 or delete pPublish to terminate publication	*/
struct Publish : Media::Source, virtual Object {
	NULLABLE
	Publish(ServerAPI& api, const char* name);
	Publish(ServerAPI& api, Media::Source& source);
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
	void addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) { queue<Write<Media::Data>>(type, packet, track, true); }
	/*!
	Report lost detection, allow to displays stats and repair the stream in waiting next key frame when happen */
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { queue<Lost>(type, lost, track); }
	/*!
	Flush writing, call one time writing queue empty (with optional publisher current ping to give publication latency information) */
	void flush(UInt16 ping=0);
	/*!
	Reset the stream, usefull when a new stream will start on same publication */
	void reset();

	struct Logger : virtual Object, Mona::Logger {
		Logger(Publish& publish) : _publish(publish) {}
		bool log(LOG_LEVEL level, const Path& file, long line, const std::string& message) { return writeData(String::Log(Logs::LevelToString(level), file, line, message, Thread::CurrentId())); }
		bool dump(const  std::string& header, const UInt8* data, UInt32 size) { return writeData(String::Date("%d/%m %H:%M:%S.%c  "), header, '\n'); }
	private:
		template<typename ...Args>
		bool writeData(Args&&... args) {
			if (!_publish)
				return true;
			shared<Buffer> pBuffer(SET);
			String::Append(*pBuffer, std::forward<Args>(args)...);
			_publish.writeData(Media::Data::TYPE_TEXT, Packet(pBuffer));
			return true;
		}
		Publish& _publish;
	};
private:
	void flush() { flush(0); }

	struct Publishing : Runner, virtual Object {
		NULLABLE
		Publishing(ServerAPI& api, const char* name) : _failed(false), Runner("Publishing"), name(name), api(api) {}
		Publishing(ServerAPI& api, Source& source) : _failed(false), _pSource(&source), Runner(NULL), name(source.name()), api(api) {}

		const std::string name;
		operator bool() const { return !_failed; }
		bool isPublication() const { return Runner::name && !_failed; }

		ServerAPI& api;
		explicit operator Source*() { return _failed ? NULL : _pSource; }
		explicit operator Publication*() { return _failed || !Runner::name ? NULL : (Publication*)_pSource; }

	private:
		bool run(Exception& ex);

		Source* _pSource;
		volatile bool _failed;
	};

	struct Action : Runner, virtual Object {
		Action(const char* name, const shared<Publishing>& pPublishing) : Runner(name), _pPublishing(pPublishing) {}
	protected:
		ServerAPI& api() { return _pPublishing->api; }
	private:
		virtual void run(Publication& publication) { run((Source&)publication); }
		virtual void run(Source& source) { ERROR(name," empty run for ", source.name()); }
		bool run(Exception& ex) { 
			if (!_pPublishing->isPublication()) {
				Source* pSource(*_pPublishing);
				if (pSource)
					run(*pSource);
			} else
				run(*(Publication*)*_pPublishing);
			return true;
		}
		shared<Publishing> _pPublishing;
	};

	template<typename MediaType>
	struct Write : Action, private MediaType, virtual Object {
		template<typename ...Args>
		Write(const shared<Publishing>& pPublishing, Args&&... args) : Action(typeof<Write<MediaType>>().c_str(), pPublishing), MediaType(args ...) {}
	private:
		void run(Source& source) { source.writeMedia(self); }
	};

	struct Lost : Action, private Media::Base, virtual Object {
		Lost(const shared<Publishing>& pPublishing, Media::Type type, UInt32 lost, UInt8 track = 0) : Action("Publish::Lost", pPublishing), Media::Base(type, Packet::Null(), track), _lost(lost)  {}
	private:
		void run(Source& source) { source.reportLost(type, _lost, track); }
		UInt32			_lost;
	};


	template<typename Type, typename ...Args>
	void queue(Args&&... args) {
		if (!*_pPublishing) {
			ERROR("Publication ", _pPublishing->name, " has failed, impossible to ", typeof<Type>());
			return;
		}
		_pPublishing->api.queue<Type>(_pPublishing, std::forward<Args>(args)...);
	}

	shared<Publishing> _pPublishing;
};



} // namespace Mona
