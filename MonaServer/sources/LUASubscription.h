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

#pragma once

#include "Script.h"
#include "Mona/Subscription.h"

namespace Mona {

struct LUASubscription : Subscription, Media::Target {
	LUASubscription(lua_State *pState) : Subscription((Media::Target&)self), _pState(pState) {}
	Subscription& operator()() { return self; }
private:
	UInt64	queueing() const;
	bool	beginMedia(const std::string& name);
	bool	writeProperties(const Media::Properties& properties);
	bool	writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
	bool	writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable);
	bool	writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable);
	bool	endMedia();
	void	flush();

	// overrides Media::Source methods of Subscription (remove a clang warning)
	void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { return Subscription::writeAudio(tag, packet, track); }
	void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { return Subscription::writeVideo(tag, packet, track); }
	void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) { return Subscription::writeData(type, packet, track); }

	lua_State* _pState;
};

}
