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
#include "Mona/Parameters.h"
#include "Mona/Options.h"
#include "Mona/HelpFormatter.h"
#include "Mona/Logger.h"
#include "Mona/File.h"
#include <iostream>

namespace Mona {


struct Application : Parameters, private Logger, virtual Object {
	enum ExitCode {
		EXIT_OK = 0,  /// successful termination
		EXIT_USAGE = 64, /// command line usage error
		EXIT_DATAERR = 65, /// data format error
		EXIT_NOINPUT = 66, /// cannot open input
		EXIT_NOUSER = 67, /// addressee unknown
		EXIT_NOHOST = 68, /// host name unknown
		EXIT_UNAVAILABLE = 69, /// service unavailable
		EXIT_SOFTWARE = 70, /// internal software error
		EXIT_OSERR = 71, /// system error (e.g., can't fork)
		EXIT_OSFILE = 72, /// critical OS file missing
		EXIT_CANTCREAT = 73, /// can't create (user) output file
		EXIT_IOERR = 74, /// input/output error
		EXIT_TEMPFAIL = 75, /// temp failure; user is invited to retry
		EXIT_PROTOCOL = 76, /// remote error in protocol
		EXIT_NOPERM = 77, /// permission denied
		EXIT_CONFIG = 78,  /// configuration error
	};


	const Path&				file() const { return _file; }

	const Options&			options() const { return _options; }

	virtual void			displayHelp() { HelpFormatter::Format(std::cout, _file.c_str(), options()); }
	const char*				version() { return _version; }

    int						run(int argc, const char* argv[]);

	virtual bool			isInteractive() const { return true; }

protected:
	Application();

    bool					init(int argc, const char* argv[]);
	virtual int				main() = 0;

	// Can be override to change load behavior + path location
	virtual bool			loadConfigurations(Path& path);
	virtual bool			loadLogFiles(std::string& directory, std::string& fileName, UInt32& sizeByFile, UInt16& rotation);
	virtual const char*		defineVersion() { return NULL; }
	virtual void			defineOptions(Exception& ex, Options& options);

	virtual void			log(LOG_LEVEL level, const Path& file, long line, const std::string& message);
	virtual void			dump(const std::string& header, const UInt8* data, UInt32 size);
	
private:
#if !defined(_WIN32)
	static void HandleSignal(int sig);
#endif


	void			manageLogFiles();

	std::vector<std::string>    _args;
	Options						_options;
	Path						_file;
	const char*					_version;

	// logs
	UInt32						_logSizeByFile;
	UInt16						_logRotation;
	std::string					_logPath;
	std::unique_ptr<File>		_pLogFile;
};


} // namespace Mona
