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
#include "Mona/StringReader.h"
#include "Mona/StringWriter.h"
#include "Mona/MapReader.h"
#include "Mona/MapWriter.h"

#include "Mona/MediaSocket.h"
#include "Mona/MediaFile.h"

using namespace std;


namespace Mona {

UInt32 Media::Base::time() const {
	switch (type) {
		case TYPE_AUDIO:
			return ((Media::Audio*)this)->tag.time;
		case TYPE_VIDEO:
			return ((Media::Video*)this)->tag.time;
		default:;
	}
	return 0;
}
void Media::Base::setTime(UInt32 time) {
	switch (type) {
		case TYPE_AUDIO:
			((Media::Audio*)this)->tag.time = time;
			break;
		case TYPE_VIDEO:
			((Media::Video*)this)->tag.time = time;
			break;
		default:;
	}
}
bool Media::Base::isConfig() const {
	switch (type) {
		case TYPE_AUDIO:
			return ((Media::Audio*)this)->tag.isConfig;
		case TYPE_VIDEO:
			return ((Media::Video*)this)->tag.frame == Media::Video::FRAME_CONFIG;
		default:;
	}
	return false;
}


BinaryWriter& Media::Pack(BinaryWriter& writer, const Audio::Tag& tag, UInt8 track) {
	static map<UInt32, UInt8> Rates({
		{ 0, 0 },
		{ 5512, 1 },
		{ 7350, 2 },
		{ 8000, 3 },
		{ 11025, 4 },
		{ 12000, 5 },
		{ 16000, 6 },
		{ 18900, 7 },
		{ 22050, 8 },
		{ 24000, 9 },
		{ 32000, 10 },
		{ 37800, 11 },
		{ 44056, 12 },
		{ 44100, 13 },
		{ 47250, 14 },
		{ 48000, 15 },
		{ 50000, 16},
		{ 50400, 17 },
		{ 64000, 18 },
		{ 88200, 19 },
		{ 96000, 20 },
		{ 176400, 21 },
		{ 192000, 22 },
		{ 352800, 23 },
		{ 2822400, 24 },
		{ 5644800, 25 }
	});

	// 10CCCCCC SSSSSSSS RRRRR0IN [NNNNNNNN] TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
	/// C = codec
	/// R = rate "index"
	/// S = channels
	/// I = is config
	/// N = track
	/// T = time
	writer.write8((Media::TYPE_AUDIO << 6) | (tag.codec & 0x3F));
	writer.write8(tag.channels);

	UInt8 value;
	auto it = Rates.find(tag.rate);
	if (it == Rates.end()) {
		// if unsupported, set to 0 (to try to use config packet on player side)
		value = 0;
		WARN(tag.rate, " non supported by Media::Pack");
	} else
		value = it->second << 3;
	
	if (tag.isConfig)
		value |= 2;
	if (track==1)
		writer.write8(value);
	else
		writer.write8(value & 1).write8(track);
	return writer.write32(tag.time); // in last to be removed easly if protocol has already time info in its protocol header
}

BinaryWriter& Media::Pack(BinaryWriter& writer, const Video::Tag& tag, UInt8 track) {
	// 11CCCCCC FFFFF0ON [OOOOOOOO OOOOOOOO] [NNNNNNNN] TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
	/// C = codec
	/// F = frame (0-15)
	/// O = composition offset
	/// N = track
	/// T = time
	writer.write8((Media::TYPE_VIDEO << 6) | (tag.codec & 0x3F));
	writer.write8((tag.frame << 3) | (tag.compositionOffset ? 2 : 0) | (track != 1 ? 1 : 0));
	if (tag.compositionOffset)
		writer.write16(tag.compositionOffset);
	if (track!=1)
		writer.write8(track);
	return writer.write32(tag.time); // in last to be removed easly if protocol has already time info in its protocol header
}

BinaryWriter& Media::Pack(BinaryWriter& writer, Media::Data::Type type, UInt8 track) {
	// DATA => 0NTTTTTT [NNNNNNNN]
	/// N = track
	/// T = type
	return writer.write8((track ? 0x40 : 0) | (type & 0x3F));
}

Media::Type Media::Unpack(BinaryReader& reader, Audio::Tag& audio, Video::Tag& video, Data::Type& data, UInt8& track) {
	static UInt32 Rates[32] = { 0, 5512, 7350, 8000, 11025, 12000, 16000, 18900, 22050, 24000, 32000, 37800, 44056, 44100, 47250, 48000, 50000, 50400, 64000, 88200, 96000, 176400, 192000, 352800, 2822400, 5644800, 0, 0, 0, 0, 0, 0 };

	if (!reader.available())
		return Media::TYPE_NONE;

	UInt8 value = reader.read8();
	switch (value>>6) {
		case Media::TYPE_AUDIO:
			// 10CCCCCC SSSSSSSS RRRRR0IN [NNNNNNNN] TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
			/// C = codec
			/// R = rate "index"
			/// S = channels
			/// I = is config
			/// N = track
			/// T = time
			if (!reader.available())
				return Media::TYPE_NONE;
			audio.codec = Audio::Codec(value & 0x3F);
			audio.channels = reader.read8();

			value = reader.read8();
			audio.rate = Rates[value>>3];
			if (value & 2)
				audio.isConfig = true;

			track = ((value & 1) && reader.available()) ? reader.read8() : 1;
			audio.time = reader.read32();
			return Media::TYPE_AUDIO;

		case Media::TYPE_VIDEO:
			// 11CCCCCC FFFFF1ON [OOOOOOOO OOOOOOOO] [NNNNNNNN] TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
			/// C = codec
			/// F = frame (0-15)
			/// O = composition offset
			/// N = track
			/// T = time
			if (!reader.available())
				return Media::TYPE_NONE;
			video.codec = Video::Codec(value & 0x3F);
			value = reader.read8();
			video.frame = Video::Frame(value >> 3);
			if (value & 2)
				video.compositionOffset = reader.read16();
			track = ((value & 1) && reader.available()) ? reader.read8() : 1;
			video.time = reader.read32();
			return Media::TYPE_VIDEO;

		case Media::TYPE_DATA:
			track = reader.read8();
			break;
		default:
			track = 0;
	}
	// DATA => 0NTTTTTT [NNNNNNNN]
	/// N = track
	/// T = type
	data = Data::Type(value & 0x3F);
	return Media::TYPE_DATA;
}

Media::Data::Data(DataReader& properties, UInt8 track) : Base(TYPE_DATA, *properties, track), isProperties(true), tag(ToType(properties)) {
	if (tag)
		return;
	(Media::Data::Type&)tag = TYPE_AMF;
	shared<Buffer> pBuffer(new Buffer());
	AMFWriter writer(*pBuffer);
	properties.read(writer);
	set(pBuffer);
}
Media::Data::Data(const Properties& properties) : Base(TYPE_DATA, properties((Media::Data::Type&)tag), 0), isProperties(true) {
}

Media::Properties::Properties(const Media::Data& data) : _newProperties(false) {
	_packets.emplace_back(move(data));
	unique_ptr<DataReader> pReader(Data::NewReader(data.tag, data));
	MapWriter<Parameters> writer(self);
	pReader->read(writer);
}

void Media::Properties::onParamChange(const std::string& key, const std::string* pValue) {
	_newProperties = true;
	_packets.clear();
	Parameters::onParamChange(key, pValue);
}
void Media::Properties::onParamClear() {
	_newProperties = true; 
	_packets.clear();
	Parameters::onParamClear();
}
bool Media::Properties::flushProperties() {
	if (!_newProperties)
		return false;
	_newProperties = false;
	return true;
}

const Packet& Media::Properties::operator()(Media::Data::Type& type) const {
	if (!type) {
		// give the first available serialization
		if (!_packets.empty())
			return _packets[0];
		type = Media::Data::TYPE_JSON; // JSON by Default!
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

void Media::Properties::setProperties(UInt8 track, DataReader& reader) {
	if (!track)
		track = 1; // by default use track=1 to never override all properties (let's it to final user in using Media::Properties directly)

	// clear in first this track properties!
	String prefix(track, '.');
	clear(prefix);
	prefix.pop_back();

	// write new properties
	MapWriter<Parameters> writer(self);
	writer.beginObject();
	writer.writePropertyName(prefix.c_str());
	reader.read(writer);
	writer.endObject();
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
		case TYPE_TEXT:
		case TYPE_MEDIA:
			return new StringReader(packet.data(), packet.size());
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
		case TYPE_MEDIA:
		case TYPE_TEXT:
			return new StringWriter<>(buffer);
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
		case Media::TYPE_DATA: {
			const Media::Data& data = (const Media::Data&)media;
			if (!data.isProperties)
				return writeData(media.track, ((const Media::Data&)media).tag, media);
			unique_ptr<DataReader> pReader(Media::Data::NewReader(data.tag, data));
			return setProperties(media.track, *pReader);
		}
		default:
			WARN(typeof(self), " write a unknown media");
	}
}

Media::Source& Media::Source::Null() {
	static struct Null : Media::Source {
		void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) {}
		void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) {}
		void writeData(UInt8 track, Media::Data::Type type, const Packet& packet) {}
		void setProperties(UInt8 track, DataReader& reader) {}
		void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) {}
		void flush() {}
		void reset() {}
	} Null;
	return Null;
}

bool Media::Target::beginMedia(const string& name) {
	ERROR(typeof(*this), " doesn't support media streaming");
	return false;
}
bool Media::Target::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support audio streaming");
	return true;
}
bool Media::Target::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support video streaming");
	return true;
}
bool Media::Target::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support data streaming");
	return true;
}
bool Media::TrackTarget::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support audio streaming");
	return true;
}
bool Media::TrackTarget::writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(*this), " doesn't support video streaming");
	return true;
}
bool Media::TrackTarget::writeData(Media::Data::Type type, const Packet& packet, bool reliable) {
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

Media::Stream* Media::Stream::New(Exception& ex, const string& description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS) {
	// Net => [@][address] [type/TLS][/MediaFormat] [parameter]
	// File = > @file[.format][MediaFormat][parameter]
	
	const char* line = String::TrimLeft(description.c_str());

	bool isTarget(false);
	if ((isTarget = (*line == '@')))
		++line;

	// remove "" or ''
	string first;
	if (*line == '"' || *line =='\'') {
		const char* end = strchr(line +1, *line);
		if (end) {
			++line;
			first.assign(line, end - line);
			line = end +1;
		}
	}

	// isolate first (and remove blank)
	size_t size = 0;
	for(;;) {
		switch (line[size]) {
			default:
				++size;
				continue;
			case ' ':
			case '\t':
			case 0:;
		}
		first.append(line, size);
		line += size;
		break;
	}

	// query => parameters
	string query;
	size_t queryPos = first.find('?');
	if (queryPos != string::npos) {
		query.assign(first.c_str()+queryPos, first.size() - queryPos);
		first.resize(queryPos);
	}

	Type type(TYPE_FILE);
	bool isSecure(false);
	bool isFile(false);
	string format;
	String::ForEach forEach([&](UInt32 index, const char* value) {
		if (String::ICompare(value, "UDP") == 0) {
			isFile = false;
			type = TYPE_UDP;
			return true;
		}
		if (String::ICompare(value, "TCP") == 0) {
			isFile = false;
			if (type != TYPE_HTTP)
				type = TYPE_TCP;
			return true;
		}
		if (String::ICompare(value, "HTTP") == 0) {
			isFile = false;
			type = TYPE_HTTP;
			return true;
		}
		if (String::ICompare(value, "TLS") == 0) {
			isSecure = true;
			return true;
		}
		if (String::ICompare(value, "FILE") == 0) {
			// force file!
			isFile = true;
			type = TYPE_FILE;
			return true;
		}
		format = value;
		return false;
	});

	const char* params = strpbrk(line = String::TrimLeft(line), " \t\r\n\v\f");
	String::Split(line, params ? (line - params) : string::npos, "/", forEach, SPLIT_IGNORE_EMPTY);
	
	Path   path;
	SocketAddress address;
	UInt16 port;
	
	if (!isFile) {
		// if is not explicitly a file, test if it's a port
		size = first.find_first_of("/\\", 0);
		if (size == string::npos)
			size = first.size(); // to fix past.set (doesn't work with string::npos)
		if (String::ToNumber(first.data(), size, port)) {
			address.setPort(port);
			path.set(first.c_str() + size);
			if (type != TYPE_UDP || isTarget) // if no host and TCP or target
				address.host().set(IPAddress::Loopback());
		} else {
			bool isAddress = false;
			// Test if it's a address
			{
				String::Scoped scoped(first.data() + size);
				Exception exc;
				isAddress = address.set(exc, first.data());
				if (!isAddress && type) {
					// explicitly indicate as network, and however address invalid!
					ex = exc;
					return NULL;
				}
			}
			if (isAddress) {
				path.set(first.c_str() + size);
				if (!address.host() && (type != TYPE_UDP || isTarget)) {
					ex.set<Ex::Net::Address::Ip>("A TCP or target Stream can't have a wildcard ip");
					return NULL;
				}
			} else {
				if (!path.set(move(first))) {
					ex.set<Ex::Format>("No file name in stream file description");
					return NULL;
				}
				if (path.isFolder()) {
					ex.set<Ex::Format>("Stream file ", path, " can't be a folder");
					return NULL;
				}
				isFile = true;
				type = TYPE_FILE;
			}
		}
	}
	
	// fix params!
	if (params) {
		while (isspace(*params)) {
			if (!*++params) {
				params = NULL;
				break;
			}
		}
	}

	if (format.empty()) {
		switch (type) {
			case TYPE_UDP:
				// UDP and No Format => TS by default to catch with VLC => UDP = TS
				format = "mp2t";
				break;
			case TYPE_HTTP:
				if (path.isFolder()) {
					ex.set<Ex::Format>("A HTTP source or target stream can't be a folder");
					return NULL;
				}
			default: {
				const char* subMime;
				if (MIME::Read(path, subMime)) {
					format = subMime;
					break;
				}
				if (!isTarget && type == TYPE_HTTP)
					break; // HTTP source allow empty format (will be determinate with content-type)
				ex.set<Ex::Format>(TypeToString(type), " stream description have to indicate a media format");
				return NULL;
			}
		}
	}
	
	Stream* pStream;
	if (isFile) {
		if (isTarget) {
			pStream = MediaFile::Writer::New(path, format.c_str(), ioFile);
			if (!pStream) {
				ex.set<Ex::Unsupported>("Target stream file format ", format, " not supported");
				return NULL;
			}
		} else {
			pStream = MediaFile::Reader::New(path, format.c_str(), timer, ioFile);
			if (!pStream) {
				ex.set<Ex::Unsupported>("Source stream file format ", format, " not supported");
				return NULL;
			}
		}
	} else {
		if (!type)
			type = String::ICompare(format, EXPAND("RTP")) == 0 ? TYPE_UDP : TYPE_TCP;

		// KEEP this model of double creation to allow a day a new RTPWriter<...>(parameter)
		if (isTarget) {
			pStream = MediaSocket::Writer::New(type, path, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
			if(!pStream) {
				ex.set<Ex::Unsupported>("Target stream ", TypeToString(type), " format ", format, " not supported");
				return NULL;
			}
		} else {
			pStream = MediaSocket::Reader::New(type, path, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
			if (!pStream) {
				ex.set<Ex::Unsupported>("Source stream ", TypeToString(type), " format ", format, " not supported");
				return NULL;
			}
		}
	}
	
	(string&)pStream->query = move(query);
	return pStream;
}

} // namespace Mona
