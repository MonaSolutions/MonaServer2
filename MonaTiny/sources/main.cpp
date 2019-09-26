
#include "Mona/Server.h"
#include "Mona/ServerApplication.h"
#include "MonaTiny.h"
#include "Version.h"

#define VERSION		"1." STRINGIFY(MONA_VERSION)

using namespace std;
using namespace Mona;

struct ServerApp : ServerApplication  {

	ServerApp() { setString("description", "MonaTiny server"); }

	const char* defineVersion() { return VERSION; }

///// MAIN
	int main(TerminateSignal& terminateSignal) {

		// starts the server
		MonaTiny server(terminateSignal, getNumber<UInt16>("cores"));

		server.start(*this);

	//	Sleep(20000);

	//	string data("onTextData&text=Doremi&lang=eng&trackid=1");
	//	server.publish(KBBridge::DATA, 0, data.data(), data.size());

		terminateSignal.wait();
		// Stop the server
		server.stop();

		return Application::EXIT_OK;
	}

};

int main(int argc, const char* argv[]) {
	return ServerApp().run(argc, argv);
}
