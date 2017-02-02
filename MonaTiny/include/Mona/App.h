
#pragma once

#include "Mona/Mona.h"
#include "Mona/Publication.h"
#include "Mona/Group.h"

namespace Mona {

class App : virtual public Object {
public:

	class Client : virtual public Object {
	public:
		Client(Mona::Client& client) : client(client) {}
		virtual ~Client() {}

		virtual void onAddressChanged(const SocketAddress& oldAddress) {}
		virtual bool onInvocation(Exception& ex, const std::string& name, DataReader& arguments, UInt8 responseType) { return false; }
		virtual bool onFileAccess(Exception& ex, Mona::File::Mode mode, Path& file, DataReader& arguments, DataWriter& properties) { return true; }

		virtual bool onPublish(Exception& ex, const Publication& publication) { return true; }
		virtual void onUnpublish(const Publication& publication) {}

		virtual bool onSubscribe(Exception& ex, const Subscription& subscription, const Publication& publication) { return true; }
		virtual void onUnsubscribe(const Subscription& subscription, const Publication& publication) {}

		virtual void onJoinGroup(Group& group) {}
		virtual void onUnjoinGroup(Group& group) {}

		Mona::Client& client;
	};

	App(const Parameters& configs) {}

	virtual App::Client* newClient(Exception& ex, Mona::Client& client, DataReader& parameters, DataWriter& response) {
		return NULL;
	}

	virtual void manage() {}
};

} // namespace Mona
