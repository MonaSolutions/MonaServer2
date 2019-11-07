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


#include "Mona/ServerApplication.h"
#include "Mona/Exceptions.h"
#include "Mona/HelpFormatter.h"
#if !defined(_WIN32)
#include "Mona/Process.h"
#include "Mona/File.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


ServerApplication*		ServerApplication::_PThis(NULL);
TerminateSignal			ServerApplication::_TerminateSignal;


#if defined(_WIN32)


static SERVICE_STATUS			_ServiceStatus;
static SERVICE_STATUS_HANDLE	_ServiceStatusHandle(0);

void  ServerApplication::ServiceControlHandler(DWORD control) {
	switch (control) {
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
		_TerminateSignal.set();
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	}
	SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
}


BOOL ServerApplication::ConsoleCtrlHandler(DWORD ctrlType) {
	switch (ctrlType) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		_TerminateSignal.set();
		return TRUE;
	default:
		return FALSE;
	}
}

void ServerApplication::ServiceMain(DWORD argc, char** argv) {
	_PThis->setBoolean("application.runAsService", true);
	_PThis->_isInteractive = false;

	memset(&_ServiceStatus, 0, sizeof(_ServiceStatus));
	_ServiceStatusHandle = RegisterServiceCtrlHandlerA("", ServiceControlHandler);
	if (!_ServiceStatusHandle) {
		FATAL_ERROR("Cannot register service control handler");
		return;
	}
	_ServiceStatus.dwServiceType = SERVICE_WIN32;
	_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	_ServiceStatus.dwWin32ExitCode = 0;
	_ServiceStatus.dwServiceSpecificExitCode = 0;
	_ServiceStatus.dwCheckPoint = 0;
	_ServiceStatus.dwWaitHint = 0;
	SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);

#if !defined(_DEBUG)
	try {
#endif

		if (_PThis->init(argc, const_cast<LPCSTR*>(argv))) {
			_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
			int rc = _PThis->main(_TerminateSignal);
			_ServiceStatus.dwWin32ExitCode = rc ? ERROR_SERVICE_SPECIFIC_ERROR : 0;
			_ServiceStatus.dwServiceSpecificExitCode = rc;
		}

#if !defined(_DEBUG)
	} catch (exception& ex) {
		FATAL(ex.what());
		_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		_ServiceStatus.dwServiceSpecificExitCode = EXIT_SOFTWARE;
	} catch (...) {
		FATAL("Unknown error");
		_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		_ServiceStatus.dwServiceSpecificExitCode = EXIT_SOFTWARE;
	}
#endif

	_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
}


int ServerApplication::run(int argc, const char** argv) {
#if !defined(_DEBUG)
	try {
#endif
		if (argc==2 && FileSystem::IsFolder(argv[1]) && !hasConsole()) {
			// is service just if no console + argc==2 + arv[1] is the current folder (see WinService::registerService)
			SERVICE_TABLE_ENTRY svcDispatchTable[2];
			svcDispatchTable[0].lpServiceName = "";
			svcDispatchTable[0].lpServiceProc = ServiceMain;
			svcDispatchTable[1].lpServiceName = NULL;
			svcDispatchTable[1].lpServiceProc = NULL;
			// is service, change current directory!
			if (!SetCurrentDirectory(argv[1]))
				FATAL_ERROR("Cannot set current directory of ", argv[0]); // useless to continue, the application could not report directory error (no logs, no init, etc...)
			return StartServiceCtrlDispatcherA(svcDispatchTable) ? EXIT_OK : EXIT_CONFIG;
		}

		SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

		if (!init(argc, argv))
			return EXIT_OK;
		if (_service) {
			registerService();
			return EXIT_OK;
		}
		if (hasKey("arguments.unregisterService")) {
			unregisterService();
			return EXIT_OK;
		}
		return main(_TerminateSignal);
#if !defined(_DEBUG)
	} catch (exception& ex) {
		FATAL(ex.what());
		return EXIT_SOFTWARE;
	} catch (...) {
		FATAL("Unknown error");
		return EXIT_SOFTWARE;
	}
#endif
}


bool ServerApplication::hasConsole() {
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	return hStdOut != INVALID_HANDLE_VALUE && hStdOut != NULL;
}


void ServerApplication::registerService() {
	WinService service(name());
	Exception ex;
	if (service.registerService(ex, file())) {
		service.setStartup(ex, _service);
		const char* description = getString("description");
		if (description)
			service.setDescription(ex, description);
		if (ex)
			WARN(ex);
		NOTE(name(), " has been successfully registered as a service");
	} else
		ERROR(ex);
}


void ServerApplication::unregisterService() {
	WinService service(name());
	Exception ex;
	if (service.unregisterService(ex)) {
		if (ex)
			WARN(ex);
		NOTE(name(), " is no more registered as a service");
	} else
		ERROR(ex);
}

void ServerApplication::defineOptions(Exception& ex,Options& options) {

	options.add(ex, "registerService", "r", String("Register ", name()," as a service."))
		.argument("manual|auto", false)
		.handler([this](Exception& ex, const char* value) {
			_service =( value && String::ICompare(value, EXPAND("auto")) == 0) ? WinService::STARTUP_AUTO : WinService::STARTUP_MANUAL;
			return true;
		});

	options.add(ex, "unregisterService", "u", String("Unregister ", name(), " as a service."));

	Application::defineOptions(ex, options);
}

#else

//
// Unix specific code
//

int ServerApplication::run(int argc, const char** argv) {
	int result(EXIT_OK);
#if !defined(_DEBUG)
	try {
#endif
		

		// define the "service" option now to fork on low level process!
		const char** argV = argv;
		int argC = argc;
		for (int i = 1; i < argc; ++i) {
			if (Option::Parse(argv[i]))
				break;
			++argV; // ignore the process fileName & the configuration path if set
			--argC;
		}
		Exception ex;
		Options& options = (Options&)this->options();
		string name;
		Option& option = options.add(ex, "service", "s", String("Run ", FileSystem::GetName(argv[0], name), " as a service."))
			.argument("pidFile", false)
			.handler([this](Exception& ex, const char* value) {
			// become service!
			pid_t pid;
			if ((pid = fork()) < 0)
				FATAL_ERROR("Cannot fork service process");
			if (pid != 0)
				exit(0);

			setsid();
			umask(0);

			// attach stdin, stdout, stderr to /dev/null
			// instead of just closing them. This avoids
			// issues with third party/legacy code writing
			// stuff to stdout/stderr.
			FILE* fin = freopen("/dev/null", "r+", stdin);
			if (!fin)
				FATAL_ERROR("Cannot attach stdin to /dev/null");
			FILE* fout = freopen("/dev/null", "r+", stdout);
			if (!fout)
				FATAL_ERROR("Cannot attach stdout to /dev/null");
			FILE* ferr = freopen("/dev/null", "r+", stderr);
			if (!ferr)
				FATAL_ERROR("Cannot attach stderr to /dev/null");

			setBoolean("application.runAsService", true);
			_isInteractive = false;
			// write PID?
			if (!value)
				return true;
			File file(value, File::MODE_WRITE);
			if (!file.load(ex))
				return false;
			_pidFile = value;
			String id(Process::Id());
			return file.write(ex, id.data(), id.size());
		});
		{
			Util::Scoped<bool> scoped(options.ignoreUnknown, true);
			options.process(ex, argC, argV); // ignore error, will be better reported on the upper level
			option.handler(nullptr); // remove the handler!
		}

		if(init(argc, argv))
			result = main(_TerminateSignal);
#if !defined(_DEBUG)
	} catch (exception& ex) {
		FATAL( ex.what());
		result = EXIT_SOFTWARE;
	} catch (...) {
		FATAL("Unknown error");
		result = EXIT_SOFTWARE;
	}
#endif
	if (!_pidFile.empty()) {
		Exception ex;
		FileSystem::Delete(ex,_pidFile);
		ERROR("pid file deletion, ",ex);
	}
	return result;
}

void ServerApplication::defineOptions(Exception& ex, Options& options) {

	Application::defineOptions(ex, options);
}


#endif


} // namespace Mona
