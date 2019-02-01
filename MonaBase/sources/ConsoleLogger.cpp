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

#include "Mona/ConsoleLogger.h"
#include <iostream>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace std;

namespace Mona {

#if defined(_WIN32)
#define FATAL_COLOR 12
#define CRITIC_COLOR 12
#define ERROR_COLOR 13
#define WARN_COLOR 14
#define NOTE_COLOR 10
#define INFO_COLOR 15
#define DEBUG_COLOR 7
#define TRACE_COLOR 8
#define BEGIN_CONSOLE_TEXT_COLOR(color) static HANDLE ConsoleHandle(GetStdHandle(STD_OUTPUT_HANDLE)); SetConsoleTextAttribute(ConsoleHandle, color)
#define END_CONSOLE_TEXT_COLOR			SetConsoleTextAttribute(ConsoleHandle, LevelColors[6])
#else
#define FATAL_COLOR "\033[01;31m"
#define	CRITIC_COLOR "\033[01;31m"
#define ERROR_COLOR "\033[01;35m"
#define WARN_COLOR "\033[01;33m"	
#define NOTE_COLOR "\033[01;32m"
#define INFO_COLOR "\033[01;37m"
#define DEBUG_COLOR "\033[0m"
#define TRACE_COLOR "\033[01;30m"
#define BEGIN_CONSOLE_TEXT_COLOR(color) printf("%s", color)
#define END_CONSOLE_TEXT_COLOR			printf("%s", LevelColors[6])
#endif

#if defined(_WIN32)
static int			LevelColors[] = { FATAL_COLOR, CRITIC_COLOR, ERROR_COLOR, WARN_COLOR, NOTE_COLOR, INFO_COLOR, DEBUG_COLOR, TRACE_COLOR };
#else
static const char*  LevelColors[] = { FATAL_COLOR, CRITIC_COLOR, ERROR_COLOR, WARN_COLOR, NOTE_COLOR, INFO_COLOR, DEBUG_COLOR, TRACE_COLOR };
#endif

ConsoleLogger::ConsoleLogger() {
#if defined(_WIN32)
	_isInteractive = _isatty(_fileno(stdout)) ? true : false;
#else
	_isInteractive = isatty(STDOUT_FILENO) ? true : false;
#endif
}

bool ConsoleLogger::log(LOG_LEVEL level, const Path& file, long line, const string& message) {
	BEGIN_CONSOLE_TEXT_COLOR(LevelColors[level - 1]);
	printf("%s[%ld] %s", file.name().c_str(), line, message.c_str());
	END_CONSOLE_TEXT_COLOR;
	printf("\n"); // flush after color change, required especially over unix/linux
	return true;
}

bool ConsoleLogger::dump(const string& header, const UInt8* data, UInt32 size) {
	if(!header.empty())
		printf("%.*s\n", (int)header.size(), header.c_str());
	fwrite(data, sizeof(char), size, stdout);
	return true;
}

} // namespace Mona
