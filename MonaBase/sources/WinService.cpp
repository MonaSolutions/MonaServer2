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


#include "Mona/WinService.h"
#include "Mona/WinRegistryKey.h"
#include "Mona/Thread.h"
#include "Mona/FileSystem.h"

#if !defined(_WIN32_WCE)

using namespace std;


namespace Mona {

const int WinService::STARTUP_TIMEOUT = 30000;
const string WinService::REGISTRY_KEY("SYSTEM\\CurrentControlSet\\Services\\");
const string WinService::REGISTRY_DESCRIPTION("Description");

WinService::WinService(const string& name) :_name(name), _svcHandle(0), _scmHandle(0) {
}


WinService::~WinService() {
	close();
	if (_scmHandle)
		CloseServiceHandle(_scmHandle);
}

bool WinService::init(Exception& ex) const {
	if (_scmHandle || (_scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)))
		return true;
	ex.set<Ex::System::Service>("Cannot open Service Control Manager");
	return false;
}

bool WinService::open(Exception& ex) const {
	if (!init(ex))
		return false;
	if (_svcHandle || (_svcHandle = OpenServiceA(_scmHandle, _name.c_str(), SERVICE_ALL_ACCESS)))
		return true;
	ex.set<Ex::System::Service>("Service ", _name, " doesn't exist");
	return false;
}

void WinService::close() {
	if (_svcHandle) {
		CloseServiceHandle(_svcHandle);
		_svcHandle = 0;
	}
}


const string& WinService::getPath(Exception& ex, string& path) const {
	LPQUERY_SERVICE_CONFIGA pSvcConfig = config(ex);
	if (!pSvcConfig)
		return path;
	path.assign(pSvcConfig->lpBinaryPathName);
	LocalFree(pSvcConfig);
	return path;
}

const string& WinService::getDisplayName(Exception& ex, string& name) const {
	LPQUERY_SERVICE_CONFIGA pSvcConfig = config(ex);
	if (!pSvcConfig)
		return name;
	name.assign(pSvcConfig->lpDisplayName);
	LocalFree(pSvcConfig);
	return name;
}

bool WinService::registerService(Exception& ex,const string& path, const string& displayName) {
	if (!unregisterService(ex))
		return false;
	close();
	if (!init(ex))
		return false;
	_svcHandle = CreateServiceA(
		_scmHandle,
		_name.c_str(),
		displayName.c_str(), 
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		String(path, ' ', FileSystem::GetCurrentDir()).c_str(), // save the current directory = 2 params!
		NULL, NULL, NULL, NULL, NULL);
	if (_svcHandle || GetLastError() == ERROR_SERVICE_EXISTS)
		return true;
	ex.set<Ex::System::Service>("Cannot register service ", _name);
	return false;
}


bool WinService::unregisterService(Exception& ex) {
	if (!open(ex))
		return true;
	if (DeleteService(_svcHandle) || GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE)
		return true;
	ex.set<Ex::System::Service>("Cannot unregister service ", _name);
	return false;
}


bool WinService::registered(Exception& ex) const {
	if (!init(ex))
		return false;
	Exception ignore;
	return open(ignore);
}


bool WinService::running(Exception& ex) const {
	if (!open(ex))
		return false;
	SERVICE_STATUS ss;
	if (!QueryServiceStatus(_svcHandle, &ss))
		ex.set<Ex::System::Service>("Cannot query service ",_name," status");
	return ss.dwCurrentState == SERVICE_RUNNING;
}

	
bool WinService::start(Exception& ex) {
	if (!open(ex))
		return false;
	if (!StartService(_svcHandle, 0, NULL)) {
		ex.set<Ex::System::Service>("Cannot start service ", _name);
		return false;
	}

	SERVICE_STATUS svcStatus;
	int msecs = 0;
	while (msecs < STARTUP_TIMEOUT) {
		if (!QueryServiceStatus(_svcHandle, &svcStatus))
			break;
		if (svcStatus.dwCurrentState != SERVICE_START_PENDING)
			break;
		Thread::Sleep(250);
		msecs += 250;
	}
	if (!QueryServiceStatus(_svcHandle, &svcStatus))
		ex.set<Ex::System::Service>("Cannot query status of starting service ", _name);
	else if (svcStatus.dwCurrentState != SERVICE_RUNNING)
		ex.set<Ex::System::Service>("Service ", _name, " failed to start within a reasonable time");
	else
		return true;
	return false;
 }


bool WinService::stop(Exception& ex) {
	if (!open(ex))
		return false;
	SERVICE_STATUS svcStatus;
	if (!ControlService(_svcHandle, SERVICE_CONTROL_STOP, &svcStatus)) {
		ex.set<Ex::System::Service>("Cannot stop service ", _name);
		return false;
	}
	return true;
}


bool WinService::setStartup(Exception& ex, Startup startup) {
	if (!open(ex))
		return false;
	DWORD startType;
	switch (startup) {
		case STARTUP_MANUAL:
			startType = SERVICE_DEMAND_START;
			break;
		case STARTUP_AUTO:
			startType = SERVICE_AUTO_START;
			break;
		default:
			startType = SERVICE_DISABLED;
	}
	if (ChangeServiceConfig(_svcHandle, SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
		return true;
	ex.set<Ex::System::Service>("Cannot change service ", _name, " startup mode");
	return false;
}

	
WinService::Startup WinService::getStartup(Exception& ex) const {
	LPQUERY_SERVICE_CONFIGA pSvcConfig = config(ex);
	if (!pSvcConfig)
		return STARTUP_DISABLED;
	Startup result;
	switch (pSvcConfig->dwStartType) {
		case SERVICE_AUTO_START:
		case SERVICE_BOOT_START:
		case SERVICE_SYSTEM_START:
			result = STARTUP_AUTO;
			break;
		case SERVICE_DISABLED:
			result = STARTUP_DISABLED;
			break;
		default:
			ex.set<Ex::System::Service>("Unknown startup mode ", pSvcConfig->dwStartType, " for the ", _name, " service");
		case SERVICE_DEMAND_START:
			result = STARTUP_MANUAL;
			break;
	}
	LocalFree(pSvcConfig);
	return result;
}

bool WinService::setDescription(Exception& ex, const string& description) {
	string key(REGISTRY_KEY);
	key += _name;
	WinRegistryKey regKey(HKEY_LOCAL_MACHINE, key);
	return regKey.setString(ex,REGISTRY_DESCRIPTION, description);
}


const string& WinService::getDescription(Exception& ex,string& description) const {
	string key(REGISTRY_KEY);
	key += _name;
	WinRegistryKey regKey(HKEY_LOCAL_MACHINE, key, true);
	return regKey.getString(ex, REGISTRY_DESCRIPTION, description);
}


LPQUERY_SERVICE_CONFIGA WinService::config(Exception& ex) const {
	if (!open(ex))
		return NULL;
	int size = 4096;
	DWORD bytesNeeded;
	LPQUERY_SERVICE_CONFIGA pSvcConfig = (LPQUERY_SERVICE_CONFIGA)LocalAlloc(LPTR, size);
	if (!pSvcConfig) {
		ex.set<Ex::System::Service>("Cannot allocate service config buffer");
		return NULL;
	}
	while (!QueryServiceConfigA(_svcHandle, pSvcConfig, size, &bytesNeeded)) {
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			LocalFree(pSvcConfig);
			size = bytesNeeded;
			pSvcConfig = (LPQUERY_SERVICE_CONFIGA)LocalAlloc(LPTR, size);
		} else {
			LocalFree(pSvcConfig);
			ex.set<Ex::System::Service>("Cannot query service ", _name, " configuration");
			return NULL;
		}
	}
	return pSvcConfig;
}


} // namespace Mona


#endif // !defined(_WIN32_WCE)