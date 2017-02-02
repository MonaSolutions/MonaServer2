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
#include "Mona/Packet.h"
#include "Mona/Parameters.h"
#include "Mona/DataWriter.h"
#include "Mona/DataReader.h"
#include "Mona/IOSocket.h"
#include "Mona/IOFile.h"
#include "Mona/TLS.h"
#include "Mona/MIME.h"
#include "Mona/Logs.h"


namespace Mona {


struct Media : virtual Static {
	enum Type {
		TYPE_NONE=0,
		TYPE_AUDIO=1,
		TYPE_VIDEO=2,
		TYPE_DATA =4 // to be compatible with UInt8 _reliability of Subcription
	};

	struct Base : Packet, virtual Object {
		Base() : type(TYPE_NONE), track(0) {}
		Base(Media::Type type, UInt16 track) : type(type), track(track) {}
		Base(Media::Type type, UInt16 track, const Packet& packet) : type(type), track(track), Packet(std::move(packet)) {}
		const Media::Type type;
		const UInt16	  track;
	};

	struct Data : Base, virtual Object {
		enum Type {
			TYPE_UNKNOWN = 0,
			TYPE_AMF,
			TYPE_AMF0,
			TYPE_JSON,
			TYPE_XMLRPC,
			TYPE_QUERY
		};
		typedef Type Tag;
		static const char* TypeToString(Type type) {
			static const char* Strings[] = { "Unknown", "AMF", "AMF0", "JSON", "XMLRPC", "QUERY" };
			return Strings[UInt8(type)];
		}
		static const char* TypeToSubMime(Type type) {
			static const char* SubMimes[] = { "octet-stream", "x-amf", "x-amf", "json", "xml", "x-www-form-urlencoded" };
			return SubMimes[UInt8(type)];
		}

		template<typename DataType>
		static Type ToType() { return ToType(typeid(DataType)); }
		static Type ToType(DataWriter& writer) { return ToType(typeid(writer)); }
		static Type ToType(DataReader& reader) { return ToType(typeid(reader)); }
		static Type ToType(const std::string& subMime) { return ToType(subMime.c_str()); }
		static Type ToType(const char* subMime);

		static DataReader* NewReader(Type type, const Packet& packet);
		static DataWriter* NewWriter(Type type, Buffer& buffer);

		Data(UInt16 track, Media::Data::Type type, const Packet& packet) : Base(TYPE_DATA, track, packet), tag(type) {}
		/*!
		Properties usage! */
		Data(UInt16 track, DataReader& properties);

		const Media::Data::Type	tag;

	private:
		static Type ToType(const std::type_info& info);
	};

	struct Video : Base, virtual Object {
		enum Codec { // Aligned values with FLV codec value
			CODEC_RAW = 0,
			CODEC_JPEG = 1,
			CODEC_SORENSON = 2,
			CODEC_SCREEN1 = 3,
			CODEC_VP6 = 4,
			CODEC_VP6_ALPHA = 5,
			CODEC_SCREEN2 = 6,
			CODEC_H264 = 7,
			CODEC_H263 = 8,
			CODEC_MPEG4_2 = 9
		};
		static const char* CodecToString(Codec codec) {
			static const char* Strings[] = { "RAW", "JPEG", "SORENSON", "SCREEN1", "VP6", "VP6_ALPHA", "SCREEN2", "H264", "H263", "MPEG4_2" };
			return Strings[UInt8(codec)];
		}
	
		enum Frame { // Aligned values with FLV frame type value, and ignore FRAME_GENERATED_KEYFRAME which is redundant with FRAME_KEY for a "key frame" test condition
			FRAME_KEY = 1,
			FRAME_INTER = 2, // Used too by H264 for Config sequence
			FRAME_DISPOSABLE_INTER = 3, // just for H263
			FRAME_INFO = 5,
			FRAME_CONFIG = 7
		};
		struct Tag : virtual Object {
			explicit Tag() : frame(FRAME_KEY), compositionOffset(0) {}
			explicit Tag(Codec codec) : codec(codec), frame(FRAME_KEY), compositionOffset(0) {}
			explicit Tag(const Tag& other) : codec(other.codec), frame(other.frame), time(other.time), compositionOffset(other.compositionOffset) {}
			explicit Tag(BinaryReader& reader, bool withTime = true) { unpack(reader, withTime); }

			UInt32 time;
			Codec  codec;
			Frame  frame;
			UInt32 compositionOffset;

			// time 32 bits
			// codec 5 bits (0-31)
			// frame 3 bits (0-7)
			// compositionOffset 32 bits (optional)
			// SIZE IS IMPAIR
			UInt8			packSize(bool withTime = true) const { return compositionOffset ? (withTime ? 9 : 5) : (withTime ? 5 : 1); }
			BinaryWriter&	pack(BinaryWriter& writer, bool withTime = true) const;
			BinaryReader&	unpack(BinaryReader& reader, bool withTime = true);
		};
		struct Config : Tag, Packet, virtual NullableObject {
			// always time=0 for config save, because will be the first packet given (subscription starts to 0)
			explicit Config() { time = 0; }
			explicit Config(const Tag& tag, const Packet& packet) { time = 0; set(tag, packet); }
			operator bool() const { return frame == FRAME_CONFIG; ; }
			void reset() {
				frame = FRAME_KEY;
				Packet::reset();
			}
			Config& set(const Tag& tag, const Packet& packet) {
				frame = FRAME_CONFIG;
				codec = tag.codec;
				compositionOffset = tag.compositionOffset;
				Packet::set(std::move(packet));
				return *this;
			}
		};

		Video(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) : Base(TYPE_VIDEO, track, packet), tag(tag) {}
		const Media::Video::Tag	tag;
	};
	static const char* CodecToString(Video::Codec codec) { return Video::CodecToString(codec); }

	struct Audio : Base, virtual Object {
		enum Codec { // Aligned values with FLV codec value
			CODEC_RAW = 0,
			CODEC_ADPCM = 1,
			CODEC_MP3 = 2,
			CODEC_PCM_LITTLE = 3,
			CODEC_NELLYMOSER_16K = 4,
			CODEC_NELLYMOSER_8K = 5,
			CODEC_NELLYMOSER = 6,
			CODEC_G711A = 7,
			CODEC_G711U = 8,
			CODEC_AAC = 10,
			CODEC_SPEEX = 11,
			CODEC_MP3_8K = 14
		};
		static const char* CodecToString(Codec codec) {
			static const char* Strings[] = { "RAW", "ADPCM", "MP3", "PCM_LITTLE", "NELLYMOSER_16K", "NELLYMOSER_8K", "NELLYMOSER", "G711A", "G711U", "UNKNOWN", "AAC", "SPEEX", "UNKNOWN", "UNKNOWN", "MPEG4_2" };
			return Strings[UInt8(codec)];
		}
		struct Tag : virtual Object {
			explicit Tag() : rate(0), channels(0), isConfig(false) {}
			explicit Tag(Codec codec) : codec(codec), rate(0), channels(0), isConfig(false) {}
			explicit Tag(const Tag& other) : codec(other.codec), rate(other.rate), time(other.time), channels(other.channels), isConfig(other.isConfig) {}
			explicit Tag(BinaryReader& reader, bool withTime = true) { unpack(reader, withTime); }


			UInt32	time;
			Codec	codec;
			bool	isConfig;
			UInt8	channels;
			UInt32	rate;

			// time 32 bits
			// isConfig 1 bit
			// codec 5 bits (0-31)
			// channels 6 bits (0-62)
			// rate 4 bits (0-15)
			// SIZE IS PAIR
			UInt8			packSize(bool withTime = true) const { return withTime ? 6 : 2; }
			BinaryWriter&	pack(BinaryWriter& writer, bool withTime=true) const;
			BinaryReader&	unpack(BinaryReader& reader, bool withTime = true);
		};
		struct Config : Tag, Packet, virtual NullableObject {
			// always time=0 for config save, because will be the first packet given (subscription starts to 0)
			explicit Config() { time = 0;  }
			explicit Config(const Tag& tag, const Packet& packet) { time = 0; set(tag, packet); }
			operator bool() const { return isConfig ? true : false; }
			void reset() {
				isConfig = false;
				Packet::reset();
			}
			Config& set(const Tag& tag, const Packet& packet) {
				isConfig = true;
				codec = tag.codec;
				channels = tag.channels;
				rate = tag.rate;
				Packet::set(std::move(packet));
				return *this;
			}
		};

		Audio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) : Base(TYPE_AUDIO, track, packet), tag(tag) {}
		const Media::Audio::Tag	tag;
	};
	static const char* CodecToString(Audio::Codec codec) { return Audio::CodecToString(codec); }

	static Media::Type TagType(Data::Type) { return TYPE_DATA; }
	static Media::Type TagType(const Audio::Tag& tag) { return TYPE_AUDIO; }
	static Media::Type TagType(const Video::Tag& tag) { return TYPE_VIDEO; }

	struct Properties : Parameters, virtual NullableObject {
		explicit operator bool() const { return !Parameters::empty(); }

		const Packet& operator[](Media::Data::Type type) const;

	protected:
		Properties() {}

		virtual void onParamChange(const std::string& key, const std::string* pValue) { _packets.clear(); Parameters::onParamChange(key, pValue); }
		virtual void onParamClear() { _packets.clear(); Parameters::onParamClear(); }
	private:
		mutable std::vector<Packet>	_packets;
	};


	/*!
	To write a media part from source (just a part of one media, so no beginMedia/endMedia and writeProperties) */
	struct Source : virtual Object {
		virtual const std::string&	name() const { static std::string Name("?");  return Name; }
		virtual void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) = 0;
		virtual void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) = 0;
		virtual void writeData(UInt16 track, Media::Data::Type type, const Packet& packet) = 0;
		virtual void writeProperties(UInt16 track, DataReader& reader) = 0;
		virtual void reportLost(Media::Type type, UInt32 lost) = 0;
		virtual void reportLost(Media::Type type, UInt16 track, UInt32 lost) = 0;
		virtual void flush() = 0;
		virtual void reset() = 0;

		/*!
		Write media on default track = 0 */
		void writeAudio(const Media::Audio::Tag& tag, const Packet& packet) { writeAudio(0, tag, packet); }
		void writeVideo(const Media::Video::Tag& tag, const Packet& packet) { writeVideo(0, tag, packet); }
		void writeData(Media::Data::Type type, const Packet& packet) { writeData(0, type, packet); }
	
		void writeMedia(Media::Base& media);

		static Source& Null();
	};

	/*!
	Complete media target, has begin/end and Properties */
	struct Target : virtual Object {
		virtual const Congestion&	congestion() const = 0;

		virtual bool beginMedia(const std::string& name, const Parameters& parameters);
		virtual bool writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable);
		virtual bool writeProperties(const Media::Properties& properties) { return true; }
		virtual void endMedia(const std::string& name) {}
		/*!
		Overload just if target bufferizes data before to send it*/
		virtual void flush() {}

		static Target& Null();
	};

	/*!
	A Media stream (sources or targets) allows mainly to bound a publication with input and ouput stream.
	Its behavior must support an automatic mode to (re)start the stream as long time as desired
	Implementation have to call protected stop(...) to log and callback an error if the user prefer delete stream on error
	/!\ start() can be used to pulse the stream (connect attempt) */
	struct Stream : virtual Object {
		typedef Event<void(const Exception&)> ON(Error);

		static UInt32 RecvBufferSize;
		static UInt32 SendBufferSize;

		enum Type {
			TYPE_FILE=0,
			TYPE_UDP,
			TYPE_TCP,
			TYPE_HTTP
		};

		static const char* TypeToString(Type type) { static const char* Strings[] = { "file", "udp", "tcp", "http" }; return Strings[UInt8(type)]; }

	
		const Type			type;
		const std::string&	description() const { return _description.empty() ? ((Stream*)this)->buildDescription(_description) : _description; }
		

		/*!
		Reader =>  [address] [type/TLS][/MediaFormat] [parameter]
		Writer => @[address:]port...
		Near of SDP syntax => m=audio 58779 [UDP/TLS/]RTP/SAVPF [111 103 104 9 0 8 106 105 13 126] 
		File => @file[.format] [MediaFormat] [parameter] */
		static Stream* New(Exception& ex, const char* description, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS);

		/*!
		/!\ Implementation have to support a pulse start! */
		virtual void start() = 0;
		virtual void start(Media::Source& source);
		virtual bool running() const = 0;
		virtual void stop() = 0;
	protected:
		Stream(Type type) : type(type) {}

		void stop(const Exception& ex);
		void stop(LOG_LEVEL level, const Exception& ex) {
			LOG(level, description(), ", ", ex);
			stop(ex);
		}
		template<typename ExType, typename ...Args>
		void stop(LOG_LEVEL level, Args&&... args) {
			Exception ex;
			LOG(level, description(), ", ", ex.set<ExType>(args ...));
			stop(ex);
		}
	private:
		virtual std::string& buildDescription(std::string& description) = 0;

		mutable std::string	_description;
	};

};

} // namespace Mona
