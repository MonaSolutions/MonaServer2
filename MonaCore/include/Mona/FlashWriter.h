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
#include "Mona/Writer.h"
#include "Mona/AMFWriter.h"
#include "Mona/Logs.h"

namespace Mona {

struct FlashWriter : Writer, Media::TrackTarget, virtual Object {
	bool					amf0;

	const bool				isMain;

	AMFWriter&				writeMessage();
	AMFWriter&				writeInvocation(const char* name) { return writeInvocation(name, 0); }
	void					writeRaw(DataReader& arguments, const Packet& packet = Packet::Null());
	BinaryWriter&			writeRaw() { return *write(AMF::TYPE_RAW); }

	AMFWriter&				writeAMFSuccess(const char* code, const std::string& description, bool withoutClosing = false) { return writeAMFState("_result", code, "status", description, withoutClosing); }
	/*!
	Write a AMF Error then the writer should be closed (writeInvocation("close") + close()) */
	AMFWriter&				writeAMFError(const char* code, const std::string& description, bool withoutClosing = false) { _callbackHandle = 1; return writeAMFState("_error", code, "error", description, withoutClosing); }
	void					writeAMFStatus(const char* code, const std::string& description) { writeAMFState("onStatus", code, "status", description); }
	void					writeAMFStatus(const char* code, const char* level, const std::string& description) { writeAMFState("onStatus", code, level, description); }
	AMFWriter&				writeAMFState(const char* name, const char* code, const char* level, const std::string& description, bool withoutClosing = false);
	
	AMFWriter&				writeAMFData(const std::string& name);

	void					writePong(UInt32 pingTime) { writeRaw().write16(0x0007).write32(pingTime); }

	virtual bool			beginMedia(const std::string& name);
	virtual bool			writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
	virtual bool			writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable);
	virtual bool			writeData(Media::Data::Type type, const Packet& packet, bool reliable);
	virtual bool			writeProperties(const Media::Properties& properties);
	virtual bool			endMedia();

	void					flush() { Writer::flush(); }

	void					setCallbackHandle(double value) { _callbackHandle = value; _callbackHandleOnAbort = 0; }
	void					resetCallbackHandle() { _callbackHandle = _callbackHandleOnAbort = 0; }
	virtual void			clear() { _callbackHandle = _callbackHandleOnAbort; } // must erase the queueing messages (don't change the writer state)

protected:
	FlashWriter();

	AMFWriter&				write(AMF::Type type, UInt32 time = 0) { return write(type, time, Media::Data::TYPE_AMF, Packet::Null(), reliable); }
	AMFWriter&				write(AMF::Type type, UInt32 time, bool reliable) { return write(type, time, Media::Data::TYPE_AMF, Packet::Null(), reliable); }
	AMFWriter&				write(AMF::Type type, UInt32 time, Media::Data::Type packetType, const Packet& packet) { return write(type, time, packetType, packet, reliable); }
	virtual AMFWriter&		write(AMF::Type type, UInt32 time, Media::Data::Type packetType, const Packet& packet, bool reliable) = 0;
	virtual void			closing(Int32 error, const char* reason = NULL);
	
	AMFWriter&				writeInvocation(const char* name, double callback);

private:
	double					_callbackHandleOnAbort;
	double					_callbackHandle;

	Packet					_vps;
	Packet					_sps;
	Packet					_pps;
	bool					_firstAV;
	const std::string*		_pPublicationName;
	UInt32					_lastTime;
	UInt32					_time;
};



} // namespace Mona
