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
#include "Mona/Date.h"
#if !defined(_WIN32)
    #include <signal.h>
#endif
#include "Mona/Util.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {


static const char* LogLevels[] = { "FATAL", "CRITIC", "ERROR", "WARN", "NOTE", "INFO", "DEBUG", "TRACE" };

Application::Application() : _logSizeByFile(1000000), _logRotation(10), _version(NULL) {
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
	// 1 - build _file!
	string path(argv[0]);
#if defined(_WIN32)
	FileSystem::GetCurrentApp(path);
#else
	if (!FileSystem::GetCurrentApp(path)) {
		if (path.find('/') != string::npos) {
			if (!FileSystem::IsAbsolute(path))
				path.insert(0, FileSystem::GetCurrentDir());
		} else {
			// just file name!
			if (!FileSystem::Find(path))
				path.insert(0, FileSystem::GetCurrentDir());
		}
	}
#endif
	_file.set(path);

	// 2 - load configurations
	Path configPath(_file.parent(), _file.baseName(), ".ini");
	if (loadConfigurations(configPath)) {
		// Has configPath!
		setString("application.configPath", configPath);
		setString("application.configDir", configPath.parent());
	}

	// 3 - Write common parameters
	setString("application.command", _file);
	setString("application.path", _file);
	setString("application.name", _file.name());
	setString("application.baseName", _file.baseName());
	setString("application.dir", _file.parent());
	UInt32 value;
	if (getNumber("net.bufferSize", value) || getNumber("bufferSize", value)) {
		Net::SetRecvBufferSize(value);
		Net::SetSendBufferSize(value);
	}
	if (getNumber("net.recvBufferSize", value) || getNumber("recvBufferSize", value))
		Net::SetRecvBufferSize(value);
	if (getNumber("net.sendBufferSize", value) || getNumber("sendBufferSize", value))
		Net::SetSendBufferSize(value);
	setNumber("net.recvBufferSize", Net::GetRecvBufferSize());
	setNumber("net.sendBufferSize", Net::GetSendBufferSize());

	// 4 - init logs
	string logDir(_file.parent());
	logDir.append("logs");
	string logFileName("log");
	Exception ex;
	if (loadLogFiles(logDir, logFileName, _logSizeByFile, _logRotation)) {
		bool success;
		AUTO_CRITIC(success=FileSystem::CreateDirectory(ex, logDir),"Log system")
		if (success) {
			_logPath.assign(FileSystem::MakeFolder(logDir)).append(logFileName);
			_pLogFile.reset(new File(_logRotation > 0 ? ((_logPath += '.') + '0') : _logPath, File::MODE_APPEND));
		}
	}
	Logs::SetLogger(*this); // Set Logger after opening _logStream!

	// 5 - init version
	if ((_version = defineVersion())) {
		setString("application.version", _version);
		INFO(file().baseName().c_str(), " v", _version);
	}

	// 6 - first logs
	DEBUG(hasKey("application.configPath") ? "Load configuration file " : "Impossible to load configuration file ", configPath)

	// 7 - define options
	defineOptions(ex, _options);
	if (ex)
        FATAL_ERROR(ex);
	if (!_options.process(ex, argc, argv, [this](const string& name, const string& value) { setString("arguments." + name, value); }))
        FATAL_ERROR("Arguments, ",ex," use 'help'")
	else if (ex)
		WARN("Arguments, ",ex," use 'help'")

	// 8 - behavior
	if (hasKey("arguments.help")) {
		displayHelp();
		return false;
	}

	// if "-v" just show version and exit
	if (hasKey("arguments.version")) {
		if (!_version)
			INFO(file().baseName().c_str(), " (no version defined)");
		return false;
	}

	return true;
}

bool Application::loadConfigurations(Path& path) {
	return Util::ReadIniFile(path, *this);
}

bool Application::loadLogFiles(string& directory, string& fileName, UInt32& sizeByFile, UInt16& rotation) {
	getString("logs.directory", directory);
	getString("logs.name", fileName);
	getNumber("logs.rotation", rotation);
	getNumber("logs.maxSize", sizeByFile);
	return true;
}

void Application::defineOptions(Exception& ex, Options& options) {

	options.add(ex, "log", "l", "Log level argument, must be beetween 0 and 8 : nothing, fatal, critic, error, warn, note, info, debug, trace. Default value is 6 (info), all logs until info level are displayed.")
		.argument("level")
		.handler([this](Exception& ex, const string& value) {
#if defined(_DEBUG)
		Logs::SetLevel(String::ToNumber<UInt8, LOG_DEBUG>(ex, value));
#else
		Logs::SetLevel(String::ToNumber<UInt8, LOG_INFO>(ex, value));
#endif
		return true;
	});

	options.add(ex, "dump", "d", "Enables packet traces in logs. Optional argument is a string filter to dump just packet which matchs this expression. If no argument is given, all the dumpable packet are gotten.")
		.argument("filter", false)
		.handler([this](Exception& ex, const string& value) { Logs::SetDump(value.c_str()); return true; });
	
	options.add(ex, "dumpLimit", "dl", "If dump is activated this option set the limit of dump messages. Argument is an unsigned integer defining the limit of bytes to show. By default there is not limit.")
		.argument("limit", true)
		.handler([this](Exception& ex, const string& value) { Logs::SetDumpLimit(String::ToNumber<Int32, -1>(ex, value)); return true; });

	options.add(ex,"help", "h", "Displays help information about command-line usage.");
	options.add(ex,"version", "v", "Displays application version.");
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

void Application::log(LOG_LEVEL level, const Path& file, long line, const string& message) {
	if (isInteractive())
		Logger::log(level, file, line, message);
	if (!_pLogFile)
		return;
	static string Buffer; // max size controlled by Logs system!
	static string temp;
	static Exception ex;

	String::Assign(Buffer, String::Date("%d/%m %H:%M:%S.%c  "), LogLevels[level - 1]);
	Buffer.append(25-Buffer.size(),' ');
	
	String::Append(Buffer, Thread::CurrentId(), ' ');
	if (String::ICompare(FileSystem::GetName(file.parent(), temp), "sources") != 0 && String::ICompare(temp, "mona") != 0)
		String::Append(Buffer, temp,'/');
	String::Append(Buffer, file.name(), '[' , line , "] ");
	if (Buffer.size() < 60)
		Buffer.append(60 - Buffer.size(), ' ');
	String::Append(Buffer, message, '\n');

	if (!_pLogFile->write(ex, Buffer.data(), Buffer.size())) {
		Logger::log(LOG_CRITIC, __FILE__, __LINE__, ex);
		return _pLogFile.reset();
	}
	manageLogFiles();
}

void Application::dump(const string& header, const UInt8* data, UInt32 size) {
	if (isInteractive())
		Logger::dump(header, data, size);
	if (!_pLogFile)
		return;
	String buffer(String::Date("%d/%m %H:%M:%S.%c  "), header, '\n');
	Exception ex;
	if (!_pLogFile->write(ex, buffer.data(), buffer.size()) || !_pLogFile->write(ex, data, size))
		return log(LOG_ERROR, __FILE__, __LINE__, ex);
	manageLogFiles();
}

void Application::manageLogFiles() {
	if (_logSizeByFile == 0 || _pLogFile->size(true) <= _logSizeByFile)
		return; // _logSizeByFile==0 => inifinite log file! (user choice..)

	_pLogFile.reset(new File(_logRotation>0 ? (_logPath + '0') : _logPath, File::MODE_APPEND)); // close old handle!
	int num = _logRotation;
	string path(_logPath);
	if (num > 0)
		String::Append(path,num);
	Exception ex;
	FileSystem::Delete(ex, path);
	// rotate
	string newPath;
	while(--num>=0)
		FileSystem::Rename(String::Assign(path, _logPath, num), String::Assign(newPath, _logPath, num + 1));
}

} // namespace Mona
