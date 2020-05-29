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
#include "Mona/SplitWriter.h"

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
		writer.write8(value | 1).write8(track);
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
			audio.isConfig = value & 2 ? true : false;

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
			video.compositionOffset = value & 2 ? reader.read16() : 0;
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

void Media::Properties::onParamInit() {
	if (_packets.size()) // else no packets
		addProperties(_track, Media::Data::Type(_packets.size()+1), _packets.back());
}
void Media::Properties::onParamChange(const std::string& key, const std::string* pValue) {
	_packets.clear();
	Parameters::onParamChange(key, pValue);
}
void Media::Properties::onParamClear() {
	_packets.clear();
	Parameters::onParamClear();
}
const Packet& Media::Properties::data(Media::Data::Type& type) const {
	if(!type) {
		// JSON in priority if available!
		if (_packets.size() >= Media::Data::TYPE_JSON && _packets[Media::Data::TYPE_JSON - 1])
			return _packets[(type=Media::Data::TYPE_JSON) - 1];
		// Or give the first available serialization
		for (UInt32 i = 0; i < _packets.size(); ++i) {
			if (_packets[i]) {
				type = Media::Data::Type(i + 1);
				return _packets[i];
			}
		}
		// Else serialize to JSON
		type = Media::Data::TYPE_JSON;
	}
	// not considerate the empty() case, because empty properties must write a object empty to match onMetaData(obj) with argument on clear properties!
	UInt32 oldSize = _packets.size();
	if(type>oldSize)
		_packets.resize(type);
	Packet& packet = _packets[type - 1];
	if (!packet) {
		// Serialize in the format requested!
		shared<Buffer> pBuffer(SET);
		unique<DataWriter> pWriter(Media::Data::NewWriter(type, *pBuffer));
		if (!pWriter) {
			_packets.resize(oldSize);
			WARN("Properties type ", Data::TypeToString(type), " unsupported");
			return data(type = Data::TYPE_JSON);
		}
		MapReader<Parameters>(self).read(*pWriter);
		packet.set(pBuffer);
	}
	return packet;
}
UInt32 Media::Properties::addProperties(UInt8 track, DataReader& reader) {
	// add new properties
	MapWriter<Parameters> writer(self);
	if(!track)
		return reader.read(writer, 1); // just one argument!
	writer.beginObject();
	writer.writePropertyName(String(track).c_str());
	UInt32 count = reader.read(writer, 1); // just one argument!
	writer.endObject(); // impossible to save packet, not represent all metadata!
	return count;
}
void Media::Properties::addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) {
	// keep _packets.empty condition BEFORE count() condition to avoid a overflow call while onParamInit (count() can call onParamInit)
	if (type && _packets.empty() && !count()) { 
		_packets.resize(type);
		_packets.back().set(packet);
		_track = track;
		return;
	}
	std::deque<Packet> packets = move(_packets); // to avoid _packets clear of onParamChange/Clear while onParamInit!
	unique<DataReader> pReader(Media::Data::NewReader(type, packet));
	if (pReader)
		addProperties(track, *pReader);
	else
		WARN("Properties type ", Data::TypeToString(type), " unsupported");
	if (!packets.empty() && &packet == &packets.back())
		_packets = move(packets); // was onParamInit!
}

void Media::Properties::clearTracks() {
	UInt16 track;
	auto it = lower_bound("1");
	while (it != end()) {
		if (it->first[0] > '9')
			break;
		size_t found = it->first.find('.');
		if (found != string::npos && String::ToNumber(it->first.data(), found, track))
			it = erase(it);
		else
			++it;
	}
}
void Media::Properties::clear(UInt8 track) {
	if (track) {
		erase(lower_bound(String(track, '.')), lower_bound(String(track, '.' + 1)));
		return;
	}
	// erase all what is not a metadata track
	auto it = begin();
	while (it != end()) {
		size_t found = it->first.find('.');
		if (found == string::npos || !String::ToNumber(it->first.data(), found, track))
			it = erase(it);
		else
			++it;
	}
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
	if (String::ICompare(subMime, EXPAND("query")) == 0 || String::ICompare(subMime, EXPAND("x-www-form-urlencoded")) == 0)
		return TYPE_QUERY;
	return TYPE_UNKNOWN;
}

unique<DataReader> Media::Data::NewReader(Type type, const Packet& packet) {
	switch (type) {
		case TYPE_JSON: {
			unique<JSONReader> pReader(SET, packet);
			if (pReader->isValid())
				return move(pReader);
			break;
		}
		case TYPE_XMLRPC: {
			unique<XMLRPCReader> pReader(SET, packet);
			if (pReader->isValid())
				return move(pReader);
			break;
		}
		case TYPE_AMF:
		case TYPE_AMF0:
			return make_unique<AMFReader>(packet);
		case TYPE_QUERY:
			return make_unique<QueryReader>(packet);
		case TYPE_TEXT:
			return make_unique<StringReader>(packet);
		case TYPE_MEDIA:
		case TYPE_UNKNOWN:;
		// do default to be warned if we have added a Media::Date::Type and we have forgotten to add the related switch/case
	}
	return nullptr;
}

unique<DataWriter> Media::Data::NewWriter(Type type, Buffer& buffer) {
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
	return nullptr;
}

void Media::Source::addProperties(const Media::Properties& properties) {
	Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
	const Packet& packet = properties.data(type);
	addProperties(0, type, packet);
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
				return addProperties(media.track, data.tag, media);
			return writeData(data.tag, media, media.track);
		}
		default:
			WARN(typeof(self), " write an unknown media ", UInt8(media.type));
	}
}

Media::Source& Media::Source::Null() {
	static struct Null : Media::Source {
		void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track=1) {}
		void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track=1) {}
		void writeData(Media::Data::Type type, const Packet& packet, UInt8 track=0) {}
		void addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) {}
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

} // namespace Mona
