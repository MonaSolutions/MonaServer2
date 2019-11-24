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

#include "Mona/Publish.h"

using namespace std;


namespace Mona {


bool Publish::Action::run(Exception& ex) {
	// if "Publish" is always alive + not failed
	if (_ppSource.unique() || !*_ppSource)
		return false;
	if(_isPublication)
		run((Publication&)**_ppSource);
	else
		run(**_ppSource);
	return true;
}

Publish::Publish(ServerAPI& api, const char* name) : Server::Action(api, "Publish"), _pName(SET, name), _api(api), _ppSource(SET, &Source::Null()) {
}
Publish::Publish(ServerAPI& api, Source& source) : Server::Action(api, "Publish"), _api(api), _ppSource(SET, &source) {
}
 
Publish::~Publish() {
	if (!_pName || !*_ppSource)
		return; // _ppSource is not a publication OR publication has failed
	struct Unpublishing : Action, virtual Object {
		Unpublishing(const shared<Source*>& ppSource, ServerAPI& api) : _api(api), Action("Unpublish", ppSource, true) {}
		void run(Publication& publication) { _api.unpublish(publication); }
		ServerAPI& _api;
	};
	queue<Unpublishing>(_api);
}

void Publish::reportLost(Media::Type type, UInt32 lost, UInt8 track) {
	struct Lost : Action, private Media::Base, virtual Object {
		Lost(const shared<Source*>& ppSource, Media::Type type, UInt32 lost, UInt8 track = 0) : Action("Publish::Lost", ppSource), Media::Base(type, Packet::Null(), track), _lost(lost) {}
		void run(Source& source) { source.reportLost(type, _lost, track); }
		UInt32			_lost;
	};
	queue<Lost>(type, lost, track);
}

void Publish::flush(UInt16 ping) {
	struct Flush : Action, virtual Object {
		Flush(const shared<Source*>& ppSource, UInt16 ping, bool isPublication) : Action("Publish::Flush", ppSource, isPublication), _ping(ping) {}
		void run(Publication& publication) { publication.flush(_ping); }
		void run(Source& source) { source.flush(); }
		UInt16			_ping;
	};
	queue<Flush>(ping, _pName ? true : false);
}

void Publish::reset() {
	struct Reset : Action, virtual Object {
		Reset(const shared<Source*>& ppSource) : Action("Publish::Reset", ppSource) {}
	private:
		void run(Source& source) { source.reset(); }
	};
	queue<Reset>();
}


} // namespace Mona
