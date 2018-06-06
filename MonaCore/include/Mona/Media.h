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
#include "Mona/Timer.h"
#include "Mona/Logs.h"


namespace Mona {


struct Media : virtual Static {
	enum Type {
		TYPE_NONE=0,
		TYPE_DATA = 1,
		TYPE_AUDIO = 2, // => 10, to have the first bit to 1 and be compatible with Media::Pack
		TYPE_VIDEO = 3, // => 11, to have the first bit to 1 and be compatible with Media::Pack
		// these values allow to write media type on 2 bits!
	};
	static const char* TypeToString(Type type) {
		static const char* Strings[] = { "none", "data", "audio", "video"};
		return Strings[UInt8(type)];
	}

	struct Base : Packet, virtual Object {
		Base() : type(TYPE_NONE), track(0) {}
		Base(Media::Type type, const Packet& packet, UInt8 track) : type(type), track(track), Packet(std::move(packet)) {}

		bool   hasTime() const { return type>TYPE_DATA; }
		UInt32 time() const;
		void   setTime(UInt32 time);

		UInt32 compositionOffset() const { return type == TYPE_VIDEO ? ((Media::Video*)this)->tag.compositionOffset : 0; }
		bool   isConfig() const;
		Media::Type type;
		UInt8		track;
	};

	struct Properties;
	struct Data : Base, virtual Object {
		enum Type {
			TYPE_UNKNOWN = 0,
			TYPE_AMF = 1,
			TYPE_AMF0 = 2,
			TYPE_JSON = 3,
			TYPE_XMLRPC = 4,
			TYPE_QUERY = 5,
			TYPE_TEXT = 6,
			TYPE_MEDIA = 0xFF, // just used in intern in Mona!
		};
		typedef Type Tag;
		static const char* TypeToString(Type type) {
			static const char* Strings[] = { "Unknown", "AMF", "AMF0", "JSON", "XMLRPC", "QUERY", "Text", "Media" };
			return Strings[UInt8(type)];
		}
		static const char* TypeToSubMime(Type type) {
			static const char* SubMimes[] = { "octet-stream", "x-amf", "x-amf", "json", "xml", "x-www-form-urlencoded","text", "octet-stream" };
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

		Data(Media::Data::Type type, const Packet& packet, UInt8 track = 0, bool isProperties=false) : Base(TYPE_DATA, packet, track), isProperties(isProperties), tag(type) {}

		Media::Data::Type	tag;
		const bool			isProperties;
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
			CODEC_MPEG4_2 = 9,
			CODEC_HEVC = 12 // Added codec HEVC (not officialy supported by FLV)
			// FFmpeg fork with hevc support in flv : https://github.com/ksvc/FFmpeg/wiki
		};
		static const char* CodecToString(Codec codec) {
			static const char* Strings[] = { "RAW", "JPEG", "SORENSON", "SCREEN1", "VP6", "VP6_ALPHA", "SCREEN2", "H264", "H263", "MPEG4_2", "", "", "HEVC" };
			return Strings[UInt8(codec)];
		}
		
		enum Frame { // Aligned values with FLV frame type value, and ignore FRAME_GENERATED_KEYFRAME which is redundant with FRAME_KEY for a "key frame" test condition
			FRAME_UNSPECIFIED = 0,
			FRAME_KEY = 1,
			FRAME_INTER = 2, // Used too by H264 for Config sequence
			FRAME_DISPOSABLE_INTER = 3, // just for H263
			FRAME_INFO = 5,
			FRAME_CONFIG = 7
		};
		struct Tag : virtual Object {
			explicit Tag() : frame(FRAME_UNSPECIFIED), compositionOffset(0) {}
			explicit Tag(Media::Video::Codec codec) : codec(codec), frame(FRAME_UNSPECIFIED), compositionOffset(0) {}
			explicit Tag(const Tag& other) { set(other); }

			Tag& set(const Tag& other) {
				codec = other.codec;
				frame = other.frame;
				time = other.time;
				compositionOffset = other.compositionOffset;
				return self;
			}

			UInt32				 time;
			Media::Video::Codec  codec;
			Frame				 frame;
			UInt16				 compositionOffset;
		};
		struct Config : Tag, Packet, virtual Object {
			NULLABLE
			// always time=0 for config save, because will be the first packet given (subscription starts to 0)
			explicit Config() { time = 0; }
			explicit Config(const Tag& tag, const Packet& packet) { time = 0; set(tag, packet); }
			operator bool() const { return frame == FRAME_CONFIG; ; }
			void reset() {
				frame = FRAME_UNSPECIFIED;
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

		Video(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) : Base(TYPE_VIDEO, packet, track), tag(tag) {}
		Media::Video::Tag	tag;
	};
	static const char* CodecToString(Media::Video::Codec codec) { return Video::CodecToString(codec); }

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
			CODEC_MP38K_FLV = 14 // just usefull for FLV!
		};
		static const char* CodecToString(Codec codec) {
			static const char* Strings[] = { "RAW", "ADPCM", "MP3", "PCM_LITTLE", "NELLYMOSER_16K", "NELLYMOSER_8K", "NELLYMOSER", "G711A", "G711U", "UNKNOWN", "AAC", "SPEEX", "UNKNOWN", "UNKNOWN", "MPEG4_2" };
			return Strings[UInt8(codec)];
		}
		struct Tag : virtual Object {
			explicit Tag() : rate(0), channels(0), isConfig(false) {}
			explicit Tag(Media::Audio::Codec codec) : codec(codec), rate(0), channels(0), isConfig(false) {}
			explicit Tag(const Tag& other) { set(other); }
	
			Tag& set(const Tag& other) {
				codec = other.codec;
				isConfig = other.isConfig;
				time = other.time;
				channels = other.channels;
				rate = other.rate;
				return self;
			}

			UInt32				time;
			Media::Audio::Codec codec;
			bool				isConfig;
			UInt8				channels;
			UInt32				rate;
		};
		struct Config : Tag, Packet, virtual Object {
			NULLABLE
			// always time=0 for config save, because will be the first packet given (subscription starts to 0)
			explicit Config() { time = 0;  }
			explicit Config(const Tag& tag, const Packet& packet) { time = 0; set(tag, packet); }
			operator bool() const { return isConfig ? true : false; }
			void reset() {
				isConfig = false;
				Packet::reset();
			}
			Config& set(const Tag& tag, const Packet& packet=Packet::Null()) {
				isConfig = true;
				codec = tag.codec;
				channels = tag.channels;
				rate = tag.rate;
				Packet::set(std::move(packet));
				return *this;
			}
		};

		Audio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) : Base(TYPE_AUDIO, packet, track), tag(tag) {}
		Media::Audio::Tag	tag;
	};
	static const char* CodecToString(Media::Audio::Codec codec) { return Audio::CodecToString(codec); }


	// AUDIO => 10CCCCCC SSSSSSSS RRRRR0IN [NNNNNNNN] TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
	/// C = codec
	/// S = channels
	/// R = rate "index" => 0, 5512, 7350, 8000, 11025, 12000, 16000, 18900, 22050, 24000, 32000, 37800, 44056, 44100, 47250, 48000, 50000, 50400, 64000, 88200, 96000, 176400, 192000, 352800, 2822400, 5644800
	/// I = is config
	/// N = track
	/// T = time
	// VIDEO => 11CCCCCC FFFFF0ON [OOOOOOOO OOOOOOOO] [NNNNNNNN] TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
	/// C = codec
	/// F = frame (0-15)
	/// O = composition offset
	/// N = track
	/// T = time
	// DATA => 0NTTTTTT [NNNNNNNN]
	/// N = track
	/// T = type
	static UInt8		 PackedSize(const Media::Audio::Tag& tag, UInt8 track = 1) { return track==1 ? 7 : 8; }
	static UInt8		 PackedSize(const Media::Video::Tag& tag, UInt8 track = 1) { return tag.compositionOffset ? (track == 1 ? 8 : 9) : (track == 1 ? 6 : 7); }
	static UInt8		 PackedSize(Media::Data::Type type, UInt8 track = 0) { return track ? 2 : 1; }
	static BinaryWriter& Pack(BinaryWriter& writer, const Media::Audio::Tag& tag, UInt8 track = 1);
	static BinaryWriter& Pack(BinaryWriter& writer, const Media::Video::Tag& tag, UInt8 track = 1);
	static BinaryWriter& Pack(BinaryWriter& writer, Media::Data::Type type, UInt8 track = 0);
	static Media::Type	 Unpack(BinaryReader& reader, Media::Audio::Tag& audio, Media::Video::Tag& video, Media::Data::Type& data, UInt8& track);


	static Media::Type TagType(Data::Type) { return TYPE_DATA; }
	static Media::Type TagType(const Audio::Tag& tag) { return TYPE_AUDIO; }
	static Media::Type TagType(const Video::Tag& tag) { return TYPE_VIDEO; }

	struct Properties : Parameters, virtual Object {
		Properties() : _timeProperties(0) {}
		Properties(const Media::Data& data);

		const Packet& operator[](Media::Data::Type type) const;
		const Packet& operator()(Media::Data::Type& type) const;

		const Time& timeProperties() const { return _timeProperties; }

		void setProperties(UInt8 track, Media::Data::Type type, const Packet& packet);

	protected:

		virtual void onParamChange(const std::string& key, const std::string* pValue);
		virtual void onParamClear();

	private:
		mutable std::deque<Packet>	_packets;
		Time						_timeProperties;
	};


	/*!
	To write a media part from source (just a part of one media, so no beginMedia/endMedia and writeProperties) */
	struct Source : virtual Object {
		virtual const std::string&	name() const { static std::string Name("?");  return Name; }

		virtual void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) = 0;
		virtual void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) = 0;
		virtual void writeData(UInt8 track, Media::Data::Type type, const Packet& packet) = 0;
		virtual void setProperties(UInt8 track, Media::Data::Type type, const Packet& packet) = 0;
		virtual void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) = 0;
		virtual void flush() = 0;
		virtual void reset() = 0;

		void setProperties(UInt8 track, const Media::Properties& properties);
		void setProperties(UInt8 track, DataReader& reader);
		void writeMedia(const Media::Base& media);
		void writeMedia(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) { writeAudio(track, tag, packet); }
		void writeMedia(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) { writeVideo(track, tag, packet); }
		void writeMedia(UInt8 track, Media::Data::Type type, const Packet& packet) { writeData(track, type, packet); }

		static Source& Null();
	};

	/*!
	Complete media target, has begin/end and Properties */
	struct Target : virtual Object {
		/*!
		If Target is sending queueable (bufferize), returns queueing size to allow to detect congestion */
		virtual UInt64 queueing() const { return 0; }
		/*!
		Is called one time before beginMedia, and on params change */
		virtual void setMediaParams(const Parameters& parameters) {}
		/*!
		/!\ can be called multiple times (without one call to endMedia) when media change (MBR switch for example) */
		virtual bool beginMedia(const std::string& name);
		virtual bool writeProperties(const Media::Properties& properties) { return true; }
		virtual bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable);
		virtual void endMedia() {}
	
		/*!
		Overload just if target bufferizes data before to send it*/
		virtual void flush() {}

		bool writeMedia(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return writeAudio(track, tag, packet, reliable); }
		bool writeMedia(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return writeVideo(track, tag, packet, reliable); }
		bool writeMedia(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return writeData(track, type, packet, reliable); }

		static Target& Null() { static Target Null; return Null; }
	};
	struct TrackTarget : Target, virtual Object {
		virtual bool writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeData(Media::Data::Type type, const Packet& packet, bool reliable);
	private:
		bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return writeAudio(tag, packet, reliable); }
		bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return writeVideo(tag, packet, reliable); }
		bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return writeData(type, packet, reliable); }
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
		const Path			path;
		const std::string	query;
		const std::string&	description() const { return _description.empty() ? ((Stream*)this)->buildDescription(_description) : _description; }
		
		/*!
		Reader =>  [address] [type/TLS][/MediaFormat] [parameter]
		Writer => @[address:]port...
		Near of SDP syntax => m=audio 58779 [UDP/TLS/]RTP/SAVPF [111 103 104 9 0 8 106 105 13 126] 
		File => @file[.format] [MediaFormat] [parameter] */
		static Stream* New(Exception& ex, const std::string& description, const Timer&	timer, IOFile& ioFile, IOSocket& ioSocket, const shared<TLS>& pTLS);

		/*!
		/!\ Implementation have to support a pulse start! */
		virtual void start() = 0;
		virtual void start(Media::Source& source);
		virtual bool running() const = 0;
		virtual void stop() = 0;
	protected:
		Stream(Type type, const Path& path) : type(type), path(path) {}

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
