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
#include "Mona/Server.h"

namespace Mona {

/*!
Publish a publication by the code, externally to the server:
1 - shared<Publish> pPublish = server.action<Publish>(name);
2 - Use pPublish methods to write medias and flush
3 - Check *pPublish to test if publication is still active (not failed)
4 - Start again at point 2 or delete pPublish to terminate publication	*/
struct Publish : Media::Source, Server::Action, virtual Object {
	NULLABLE(!_api.running() || !*_ppSource) // Test if publication is always valid (not failed)
	Publish(ServerAPI& api, const char* name);
	Publish(ServerAPI& api, Media::Source& source);
	~Publish();
	/*!
	Publication name */
	const std::string& name() const { return _pName ? *_pName : (*_ppSource)->name(); }
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
	void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0);
	/*!
	Flush writing, call one time writing queue empty (with optional publisher current ping to give publication latency information) */
	void flush(UInt16 ping);
	void flush() { flush(0); } // overrides Media::Source::flush()
	/*!
	Reset the stream, usefull when a new stream will start on same publication */
	void reset();

	struct Logger : virtual Object, Mona::Logger {
		Logger(Publish& publish) : _publish(publish) {}
		bool log(LOG_LEVEL level, const Path& file, long line, const std::string& message) { writeData(String::Log(Logs::LevelToString(level), file, line, message, Thread::CurrentId())); return true;	}
		bool dump(const  std::string& header, const UInt8* data, UInt32 size) { writeData(header, '\n', String::Data(data, size)); return true; }
	private:
		template<typename ...Args>
		void writeData(Args&&... args) {
			if (!_publish)
				return;
			shared<Buffer> pBuffer(SET);
			String::Append(*pBuffer, std::forward<Args>(args)...);
			_publish.writeData(Media::Data::TYPE_TEXT, Packet(pBuffer));
			_publish.flush();
		}
		Publish& _publish;
	};
private:

	void run(ServerAPI& api) { Exception ex; *_ppSource = api.publish(ex, name()); }

	struct Action : Runner, virtual Object {
		Action(const char* name, const shared<Source*>& ppSource, bool isPublication=false) : Runner(name), _ppSource(ppSource), _isPublication(isPublication) {}
	private:
		virtual void run(Publication& publication) { run((Source&)publication); }
		virtual void run(Source& source) { ERROR(name, " empty run for ", source.name()); }

		bool				run(Exception& ex);
		shared<Source*>		_ppSource;
		bool				_isPublication;
	};

	template<typename MediaType>
	struct Write : Action, private MediaType, virtual Object {
		template<typename ...Args>
		Write(const shared<Source*>& ppSource, Args&&... args) : Action(typeof<Write<MediaType>>().c_str(), ppSource), MediaType(std::forward<Args>(args)...) {}
		void run(Source& source) { source.writeMedia(self); }
	};

	template<typename Type, typename ...Args>
	void queue(Args&&... args) {
		if (!*_ppSource)
			ERROR("Publish ", name(), " has failed, impossible to ", typeof<Type>())
		else if (!_api.handler.tryQueue<Type>(_ppSource, std::forward<Args>(args)...))
			ERROR("Server stopped, impossible to ", typeof<Type>(), " ", name());
	}

	unique<std::string>	_pName; // if is set => source is a publication (otherwise _ppSource points to source constructor argument)
	shared<Source*>		_ppSource; // if Source*==NULL => failed (always set otherwise)
	ServerAPI&			_api;
};



} // namespace Mona
