/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "MonaServer.h"
#include "Mona/ServerApplication.h"
#include "Version.h"

#define VERSION_MAJOR		"1.1"
#define VERSION_MINOR		MONA_VERSION-367

using namespace std;
using namespace Mona;


class ServerMona : public ServerApplication  {
private:

	const char* defineVersion() {
		return String::Assign(_version, VERSION_MAJOR".", VERSION_MINOR).c_str();
	}

	void defineOptions(Exception& ex, Options& options) {
		options.acceptUnknownOption = true;
		ServerApplication::defineOptions(ex, options);
	}

///// MAIN
	int main(TerminateSignal& terminateSignal) {

		// starts the server
		MonaServer server(*this, terminateSignal);
		if (server.start(*this)) {
			terminateSignal.wait();
			// Stop the server
			server.stop();
		}
		return Application::EXIT_OK;
	}

	std::string _version;
};

int main(int argc, const char* argv[]) {
	return ServerMona().run(argc, argv);
}
