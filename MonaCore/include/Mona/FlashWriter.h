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

struct FlashWriter : Writer, virtual Object {
	// For AMF response!
	bool					amf0;


	AMFWriter&				writeMessage();
	AMFWriter&				writeInvocation(const char* name) { return writeInvocation(name,0); }
	void					writeRaw(DataReader& reader);
	BinaryWriter&			writeRaw() { return *write(AMF::TYPE_RAW); }

	AMFWriter&				writeAMFSuccess(const char* code, const std::string& description, bool withoutClosing = false) { return writeAMFState("_result", code, false, description, withoutClosing); }
	void					writeAMFStatus(const char* code, const std::string& description) { writeAMFState("onStatus", code, false, description); }
	void					writeAMFStatusError(const char* code, const std::string& description) { writeAMFState("onStatus", code, true, description); }
	void					writeAMFError(const char* code, const std::string& description) { writeAMFState("_error", code, true, description); }
	
	AMFWriter&				writeAMFData(const std::string& name);

	void					writePong(UInt32 pingTime) { writeRaw().write16(0x0007).write32(pingTime); }

	virtual bool			beginMedia(const std::string& name, const Parameters& parameters);
	virtual bool			writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
	virtual bool			writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable);
	virtual bool			writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable);
	virtual bool			writeProperties(const Media::Properties& properties);
	virtual void			endMedia(const std::string& name);

	void					setCallbackHandle(double value) { _callbackHandle = value; _callbackHandleOnAbort = 0; }
	virtual void			clear() { _callbackHandle = _callbackHandleOnAbort; } // must erase the queueing messages (don't change the writer state)

protected:
	FlashWriter();

	AMFWriter&				write(AMF::Type type, UInt32 time = 0) { return write(type, time, Media::Data::TYPE_AMF, Packet::Null(), reliable); }
	AMFWriter&				write(AMF::Type type, UInt32 time, bool reliable) { return write(type, time, Media::Data::TYPE_AMF, Packet::Null(), reliable); }
	AMFWriter&				write(AMF::Type type, UInt32 time, const Packet& packet, bool reliable) { return write(type, time, Media::Data::TYPE_AMF, packet, reliable); }
	virtual AMFWriter&		write(AMF::Type type, UInt32 time, Media::Data::Type packetType, const Packet& packet, bool reliable) = 0;
	virtual void			closing(Int32 error, const char* reason = NULL);
	
	AMFWriter&				writeInvocation(const char* name,double callback);
	AMFWriter&				writeAMFState(const char* name,const char* code,bool isError, const std::string& description,bool withoutClosing=false);

private:
	template<typename TagType>
	void writeTrackedMedia(UInt16 track, const TagType& tag, const Packet& packet, bool reliable) {
		if (amf0) {
			WARN("Impossible to write a byte array in AMF0, switch to AMF3");
			return;
		}
		// flash doesn't support multitrack, so send it by an alternative way
		AMFWriter& writer(write(AMF::TYPE_INVOCATION, 0, packet, reliable));
		writer.amf0 = true;
		writer.writeString(EXPAND("onTrack"));
		writer.writeNumber(0);
		writer->write8(AMF::AMF0_NULL); // for RTMP compatibility! (requiere it)
		writer.amf0 = amf0;
		// write header
		UInt8 buffer[7];
		if (typeid(tag) == typeid(Media::Data::Type))
			writer.writeString(STR buffer, writeTrackedMediaHeader(track, tag, BinaryWriter(buffer, sizeof(buffer))));
		else
			writer.writeBytes(buffer, writeTrackedMediaHeader(track, tag, BinaryWriter(buffer, sizeof(buffer))));
		// write header ByteArray (will be concat with packet on send)
		writer->write8(AMF::AMF0_AMF3_OBJECT); // switch in AMF3 format
		writer->write8(AMF::AMF3_BYTEARRAY); // bytearray in AMF3 format!
		writer->write7BitValue((packet.size() << 1) | 1);
	}
	template<typename TagType>
	UInt8 writeTrackedMediaHeader(UInt16 track, const TagType& tag, BinaryWriter& writer) { return tag.pack(writer, false).write16(track).size(); }
	UInt8 writeTrackedMediaHeader(UInt16 track, Media::Data::Type type, BinaryWriter& writer) { return writer.write8(type).write16(track).size(); }
	

	double					_callbackHandleOnAbort;
	double					_callbackHandle;

	Packet					_sps;
	Packet					_pps;
	bool					_firstAV;
};



} // namespace Mona
