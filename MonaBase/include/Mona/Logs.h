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

#pragma once

#include "Mona/Mona.h"
#include "Mona/ConsoleLogger.h"
#include "Mona/String.h"
#include "Mona/Thread.h"

namespace Mona {

/*!
Mona Logs API, contains statis methods to manage logs */
struct Logs : virtual Static {
	/*!
	Add a logger target, must implements Logger.h interface
	If name have a suffix with the form ![error] then the application shutdown on failure and displays error if set */
	template <typename LoggerType, typename ...Args>
	static bool			AddLogger(const char* name, Args&&... args) { return AddLogger<LoggerType>(std::string(name), std::forward<Args>(args) ...); }
	template <typename LoggerType, typename ...Args>
	static bool			AddLogger(std::string&& name, Args&&... args) {
		size_t fatalPos = name.find('!');
		if (fatalPos != std::string::npos)
			name[fatalPos] = 0;
		std::lock_guard<std::mutex> lock(_Mutex);
		auto& it = _Loggers.emplace(std::move(name), make_unique<LoggerType>(std::forward<Args>(args) ...));
		it.first->second->name = it.first->first.c_str();
		if (fatalPos != std::string::npos)
			it.first->second->fatal = it.first->first.c_str() + fatalPos + 1;
		return it.second;
	}
	/*!
	Remove a logger */
	static void			RemoveLogger(const char* name) { std::lock_guard<std::mutex> lock(_Mutex); _Loggers.erase(name); }
	/*!
	Set LOG level */
	static void			SetLevel(LOG_LEVEL level) { _Level = level; }
	/*!
	Get LOG level  */
	static LOG_LEVEL	GetLevel() { return _Level; }
	static const char*  LevelToString(LOG_LEVEL level) {
		static const char* Strings[] = { "NONE", "FATAL", "CRITIC", "ERROR", "WARN", "NOTE", "INFO", "DEBUG", "TRACE" };
		return Strings[level];
	}

	static void			SetDumpLimit(Int32 limit) { std::lock_guard<std::mutex> lock(_Mutex); _DumpLimit = limit; }
	static void			SetDump(const char* name); // if null, no dump, otherwise dump name, and if name is empty everything is dumped
	static bool			IsDumping() { return _Dumping; }

	template <typename ...Args>
    static void	Log(LOG_LEVEL level, const char* file, long line, Args&&... args) {
		if (_Level < level)
			return;
		std::lock_guard<std::mutex> lock(_Mutex);
		static Path File;
		static String Message;
		File.set(file);
		String::Assign(Message, std::forward<Args>(args)...);
		for (auto& it : _Loggers) {
			if (*it.second && !it.second->log(level, File, line, Message))
				_Loggers.fail(*it.second);
		}
		if(Message.size()>0xFF) {
			Message.resize(0xFF);
			Message.shrink_to_fit();
		}
		_Loggers.flush();
	}

	template <typename ...Args>
	static void Dump(const char* name, const UInt8* data, UInt32 size, Args&&... args) {
		if (!_Dumping)
			return;
		std::lock_guard<std::mutex> lock(_Mutex);
		if (_Dump.empty() || String::ICompare(_Dump, name) == 0)
			Dump(String(std::forward<Args>(args)...), data, size);
	}

	template <typename ...Args>
	static void DumpRequest(const char* name, const UInt8* data, UInt32 size, Args&&... args) {
		if (_DumpRequest)
			Dump(name, data, size, std::forward<Args>(args)...);
	}

	template <typename ...Args>
	static void DumpResponse(const char* name, const UInt8* data, UInt32 size, Args&&... args) {
		if (_DumpResponse)
			Dump(name, data, size, std::forward<Args>(args)...);
	}

#if defined(_DEBUG)
	// To dump easly during debugging => no name filter = always displaid even if no dump argument
	static void Dump(const UInt8* data, UInt32 size) {
		std::lock_guard<std::mutex> lock(_Mutex);
		Dump(String::Empty(), data, size);
	}
#endif


private:
	static void		Dump(const std::string& header, const UInt8* data, UInt32 size);

	static std::mutex				_Mutex;

	static std::atomic<LOG_LEVEL>	_Level;
	static struct Loggers : std::map<std::string, unique<Logger>, String::IComparator>, virtual Object {
		Loggers() { emplace("console", new ConsoleLogger()); }
		void fail(Logger& logger) { _failed.emplace_back(&logger); }
		void flush();
	private:
		std::vector<Logger*> _failed;
	}								_Loggers;

	static volatile bool	_Dumping;
	static std::string		_Dump; // empty() means all dump, otherwise is a dump filter

	static volatile bool	_DumpRequest;
	static volatile bool	_DumpResponse;
	static Int32			_DumpLimit; // -1 means no limit
};

#undef ERROR
#undef DEBUG
#undef TRACE

#define DUMP(NAME,...) { if(Mona::Logs::IsDumping()) Mona::Logs::Dump(NAME,__VA_ARGS__); }

#define DUMP_REQUEST(NAME, DATA, SIZE, ADDRESS) { if(Mona::Logs::IsDumping()) Mona::Logs::DumpRequest(NAME, DATA, SIZE, NAME, " <= ", ADDRESS); }
#define DUMP_REQUEST_DEBUG(NAME, DATA, SIZE, ADDRESS) if(Logs::GetLevel() >= Mona::LOG_DEBUG) DUMP_REQUEST(NAME, DATA, SIZE, ADDRESS)

#define DUMP_RESPONSE(NAME, DATA, SIZE, ADDRESS) { if(Mona::Logs::IsDumping()) Mona::Logs::DumpResponse(NAME, DATA, SIZE, NAME, " => ", ADDRESS); }
#define DUMP_RESPONSE_DEBUG(NAME, DATA, SIZE, ADDRESS) if(Logs::GetLevel() >= Mona::LOG_DEBUG) DUMP_RESPONSE(NAME, DATA, SIZE, ADDRESS)

#define LOG(LEVEL, ...)  { if(Mona::Logs::GetLevel()>=(LEVEL)) { Mona::Logs::Log((LEVEL), __FILE__,__LINE__, __VA_ARGS__); } }

#define FATAL(...)	LOG(Mona::LOG_FATAL, __VA_ARGS__)
#define CRITIC(...) LOG(Mona::LOG_CRITIC, __VA_ARGS__)
#define ERROR(...)	LOG(Mona::LOG_ERROR, __VA_ARGS__)
#define WARN(...)	LOG(Mona::LOG_WARN, __VA_ARGS__)
#define NOTE(...)	LOG(Mona::LOG_NOTE, __VA_ARGS__)
#define INFO(...)	LOG(Mona::LOG_INFO, __VA_ARGS__)
#define DEBUG(...)	LOG(Mona::LOG_DEBUG, __VA_ARGS__)
#define TRACE(...)	LOG(Mona::LOG_TRACE, __VA_ARGS__)

#define AUTO_FATAL(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { FATAL_ERROR( __VA_ARGS__,", ", ex) } }
#define AUTO_CRITIC(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { CRITIC( __VA_ARGS__,", ", ex) } }
#define AUTO_ERROR(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { ERROR( __VA_ARGS__,", ", ex) } }
#define AUTO_WARN(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { WARN( __VA_ARGS__,", ", ex) } }



} // namespace Mona
