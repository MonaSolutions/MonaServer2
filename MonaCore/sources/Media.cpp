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
#include "Mona/MediaServer.h"

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
	if(!track)
		return writer.write8(type & 0x3F);
	return writer.write8(0x40 | (type & 0x3F)).write8(track);
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

	// DATA => 0NTTTTTT [NNNNNNNN]
	/// N = track
	/// T = type
		case Media::TYPE_DATA:
			track = reader.read8();
			break;
		default:
			track = 0;
	}
	data = Data::Type(value & 0x3F);
	return Media::TYPE_DATA;
}

Media::Properties::Properties(const Media::Data& data) : _packets(1, move(data)), _timeProperties(0) {
	unique<DataReader> pReader(Data::NewReader(data.tag, data, Media::Data::TYPE_TEXT));
	MapWriter<Parameters> writer(self);
	pReader->read(writer);
}

void Media::Properties::onParamChange(const std::string& key, const std::string* pValue) {
	_packets.clear();
	_timeProperties.update();
	Parameters::onParamChange(key, pValue);
}
void Media::Properties::onParamClear() {
	_packets.clear();
	_timeProperties.update();
	Parameters::onParamClear();
}

const Packet& Media::Properties::operator[](Media::Data::Type type) const {
	if (!type)
		return this->operator()(type);
	// not considerate the empty() case, because empty properties must write a object empty to match onMetaData(obj) with argument on clear properties!
	_packets.resize(type);
	Packet& packet(_packets[type - 1]);
	if (packet)
		return packet;
	// Serialize in the format requested!
	shared<Buffer> pBuffer(SET);
	unique<DataWriter> pWriter(Media::Data::NewWriter(type, *pBuffer));
	MapReader<Parameters> reader(self);
	reader.read(*pWriter);
	return packet.set(pBuffer);
}
const Packet& Media::Properties::operator()(Media::Data::Type& type) const {
	// give the first available serialization or serialize to JSON by default
	UInt32 i = 0;
	for (const Packet& packet : _packets) {
		++i;
		if (packet) {
			type = (Media::Data::Type)i;
			return packet;
		}
	}
	return this->operator[](type=Media::Data::TYPE_JSON);
}


void Media::Properties::setProperties(Media::Data::Type type, const Packet& packet, UInt8 track) {
	if (!track)
		track = 1; // by default use track=1 to never override all properties (let's it to final user in using Media::Properties directly)

	unique<DataReader> pReader(Media::Data::NewReader(type, packet, Media::Data::TYPE_TEXT));

	// clear in first this track properties!
	String prefix(track, '.');
	clear(prefix);
	prefix.pop_back();

	// write new properties
	MapWriter<Parameters> writer(self);
	writer.beginObject();
	writer.writePropertyName(prefix.c_str());
	pReader->read(writer);
	writer.endObject();

	// Save packet formatted!
	_packets.resize(type);
	_packets[type - 1].set(move(packet));
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

unique<DataReader> Media::Data::NewReader(Type type, const Packet& packet, Type alternateType) {
	switch (type) {
		case TYPE_JSON: {
			unique<JSONReader> pReader(SET, packet.data(), packet.size());
			if (pReader->isValid())
				return move(pReader);
			break;
		}
		case TYPE_XMLRPC: {
			unique<XMLRPCReader> pReader(SET, packet.data(), packet.size());
			if (pReader->isValid())
				return move(pReader);
			break;
		}
		case TYPE_AMF:
		case TYPE_AMF0:
			return make_unique<AMFReader>(packet.data(), packet.size());
		case TYPE_QUERY:
			return make_unique<QueryReader>(packet.data(), packet.size());
		case TYPE_TEXT:
			return make_unique<StringReader>(packet.data(), packet.size());
		case TYPE_MEDIA:
		case TYPE_UNKNOWN:;
		// do default to be warned if we have added a Media::Date::Type and we have forgotten to add the related switch/case
	}
	return alternateType ? NewReader(alternateType, packet) : nullptr;
}

unique<DataWriter> Media::Data::NewWriter(Type type, Buffer& buffer, Type alternateType) {
	switch (type) {
		case TYPE_JSON:
			return make_unique<JSONWriter>(buffer);
		case TYPE_XMLRPC:
			return make_unique<XMLRPCWriter>(buffer);
		case TYPE_AMF:
			return make_unique<AMFWriter>(buffer);
		case TYPE_AMF0:
			return make_unique<AMFWriter>(buffer, true);
		case TYPE_QUERY:
			return make_unique<QueryWriter>(buffer);
		case TYPE_TEXT:
			return make_unique<StringWriter<>>(buffer);
		case TYPE_MEDIA:
		case TYPE_UNKNOWN:;
		// do default to be warned if we have added a Media::Date::Type and we have forgotten to add the related switch/case
	}
	return alternateType ? NewWriter(alternateType, buffer) : nullptr;
}


void Media::Source::setProperties(const Media::Properties& properties, UInt8 track) {
	MapReader<Parameters> reader(properties);
	Media::Data::Type type;
	const Packet& packet = properties(type);
	setProperties(type, packet, track);
}
void Media::Source::setProperties(DataReader& reader, UInt8 track) {
	shared<Buffer> pBuffer(SET);
	JSONWriter writer(*pBuffer);
	reader.read(writer);
	setProperties(Media::Data::TYPE_JSON, Packet(pBuffer), track);
}


void Media::Source::writeMedia(const Media::Base& media) {
	switch (media.type) {
		case Media::TYPE_AUDIO:
			return writeAudio(((const Media::Audio&)media).tag, media, media.track);
		case Media::TYPE_VIDEO:
			return writeVideo(((const Media::Video&)media).tag, media, media.track);
		case Media::TYPE_DATA: {
			const Media::Data& data = (const Media::Data&)media;
			if (data.isProperties)
				return setProperties(data.tag, media, media.track);
			return writeData(data.tag, media, media.track);
		}
		default:
			WARN(typeof(self), " write a unknown media ", media.type);
	}
}

Media::Source& Media::Source::Null() {
	static struct Null : Media::Source {
		void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track=1) {}
		void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track=1) {}
		void writeData(Media::Data::Type type, const Packet& packet, UInt8 track=0) {}
		void setProperties(Media::Data::Type type, const Packet& packet, UInt8 track=1) {}
		void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) {}
		void flush() {}
		void reset() {}
	} Null;
	return Null;
}

bool Media::Target::beginMedia(const string& name) {
	ERROR(typeof(self), " doesn't support media streaming");
	return false;
}
bool Media::Target::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(self), " doesn't support audio streaming");
	return true;
}
bool Media::Target::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(self), " doesn't support video streaming");
	return true;
}
bool Media::Target::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) {
	WARN(typeof(self), " doesn't support data streaming");
	return true;
}
bool Media::TrackTarget::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(self), " doesn't support audio streaming");
	return true;
}
bool Media::TrackTarget::writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	WARN(typeof(self), " doesn't support video streaming");
	return true;
}
bool Media::TrackTarget::writeData(Media::Data::Type type, const Packet& packet, bool reliable) {
	WARN(typeof(self), " doesn't support data streaming");
	return true;
}

MediaStream::MediaStream(Type type, const Path& path, Media::Source& source) :
	_starting(false), _running(false), targets(_targets), _startCount(0),
	type(type), path(path), source(source) {
}
void MediaStream::start(const Parameters& parameters) {
	ex = nullptr; // reset lastEx on pullse start!
	if (_running && !_starting)
		return; // nothing todo, starting already done!
	if (!_startCount && !_running) {
		// do it just on first start call!
		Media::Target* pTarget = dynamic_cast<Media::Target*>(this);
		if (pTarget) {
			// This is a target, with beginMedia it can be start, so create a _pTarget and keep it alive all the life-time of this Stream
			// Indeed it can be stopped and restarted with beginMedia!
			_pTarget = shared<Media::Target>(make_shared<Media::Target>(), pTarget); // aliasing
			onNewTarget(_pTarget);
		}
	}
	_starting = true;
	if (starting(parameters))
		finalizeStart();
	if (_starting) // _starting can switch to false if finalizeStart (then _running is already set to true) and on stop (then _running must stay on false)
		_running = true;
}
bool MediaStream::finalizeStart() {
	if (!_starting)
		return false;
	if (!_running) // called while starting!
		_running = true;
	if (onStart && !onStart()) {
		stop();
		return false;
	}
	_starting = false;
	++_startCount;
	INFO(description(), " starts");
	return true;
}
void MediaStream::stop() {
	if (!_running && !_starting)
		return;
	stopping();
	_running = false;
	_targets.clear(); // to invalid targets!
	if (_starting) {
		_starting = false;
		return;
	}
	INFO(description(), " stops");
	onStop(); // in last to allow possibily a delete this (beware impossible with Subscription usage!)
}
shared<const Socket> MediaStream::socket() const {
	if (type>0)
		WARN(typeof(self), " should implement socket()");
	return nullptr;
}
shared<const File> MediaStream::file() const {
	if (type == TYPE_FILE)
		WARN(typeof(self), " should implement file()");
	return nullptr;
}

shared<Socket> MediaStream::newSocket(const Parameters& parameters, const shared<TLS>& pTLS) {
	if (type <= 0)
		return nullptr;
	shared<Socket> pSocket;
	if (type == TYPE_SRT) {
#if defined(SRT_API)
		pSocket.set<SRT::Socket>();
		return pSocket;
#endif
		ERROR(description(), "SRT unsupported replacing by UDP (build MonaBase with SRT support before)");
	}
	if (pTLS)
		pSocket.set<TLS::Socket>(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM, pTLS);
	else
		pSocket.set(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM);
	Exception ex;
	AUTO_ERROR(pSocket->processParams(ex, parameters, "stream"), description());
	return pSocket;
}

unique<MediaStream> MediaStream::New(Exception& ex, Media::Source& source, const string& description, const Timer& timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS) {
	// Net => [address] [type/TLS][/MediaFormat] [parameter]
	// File = > file[.format][MediaFormat][parameter]
	
	const char* line = String::TrimLeft(description.c_str());

	bool isTarget(&source==&Media::Source::Null());
	bool isBind = *line == '@';
	if (isBind)
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
		if (String::ICompare(value, "SRT") == 0) {
			isFile = false;
			type = TYPE_SRT;
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

#if !defined(SRT_API)
	if (type == TYPE_SRT) {
		ex.set<Ex::Unsupported>(TypeToString(type), " stream not supported, build MonaBase with SRT support first");
		return nullptr;
	}
#endif
	
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
			if (!isBind && ((type != TYPE_UDP && type != TYPE_SRT) || isTarget)) // if no host and TCP/HTTP or target
				address.host().set(IPAddress::Loopback());
		} else {
			bool isAddress = false;
			// Test if it's an address
			{
				String::Scoped scoped(first.data() + size);
				Exception exc;
				isAddress = address.set(exc, first.data());
				if (!isAddress && type) {
					// explicitly indicate as network, and however address invalid!
					ex = exc;
					return nullptr;
				}
			}
			if (isAddress) {
				path.set(first.c_str() + size);
				if (!isBind && !address.host() && ((type != TYPE_UDP && type != TYPE_SRT) || isTarget)) {
					ex.set<Ex::Net::Address::Ip>("Wildcard binding impossible for a stream ", (isTarget ? "target " : "source "), TypeToString(type));
					return nullptr;
				}
			} else {
				if (!path.set(move(first))) {
					ex.set<Ex::Format>("No file name in stream file description");
					return nullptr;
				}
				if (path.isFolder()) {
					ex.set<Ex::Format>("Stream file ", path, " can't be a folder");
					return nullptr;
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
			case TYPE_SRT: // SRT and No Format => TS by default to catch Haivision usage
			case TYPE_UDP: // UDP and No Format => TS by default to catch with VLC => UDP = TS
				format = "mp2t";
				break;
			case TYPE_HTTP:
				if (path.isFolder()) {
					ex.set<Ex::Format>("A HTTP source or target stream can't be a folder");
					return nullptr;
				}
			default: {
				const char* subMime;
				if (MIME::Read(path, subMime)) {
					format = subMime;
					break;
				}
				if (!isTarget && type == TYPE_HTTP)
					break; // HTTP source allow empty format (will be determinate with content-type)
				if(path.extension().empty())
					ex.set<Ex::Format>(TypeToString(type), " stream description have to indicate a media format");
				else
					ex.set<Ex::Format>(TypeToString(type), " stream path has a format ", path.extension(), " unknown or not supported");
				return nullptr;
			}
		}
	}
	
	unique<MediaStream> pStream;
	if (isFile) {
		if (isTarget)
			pStream = MediaFile::Writer::New(path, format.c_str(), ioFile);
		else
			pStream = MediaFile::Reader::New(path, source, format.c_str(), timer, ioFile);
	} else {
		if (!type) // TCP by default excepting if format is RTP where rather UDP by default
			type = String::ICompare(format, "RTP") == 0 ? TYPE_UDP : TYPE_TCP;
		// KEEP this model of double creation to allow a day a new RTPWriter<...>(parameter)
		if (type != TYPE_UDP && (isBind || !address.host())) { // MediaServer
			if (isTarget)
				pStream = MediaServer::Writer::New(MediaServer::Type(type), path, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
			else
				pStream = MediaServer::Reader::New(MediaServer::Type(type), path, source, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
		}
		else if (isTarget)
			pStream = MediaSocket::Writer::New(type, path, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
		else
			pStream = MediaSocket::Reader::New(type, path, source, format.c_str(), address, ioSocket, isSecure ? pTLS : nullptr);
	}

	if (!pStream) {
		ex.set<Ex::Unsupported>(isTarget ? "Target stream " : "Source stream ", TypeToString(type), " format ", format, " not supported");
		return nullptr;
	}
	(string&)pStream->query = move(query);
	return pStream;
}

} // namespace Mona
