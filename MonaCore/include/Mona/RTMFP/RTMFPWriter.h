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

#include "Mona/Mona.h"
#include "Mona/RTMFP/RTMFPSender.h"
#include "Mona/FlashWriter.h"
#include "Mona/Logs.h"


namespace Mona {

struct RTMFPWriter : FlashWriter, virtual Object {
	RTMFPWriter(UInt64 id, UInt64 flowId, const Binary& signature, RTMFP::Output& output);

	Writer&		newWriter() { return **_writers.emplace(_output.newWriter(_pQueue->flowId, Packet(_pQueue->signature.data(), _pQueue->signature.size()))).first; }

	UInt64		queueing() const { return _output.queueing(); }
	void		acquit(UInt64 stageAck, UInt32 lostCount);
	bool		consumed() { return _writers.empty() && closed() && !_pSender && _pQueue.unique() && _pQueue->empty(); }

	template <typename ...Args>
	void fail(Args&&... args) {
		if (closed())
			return;
		WARN("Writer ", _pQueue->id, " has failed, ", std::forward<Args>(args)...);
		fail();
	}

	void				clear() { _pSender.reset(); }
	void				closing(Int32 code, const char* reason = NULL);

	void				sendMember(const UInt8* id);

private:
	void				flushing();
	void				fail();

	void				repeatMessages(UInt32 lostCount =0); // fragments=0 means all possible!
	AMFWriter&			newMessage(bool reliable, Media::Data::Type type = Media::Data::TYPE_AMF, const Packet& packet = Packet::Null());
	AMFWriter&			write(AMF::Type type, UInt32 time, Media::Data::Type packetType, const Packet& packet, bool reliable);
	

	RTMFP::Output&						_output;
	shared<RTMFPSender>					_pSender;
	shared<RTMFPSender::Queue>			_pQueue;
	UInt64								_stageAck;
	UInt32								_lostCount;
	UInt32								_repeatDelay;
	Time								_repeatTime;
	std::set<shared<RTMFPWriter>>		_writers;
};


} // namespace Mona
