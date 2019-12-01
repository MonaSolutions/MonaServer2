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

#define VERSION		"2." STRINGIFY(MONA_VERSION)

using namespace std;
using namespace Mona;


struct ServerApp : ServerApplication  {

	const char* defineVersion() { return VERSION; }

private:
///// MAIN
	int main(TerminateSignal& terminateSignal) {

		// starts the server
		MonaServer server(self, terminateSignal);
		Parameters& params = server.start(self);
		// sync params!
		onChange = [&params](const string& key, const string* pValue) {
			if (pValue)
				params.setString(key, *pValue);
			else
				params.erase(key);
		};
		onClear = [&params]() { params.clear(); };

		terminateSignal.wait();

		// Stop the server
		server.stop();

		onChange = nullptr;
		onClear = nullptr;

		return Application::EXIT_OK;
	}
};

int main(int argc, const char* argv[]) {
	return ServerApp().run(argc, argv);
}
