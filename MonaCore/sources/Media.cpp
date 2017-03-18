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

#include "Mona/Media.h"
#include "Mona/JSONReader.h"
#include "Mona/JSONWriter.h"
#include "Mona/XMLRPCReader.h"
#include "Mona/XMLRPCWriter.h"
#include "Mona/AMFReader.h"
#include "Mona/AMFWriter.h"
#include "Mona/QueryReader.h"
#include "Mona/QueryWriter.h"
#include "Mona/MapReader.h"

#include "Mona/ADTSReader.h"
#include "Mona/ADTSWriter.h"

#include "Mona/MediaSocket.h"
#include "Mona/MediaFile.h"


using namespace std;


namespace Mona {

Media::Data::Data(UInt16 track, DataReader& properties) : Base(TYPE_NONE, track), tag(ToType(properties)) {
	if (!tag) {
		(Media::Data::Type&)tag = TYPE_AMF;
		shared<Buffer> pBuffer(new Buffer());
		AMFWriter writer(*pBuffer);
		properties.read(writer);
		set(pBuffer);
	} else
		set(Packet(*properties));
}

BinaryWriter& Media::Video::Tag::pack(BinaryWriter& writer, bool withTime) const {
	if(withTime)
		writer.write32(time);
	// codec 5 bits (0-31)
	// frame 3 bits (0-7)
	// compositionOffset 32 bits (optional)
	writer.write8((codec << 3) | (frame & 7));
	if(compositionOffset)
		writer.write32(compositionOffset);
	return writer;
}

BinaryReader& Media::Video::Tag::unpack(BinaryReader& reader, bool withTime) {
	if(withTime)
		time = reader.read32();
	// isConfig 1 bit
	// codec 4 bits (0-15)
	// frame 3 bits (0-7)
	// compositionOffset 32 bits (optional)
	UInt8 value = reader.read8();
	codec = Codec(value >> 3);
	frame = Frame(value & 7);
	compositionOffset = reader.read32(); // if absent return 0!
	return reader;
}

BinaryWriter& Media::Audio::Tag::pack(BinaryWriter& writer, bool withTime) const {
	if(withTime)
		writer.write32(time);
	// isConfig 1 bit
	// codec 5 bits (0-31)
	// channels 6 bits (0-62)
	// rate 4 bits (0-15)
	writer.write8((isConfig ? 0x80 : 0) | ((codec&0x1F)<<2) | ((channels&0x30)>>4));
	return writer.write8((channels&0x0F)<<4 | (ADTSWriter::RateToIndex(rate)&0x0F));
}

BinaryReader& Media::Audio::Tag::unpack(BinaryReader& reader, bool withTime) {
	if(withTime)
		time = reader.read32();
	// isConfig 1 bit
	// codec 5 bits (0-31)
	// channels 6 bits (0-62)
	// rate 4 bits (0-15)
	UInt8 value = reader.read8();
	isConfig = value & 0x80 ? true : false;
	codec = Codec((value & 0x7C)>>2);
	channels = (value & 3) << 4;
	value = reader.read8();
	channels |= (value & 0xF0)>>4;
	rate = ADTSReader::RateFromIndex(value & 0x0F);
	return reader;
}

const Packet& Media::Properties::operator[](Media::Data::Type type) const {
	if (!type) {
		ERROR("Media properties requested with an unknown data type");
		return Packet::Null();
	}
	// not considerate the empty() case, because empty properties must write a object empty to match onMetaData(obj) with argument on clear properties!
	_packets.resize(type);
	Packet& packet(_packets[type - 1]);
	if (packet)
		return packet;
	// Serialize in the format requested!
	shared<Buffer> pBuffer(new Buffer());
	unique_ptr<DataWriter> pWriter(Media::Data::NewWriter(type, *pBuffer));
	MapReader<Parameters> reader(*this);
	reader.read(*pWriter);
	return packet.set(pBuffer);
}

Media::Data::Type Media::Data::ToType(const type_info& info) {
	static const map<size_t, Media::Data::Type> Types({
		{ typeid(AMFWriter).hash_code(),TYPE_AMF },
		{ typeid(AMFReader).hash_code(),TYPE_AMF },
		{ typeid(JSONWriter).hash_code(),TYPE_JSON },
		{ typeid(JSONReader).hash_code(),TYPE_JSON },
		{ typeid(QueryReader).hash_code(),TYPE_QUERY },
		{ typeid(QueryWriter).hash_code(),TYPE_QUERY },
		{ typeid(XMLRPCReader).hash_code(),TYPE_XMLRPC},
		{ typeid(XMLRPCWriter).hash_code(),TYPE_XMLRPC },
	});
	const auto& it = Types.find(info.hash_code());
	return it == Types.end() ? TYPE_UNKNOWN : it->second;
}
Media::Data::Type Media::Data::ToType(const char* subMime) {
	if (String::ICompare(subMime, EXPAND("json")) == 0)
		return TYPE_JSON;
	if (String::ICompare(subMime, EXPAND("xml")) == 0)
		return TYPE_XMLRPC;
	if (String::ICompare(subMime, EXPAND("amf")) == 0 || String::ICompare(subMime, EXPAND("x-amf")) == 0)
		return TYPE_AMF;
	if (String::ICompare(subMime, EXPAND("x-www-form-urlencoded")) == 0)
		return TYPE_QUERY;
	return TYPE_UNKNOWN;
}

DataReader* Media::Data::NewReader(Type type, const Packet& packet) {
	switch (type) {
		case TYPE_JSON: {
			JSONReader* pReader = new JSONReader(packet.data(), packet.size());
			if (pReader->isValid())
				return pReader;
			delete pReader;
			break;
		}
		case TYPE_XMLRPC: {
			XMLRPCReader* pReader = new XMLRPCReader(packet.data(), packet.size());
			if (pReader->isValid())
				return pReader;
			delete pReader;
			break;
		}
		case TYPE_AMF:
		case TYPE_AMF0:
			return new AMFReader(packet.data(), packet.size());
		case TYPE_QUERY:
			return new QueryReader(packet.data(), packet.size());
		default: break;
	}
	return NULL;
}

DataWriter* Media::Data::NewWriter(Type type, Buffer& buffer) {
	switch (type) {
		case TYPE_JSON:
			return new JSONWriter(buffer);
		case TYPE_XMLRPC:
			return new XMLRPCWriter(buffer);
		case TYPE_AMF:
			return new AMFWriter(buffer);
		case TYPE_AMF0:
			return new AMFWriter(buffer, true);
		case TYPE_QUERY:
			return new QueryWriter(buffer);
		case TYPE_UNKNOWN:
			break;
	}
	return NULL;
}

void Media::Source::writeMedia(const Media::Base& media) {
	switch (media.type) {
		case Media::TYPE_AUDIO:
			return writeAudio(media.track, ((const Media::Audio&)media).tag, media);
		case Media::TYPE_VIDEO:
			return writeVideo(media.track, ((const Media::Video&)media).tag, media);
		case Media::TYPE_DATA:
			return writeData(media.track, ((const Media::Data&)media).tag, media);
		default: {
			const Media::Data& data = (const Media::Data&)media;
			unique_ptr<DataReader> pReader(Media::Data::NewReader(data.tag, data));
			return writeProperties(media.track, *pReader);
		}
	}
}

Media::Source& Media::Source::Null() {
	static struct Null : Media::Source {
		void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) {}
		void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) {}
		void writeData(UInt16 track, Media::Data::Type type, const Packet& packet) {}
		void writeProperties(UInt16 track, DataReader& reader) {}
		void reportLost(Media::Type type, UInt32 lost) {}
		void reportLost(Media::Type type, UInt16 track, UInt32 lost) {}
		void flush() {}
		void reset() {}
	} Null;
	return Null;
}

bool Media::Target::beginMedia(const std::string& name, const Parameters& parameters) {
	ERROR(typeof(*this), " doesn't support media streaming");
	return false;
}
bool Media::Target::writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support audio streaming");
	return true;
}
bool Media::Target::writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support video streaming");
	return true;
}
bool Media::Target::writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support data streaming");
	return true;
}

void Media::Stream::start(Source& source) {
	WARN(typeof(*this), " is a target stream, call start(Exception& ex) rather");
	return start();
}

void Media::Stream::stop(const Exception& ex) {
	stop(); 
	onError(ex);
}

UInt32	Media::Stream::RecvBufferSize(0);
UInt32	Media::Stream::SendBufferSize(0);

Media::Stream* Media::Stream::New(Exception& ex, const char* description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS) {
	// Net => [@][address] [type/TLS][/MediaFormat] [parameter MediaFormat]
	// File = > @file[.format][MediaFormat][parameter]
	
	SocketAddress address;
	Type type(TYPE_FILE);
	bool isSecure(false), isTarget(false), isPort(false), isAddress(false);
	string format;
	Path   path;
	String::ForEach forEach([&](UInt32 index, const char* value) {
		if (!index) {
			if ((isTarget = (*value == '@')))
				++value;
			// Address[/path]
	
			size_t size;
			const char* slash(strpbrk(description, "/\\"));
			if (slash) {
				type = TYPE_HTTP;
				size = slash - value;
			} else
				size = strlen(value);

			UInt16 port;
			if (String::ToNumber(value, size, port)) {
				isPort = true;
				address.setPort(port);
				if(slash)
					path.set(slash);
				return true;
			}
			{
				String::Scoped scoped(value + size);
				isAddress = address.set(ex, value);
			}
			ex = nullptr;
			if(!isAddress)
				path.set(value); // File!
			else if(slash)
				path.set(slash);
			return isAddress;
		}
		
		String::ForEach forEach([&](UInt32 index, const char* value) {
			if (String::ICompare(value, "UDP") == 0) {
				type = TYPE_UDP;
				return true;
			}
			if (String::ICompare(value, "TCP") == 0) {
				if(type != TYPE_HTTP)
					type = TYPE_TCP;
				return true;
			}
			if (String::ICompare(value, "HTTP") == 0) {
				type = TYPE_HTTP;
				return true;
			}
			if (String::ICompare(value, "TLS") == 0) {
				isSecure = true;
				return true;
			}
			format = value;
			return false;
		});
		String::Split(value, "/", forEach);
		return false;
	});
	String::Split(description, " \t", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);


	if (isPort) {
		if (type!=TYPE_UDP || isTarget) // if no host and TCP or target
			address.host().set(IPAddress::Loopback());
	} else if (isAddress) {
		if (!address.host() && (type != TYPE_UDP || isTarget)) {
			ex.set<Ex::Net::Address::Ip>("A TCP or target Stream can't have a wildcard ip");
			return NULL;
		}
	} else {
		// File!
		if (!path) {
			ex.set<Ex::Format>("No file name in stream file description");
			return NULL;
		}
		if (path.isFolder()) {
			ex.set<Ex::Format>("Stream file ", path," can't be a folder");
			return NULL;
		}
		if (isTarget) {
			if (MediaWriter* pWriter = MediaWriter::New(format.empty() ? path.extension().c_str() : format.c_str()))
				return new MediaFile::Writer(path, pWriter, ioFile);
		} else if (MediaReader* pReader = MediaReader::New(format.empty() ? path.extension().c_str() : format.c_str()))
			return new MediaFile::Reader(path, pReader, timer, ioFile);
		ex.set<Ex::Unsupported>("Stream file format ", format, " not supported");
		return NULL;
	}

	if (format.empty()) {
		switch (type) {
			case TYPE_UDP:
				// UDP and No Format => TS by default to catch with VLC => UDP = TS
				format = "mp2t";
				break;
			case TYPE_HTTP: {
				const char* subMime;
				if (MIME::Read(path, subMime)) {
					format = subMime;
					break;
				}
			}
			default:
				ex.set<Ex::Format>("Stream description have to indicate a media format");
				return NULL;
		}
	} else if (!type)
		type = String::ICompare(format, EXPAND("RTP")) == 0 ? TYPE_UDP : TYPE_TCP;
	
	if (isTarget) {
		if (MediaWriter* pWriter = MediaWriter::New(format.c_str()))
			return new MediaSocket::Writer(type, path, pWriter, address, ioSocket, isSecure ? pTLS : nullptr);
	} else if (MediaReader* pReader = MediaReader::New(format.c_str()))
		return new MediaSocket::Reader(type, path, pReader, address, ioSocket, isSecure ? pTLS : nullptr);
	ex.set<Ex::Unsupported>("Stream ", TypeToString(type), " format ",format, " not supported");
	return NULL;
}

} // namespace Mona
