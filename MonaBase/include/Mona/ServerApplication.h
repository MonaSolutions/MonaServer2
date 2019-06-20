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
#include "Mona/Application.h"
#include "Mona/TerminateSignal.h"
#if defined(_WIN32)
#include "Mona/WinService.h"
#endif

namespace Mona {

struct ServerApplication : Application, virtual Object {
	ServerApplication() : _isInteractive(true)
#if defined(_WIN32)
	, _service(WinService::STARTUP_DISABLED)
#endif
	{ _PThis = this; }

	bool	isInteractive() const { return _isInteractive; }

    int		run(int argc, const char** argv);

protected:
	void defineOptions(Exception& ex, Options& options);

private:
    virtual int main(TerminateSignal& terminateSignal) = 0;
	int			main() { return Application::EXIT_OK; }

	bool		_isInteractive;

    static ServerApplication*	 _PThis;
	static TerminateSignal		 _TerminateSignal;

#if defined(_WIN32)
	static int  __stdcall ConsoleCtrlHandler(DWORD ctrlType);
	static void __stdcall ServiceMain(DWORD argc, char** argv);
	static void __stdcall ServiceControlHandler(DWORD control);

	bool hasConsole();
	void registerService();
	void unregisterService();

	WinService::Startup _service;

#else

	std::string _pidFile;
#endif
};


} // namespace Mona
