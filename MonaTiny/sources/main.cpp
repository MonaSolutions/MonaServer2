#include "Mona/Server.h"
#include "Mona/ServerApplication.h"
#include "Mona/MonaTiny.h"

using namespace std;
using namespace Mona;

struct ServerApp : ServerApplication  {

///// MAIN
	int main(TerminateSignal& terminateSignal) {

/*
		ifstream ts("C:/Users/mathieu/Documents/Marc/SIntel.ts", ios::binary | ios::in);
		UInt8 buffer[188];

		TSReader reader;
		MediaFileWriter<FLVWriter> writer;
		
		int packets = 0;
		while (ts.read(STR buffer, sizeof(buffer))) {
			reader.read(Packet(buffer, sizeof(buffer)), writer);
			++packets;
		}
		ts.close();
		getchar();
		return 0;
		*/

		// starts the server
		MonaTiny server(file().parent()+"www",getNumber<UInt32>("socketBufferSize"), terminateSignal);

		if (server.start(*this)) {

		//	Sleep(20000);

		//	string data("onTextData&text=Doremi&lang=eng&trackid=1");
		//	server.publish(KBBridge::DATA, 0, data.data(), data.size());

			terminateSignal.wait();
			// Stop the server
			server.stop();
		}
		return Application::EXIT_OK;
	}

};

int main(int argc, const char* argv[]) {
	return ServerApp().run(argc, argv);
}
