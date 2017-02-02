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
#include "Mona/Application.h"
#include "Mona/TerminateSignal.h"

namespace Mona {

class ServerApplication : public Application, public virtual Object {
public:
	ServerApplication() : _isInteractive(true) { _PThis = this; }

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
	static int  __stdcall ConsoleCtrlHandler(unsigned long ctrlType);
	static void __stdcall ServiceMain(unsigned long argc, char** argv);
	static void __stdcall ServiceControlHandler(unsigned long control);

	bool hasConsole();
	bool isService();
	bool registerService(Exception& ex);
	bool unregisterService(Exception& ex);

	std::string _displayName;
	std::string _description;
	std::string _startup;

#else
	bool handlePidFile(Exception& ex,const std::string& value);
    bool isDaemon(int argc, const char** argv);
	void beDaemon();

	std::string _pidFile;
#endif
};


} // namespace Mona
