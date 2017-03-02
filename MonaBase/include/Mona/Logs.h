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
#include "Mona/Logger.h"
#include "Mona/String.h"
#include "Mona/Thread.h"

namespace Mona {

struct Logs : virtual Static {
	static void			SetLogger(Logger& logger) { std::lock_guard<std::mutex> lock(_Mutex); _PLogger = &logger; }

	static void			SetLevel(LOG_LEVEL level) { std::lock_guard<std::mutex> lock(_Mutex); _Level = level; }
	static LOG_LEVEL	GetLevel() { return _Level; }

	static void			SetDumpLimit(Int32 limit) { std::lock_guard<std::mutex> lock(_Mutex); _DumpLimit = limit; }
	static void			SetDump(const char* name); // if null, no dump, otherwise dump name, and if name is empty everything is dumped
	static bool			IsDumping() { return _PDump ? true : false; }

	template <typename ...Args>
    static void	Log(LOG_LEVEL level, const char* file, long line, Args&&... args) {
		std::lock_guard<std::mutex> lock(_Mutex);
		if (_Level < level)
			return;
		static Path File;
		static String Message;
		File.set(file);
		String::Assign(Message, std::forward<Args>(args)...);

		_PLogger->log(level, File, line, Message);
		if(Message.size()>0xFF) {
			Message.resize(0xFF);
			Message.shrink_to_fit();
		}
	}

	template <typename ...Args>
	static void Dump(const char* name, const UInt8* data, UInt32 size, Args&&... args) {
		shared<std::string> pDump(_PDump);
		if (pDump && (pDump->empty() || String::ICompare(*pDump, name) == 0))
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
	static void Dump(const UInt8* data, UInt32 size) { Dump(String::Empty(), data, size); }
#endif

private:

	static void Dump(const std::string& header, const UInt8* data, UInt32 size);


	static std::mutex	_Mutex;

	static LOG_LEVEL	_Level;
	static Logger		_DefaultLogger;
	static Logger*		_PLogger;

	static shared<std::string>	 _PDump; // NULL means no dump, empty() means all dump, otherwise is a dump filter
	static std::atomic<bool>			 _DumpRequest;
	static std::atomic<bool>			 _DumpResponse;
	static Int32						 _DumpLimit; // -1 means no limit
};

#undef ERROR
#undef DEBUG
#undef TRACE

#define LOG(LEVEL, ...)  { Mona::LOG_LEVEL __level(LEVEL); if(Mona::Logs::GetLevel()>=__level) { Mona::Logs::Log(__level, __FILE__,__LINE__, __VA_ARGS__); } }

#define FATAL(...)	LOG(Mona::LOG_FATAL, __VA_ARGS__)
#define CRITIC(...) LOG(Mona::LOG_CRITIC, __VA_ARGS__)
#define ERROR(...)	LOG(Mona::LOG_ERROR, __VA_ARGS__)
#define WARN(...)	LOG(Mona::LOG_WARN, __VA_ARGS__)
#define NOTE(...)	LOG(Mona::LOG_NOTE, __VA_ARGS__)
#define INFO(...)	LOG(Mona::LOG_INFO, __VA_ARGS__)
#define DEBUG(...)	LOG(Mona::LOG_DEBUG, __VA_ARGS__)
#define TRACE(...)	LOG(Mona::LOG_TRACE, __VA_ARGS__)

#define DUMP(NAME,...) { if(Mona::Logs::IsDumping()) Mona::Logs::Dump(NAME,__VA_ARGS__); }

#define DUMP_REQUEST(NAME, DATA, SIZE, ADDRESS) { if(Mona::Logs::IsDumping()) Mona::Logs::DumpRequest(NAME, DATA, SIZE, NAME, " <= ", ADDRESS); }
#define DUMP_REQUEST_DEBUG(NAME, DATA, SIZE, ADDRESS) if(Logs::GetLevel() >= Mona::LOG_DEBUG) DUMP_REQUEST(NAME, DATA, SIZE, ADDRESS)

#define DUMP_RESPONSE(NAME, DATA, SIZE, ADDRESS) { if(Mona::Logs::IsDumping()) Mona::Logs::DumpResponse(NAME, DATA, SIZE, NAME, " => ", ADDRESS); }
#define DUMP_RESPONSE_DEBUG(NAME, DATA, SIZE, ADDRESS) if(Logs::GetLevel() >= Mona::LOG_DEBUG) DUMP_RESPONSE(NAME, DATA, SIZE, ADDRESS)

#define AUTO_CRITIC(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { CRITIC( __VA_ARGS__,", ", ex) } }
#define AUTO_ERROR(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { ERROR( __VA_ARGS__,", ", ex) } }
#define AUTO_WARN(FUNCTION,...) { if((FUNCTION)) { if(ex)  WARN( __VA_ARGS__,", ", ex); } else { WARN( __VA_ARGS__,", ", ex) } }


} // namespace Mona
