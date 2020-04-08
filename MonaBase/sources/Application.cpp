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

#include "Mona/Net.h"
#include "Mona/Application.h"
#include "Mona/HelpFormatter.h"
#include "Mona/Date.h"
#if !defined(_WIN32)
#include <signal.h>
#define SetCurrentDirectory !chdir
#endif
#include "Mona/FileLogger.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

Application::Application() : _version(NULL) {
#if defined(_DEBUG)
#if defined(_WIN32)
	DetectMemoryLeak();
#else
	struct sigaction sa;
	sa.sa_handler = HandleSignal;
	sa.sa_flags   = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGILL, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGSYS, &sa, 0);
#endif
#endif
}

#if !defined(_WIN32)
void Application::HandleSignal(int sig) {
	switch (sig) {
		case SIGILL:
            FATAL_ERROR("Illegal instruction");
		case SIGBUS:
            FATAL_ERROR("Bus error");
		case SIGSEGV:
            FATAL_ERROR("Segmentation violation");
		case SIGSYS:
            FATAL_ERROR("Invalid system call");
		default:
            FATAL_ERROR("Error code ",sig)
	}
}
#endif

bool Application::init(int argc, const char* argv[]) {
	// 1 - build _file, _name and configPath
	FileSystem::GetBaseName(argv[0], _name); // in service mode first argument is the service name
	_file.set(FileSystem::GetCurrentApp(argv[0])); // use GetCurrentApp because for service the first argv argument can be the service name and not the executable!
	Path configPath;
	bool iniParam = false;
	for (int i = 1; i < argc; ++i) {
		// search File.ini in arguments!
		if (!configPath) {
			_version = Option::Parse(argv[i]);
			if (_version || !configPath.set(argv[i]))
				continue;
			iniParam = !configPath.isFolder();
		}
		argv[i-1] = argv[i];
	}
	if (configPath) {
		--argc;
		// Set the current directory to the configuration file => forced to work with "dir/file.ini" argument (and win32 double click on ini file)
		if(configPath.isFolder())
			configPath.set(configPath, _name, ".ini");
		else
			_name = configPath.baseName(); // not make configuration "name" in ini file otherwise in service mode impossible to refind the correct ini file to load: service name must stay the base name of ini file!
		if (!SetCurrentDirectory(configPath.parent().c_str()))
			FATAL_ERROR("Cannot set current directory of ", name()); // useless to continue, the application could not report directory error (no logs, no init, etc...)
	}
	configPath.set(_name, ".ini");

	// 2 - load configurations + write common parameters
	if (loadConfigurations(configPath)) {
		setString("application.configPath", configPath);
		setString("application.configDir", configPath.parent());
	} else {
		if (iniParam)
			FATAL_ERROR("Impossible to load configuration file ", name(), ".ini");
		configPath.reset();
	}
	setString("name", _name);
	setString("application.command", _file);
	setString("application.path", _file);
	setString("application.name", _file.name());
	setString("application.baseName", _file.baseName());
	setString("application.dir", _file.parent());
	if ((_version = defineVersion()))
		setString("application.version", _version);
	// not assign "logs.level" because is not mandatory beause can be assigned everywhere by Logs::SetLevel!

	// 3 - init logs
	String logDir(_name, ".log/");
	UInt16 rotation(FileLogger::DEFAULT_ROTATION);
	UInt32 sizeByFile(FileLogger::DEFAULT_SIZE_BY_FILE);
	if (initLogs(logDir, sizeByFile, rotation)) {
		Exception ex;
		if (!FileSystem::CreateDirectory(ex, logDir))
			FATAL_ERROR(name(), " already running, ", ex);
		if(ex)
			WARN(ex);
		setString("logs.directory", logDir);
		setNumber("logs.rotation", rotation);
		setNumber("logs.maxSize", sizeByFile);
		// Set Logger after opening _logStream!
		if (!Logs::AddLogger<FileLogger>(String("file!", name(), " already running?"), move(logDir), sizeByFile, rotation))
			FATAL_ERROR(name(), " initLogs can't override file logger");
	}

	// 4 - first logs
	if (_version)
		INFO(name(), " v", _version);
	if(configPath)
		INFO("Load configuration file ", name(), ".ini");

	// 5 - define options: after configurations to override configurations (for logs.level for example) and to allow log in defineOptions
	Exception ex;
	defineOptions(ex, _options);
	if (ex)
		FATAL_ERROR(ex);
	if (!_options.process(ex, argc, argv, [this](const string& name, const char* value) { setString("arguments." + name, value ? value : String::Empty().c_str()); }))
		FATAL_ERROR(ex, ", use --help")
	else if(ex)
		WARN(ex, ", use --help")

	// 6 - behavior
	if (hasKey("arguments.help")) {
		displayHelp();
		return false;
	}
	if (hasKey("arguments.version")) {
		// just show version and exit
		if (!_version)
			INFO(name(), " (no version defined)");
		return false;
	}
	return true;
}

void Application::displayHelp() {
	HelpFormatter::Description description(_file.name().c_str(), _options);
	String::Append(description.usage, " [/currentDir/[", _file.baseName(), ".ini]]");
	description.header = getString("description", name().c_str());
	HelpFormatter::Format(std::cout, description);
}

bool Application::initLogs(string& directory, UInt32& sizeByFile, UInt16& rotation) {
	if (!getBoolean<true>("logs"))
		return false;
	getString("logs.directory", directory);
	getNumber("logs.rotation", rotation);
	getNumber("logs.maxSize", sizeByFile);
	return true;
}

void Application::defineOptions(Exception& ex, Options& options) {

	options.add(ex, "log", "l", "Log level argument, must be beetween 0 and 8 : nothing, fatal, critic, error, warn, note, info, debug, trace. Default value is 6 (info), all logs until info level are displayed.")
		.argument("level")
		.handler([](Exception& ex, const char* value) {
			Logs::SetLevel(String::ToNumber<UInt8, LOG_DEFAULT>(value));
			return true;
		});

	options.add(ex, "dump", "d", "Enables packet traces in logs. Optional argument is a string filter to dump just packet which matchs this expression. If no argument is given, all the dumpable packet are gotten.")
		.argument("filter", false)
		.handler([](Exception& ex, const char* value) { Logs::SetDump(value ? value : ""); return true; });
	
	options.add(ex, "dumpLimit", "dl", "If dump is activated this option set the limit of dump messages. Argument is an unsigned integer defining the limit of bytes to show. By default there is not limit.")
		.argument("limit")
		.handler([](Exception& ex, const char* value) { Logs::SetDumpLimit(String::ToNumber<Int32, -1>(ex, value)); return true; });

	options.add(ex,"help", "h", "Displays help information about command-line usage.");
	options.add(ex,"version", "v", String("Displays ", name()," version."));
}

void Application::onParamChange(const string& key, const string* pValue) {
	if (String::ICompare(key, "net.bufferSize") == 0 ||
		String::ICompare(key, "net.recvBufferSize") == 0 ||
		String::ICompare(key, "net.sendBufferSize") == 0 ||
		String::ICompare(key, "bufferSize") == 0 ||
		String::ICompare(key, "recvBufferSize") == 0 ||
		String::ICompare(key, "sendBufferSize") == 0) {

		UInt32 value;
		if (getNumber("net.recvBufferSize", value) ||
			getNumber("net.bufferSize", value) ||
			getNumber("recvBufferSize", value) ||
			getNumber("bufferSize", value)) {
			Net::SetRecvBufferSize(value);
		} else
			Net::ResetRecvBufferSize();

		if (getNumber("net.sendBufferSize", value) ||
			getNumber("net.bufferSize", value) ||
			getNumber("sendBufferSize", value) ||
			getNumber("bufferSize", value)) {
			Net::SetSendBufferSize(value);
		} else
			Net::ResetSendBufferSize();

		DEBUG("Defaut socket buffers set to ", Net::GetRecvBufferSize(), "B in reception and ", Net::GetSendBufferSize(), "B in sends");
	} else if (String::ICompare(key, "logs.level") == 0) {
		UInt8 level = getNumber<UInt8, LOG_DEFAULT>("arguments.log");
		if (pValue)
			String::ToNumber(*pValue, level);
		Logs::SetLevel(level);
	}
		
	Parameters::onParamChange(key, pValue);
}
void Application::onParamClear() {
	Logs::SetLevel(getNumber<UInt8, LOG_DEFAULT>("arguments.log"));
	Net::ResetRecvBufferSize();
	Net::ResetSendBufferSize();
	Parameters::onParamClear();
}

int Application::run(int argc, const char* argv[]) {
#if !defined(_DEBUG)
	try {
#endif
		if (!init(argc, argv))
			return EXIT_OK;
		return main();
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

} // namespace Mona
