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
#include "Mona/Media.h"


namespace Mona {

/*!
A Media stream (sources or targets) allows mainly to bound a publication with input and ouput stream.
Its behavior must support an automatic mode to (re)start the stream as long time as desired
Implementation have to call protected stop(...) to log and callback an error if the user prefer delete stream on error
/!\ start() can be used to pulse the stream (connect attempt) */
struct MediaStream : virtual Object {
	typedef Event<void()>								ON(Running);
	typedef Event<void()>								ON(Stop);
	typedef Event<void()>								ON(Delete);
	typedef Event<void(const shared<Media::Target>&)>	ON(NewTarget); // valid until pTarget.unique()!

	enum Type {
		TYPE_LOGS = -2,
		TYPE_FILE = -1,
		TYPE_OTHER = 0, // > 0 => socket!
		TYPE_TCP = 1,
		TYPE_HTTP = 2,
		TYPE_SRT = 3,
		TYPE_UDP = 0x80 // >= 0x80 => socket UDP (without server ability)
	};
	static const char* TypeToString(MediaStream::Type type) {
		static const char *Strings[] = { "logs", "file", "other", "tcp", "http", "srt" }, *Udps[] = { "udp" };
		return type < TYPE_UDP ? Strings[type - TYPE_LOGS] : Udps[type - TYPE_UDP];
	}

	/*!
	Determinate the subMime format considering the media stream type, the request. */
	static const char* Format(Exception& ex, MediaStream::Type type, const char* request, Path& path);
	static const char* Format(Exception& ex, MediaStream::Type type, const std::string& request, Path& path) { return Format(ex, type, request.c_str(), path); }

	typedef std::function<unique<MediaStream>(const char* request, std::string&& format, Media::Source* pSource)> OnNewStream;
	/*!
	Add a custom Type, reutrn false if this type exists already!  */
	static bool AddType(std::string&& type, const OnNewStream& onNewStream) { return _OtherStreams.emplace(move(type), onNewStream).second; }

	enum State {
		STATE_STOPPED = 0,
		STATE_STARTING,
		STATE_RUNNING
	};
	
	/*!
	New Stream target => [protocol://host:port][/path/file.format] [format?params] */
	static unique<MediaStream> New(Exception& ex, const std::string& description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS = nullptr) { return New(ex, Media::Source::Null(), description, timer, ioFile, ioSocket, pTLS); }
	/*!
	New Stream target => [protocol://host:port][/path/file.format] [format?params]  */
	static unique<MediaStream> New(Exception& ex, Media::Source& source, const std::string& description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS = nullptr);


	const MediaStream::Type	type;
	const String			description;

	const Parameters&	params();
	bool				isSource() const { return &source == &Media::Source::Null(); }

	UInt32	runCount() const { return _runCount; };
	State	state() const { return _state; }

	Exception			ex;

	virtual shared<const Socket> socket() const;
	virtual shared<const File>	 file() const;

	/*!
	Children targets */
	const std::set<shared<const MediaStream>>& targets;

	bool start(const Parameters& parameters = Parameters::Null());
	void stop();


	~MediaStream();
protected:
	template <typename ...Args>
	MediaStream(Type type, Args&&... args) : _firstStart(true), description(std::forward<Args>(args)...),
		_state(STATE_STOPPED), targets(_targets), _runCount(0), type(type), source(Media::Source::Null()) {
		if (description.empty())
			String::Assign((std::string&)description, "Stream source ", TypeToString(type));
	}
	template <typename ...Args>
	MediaStream(Type type, Media::Source& source, Args&&... args) : _firstStart(true), description(std::forward<Args>(args)...),
		_state(STATE_STOPPED), targets(_targets), _runCount(0), type(type), source(source) {
		if (description.empty())
			String::Assign((std::string&)description, "Stream target ", TypeToString(type));
	}

	Media::Source& source;

	/*!
	Pass from state "starting" to "running" */
	bool run();
	void stop(LOG_LEVEL level, const Exception& exc) {
		LOG(level, description, ", ", ex = exc);
		stop();
	}
	template<typename ExType, typename ...Args>
	void stop(LOG_LEVEL level, Args&&... args) {
		LOG(level, description, ", ", ex.set<ExType>(std::forward<Args>(args)...));
		stop();
	}

	bool initSocket(shared<Socket>& pSocket, const Parameters& parameters, const shared<TLS>& pTLS = nullptr);

	template <typename StreamType, typename ...Args>
	StreamType* addTarget(Args&&... args) {
		if (!_state) {
			ERROR(description, ", child stream target authorized when start");
			return NULL;
		}
		STATIC_ASSERT(std::is_base_of<Media::Target, StreamType>::value);
		shared<StreamType> pTarget(SET, std::forward<Args>(args) ...);
		auto it = _targets.lower_bound(pTarget);
		if (it != _targets.end() && it->unique())
			it = _targets.erase(it); // target useless!
		pTarget->start(_params); // give same parameters than parent!
		if (!pTarget->state())
			return NULL;
		onNewTarget(pTarget);
		if (pTarget.unique())
			return NULL;
		// by default remove children on stop, add Target user can simply cancel it in reset onStop to null!
		pTarget->onStop = [this, &target = *pTarget]() {
			const auto& it = lower_bound(_targets, target, [](const std::set<shared<const MediaStream>>::const_iterator& it, StreamType& target) {
				return  ToPointer(*it) < ToPointer(target);
			});
			if (it != _targets.end() && it->get() == &target)
				_targets.erase(it);
		};
		_targets.emplace_hint(it, pTarget);
		// let's addTarget caller call start(params)
		return pTarget.get();
	}

private:
	/*!
	Call run() inside to finish starting (or call run() more later to finish starting) and returns true, finally stop() to cancel starting and returns false
	On first call state() = STATE_STOPPED
	/!\ Call be repeated until run() call! */
	virtual bool starting(const Parameters& parameters) = 0;
	virtual void stopping() = 0;

	UInt32								_runCount;
	std::set<shared<const MediaStream>>	_targets;
	shared<Media::Target>				_pTarget;
	bool								_firstStart;
	State								_state;
	Parameters							_params;

	static std::map<std::string, OnNewStream, String::IComparator>	_OtherStreams;
};


} // namespace Mona
