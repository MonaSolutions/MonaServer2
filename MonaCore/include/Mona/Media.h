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
	enum Type : UInt8 { // Keep UInt8 to allow easly a compression + String::ToNumber usage!
		TYPE_NONE=0,
		TYPE_DATA = 1,
		TYPE_AUDIO = 2, // => 10, to have the first bit to 1 and be compatible with Media::Pack
		TYPE_VIDEO = 3, // => 11, to have the first bit to 1 and be compatible with Media::Pack
		// these values allow to write media type on 2 bits!
	};
	static const char* TypeToString(Type type) {
		static const char* Strings[] = { "none", "data", "audio", "video"};
		if (UInt8(type) >= (sizeof(Strings) / sizeof(Strings[0])))
			return "undefined";
		return Strings[UInt8(type)];
	}

	static UInt16 ComputeTextDuration(UInt32 length) { return (UInt16)min(max(length / 20.0, 3) * 1000, 10000); }
	
	struct Base : Packet, virtual Object {
		Base() : type(TYPE_NONE), track(0) {}
		Base(Media::Type type, const Packet& packet, UInt8 track=0) : type(type), track(track), Packet(std::move(packet)) {}

		bool   hasTime() const { return type>TYPE_DATA && !isConfig(); }
		UInt32 time() const;
		void   setTime(UInt32 time);

		bool   isConfig() const;
		Media::Type type;
		UInt8		track;
	};

	struct Properties;
	struct Data : Base, virtual Object {
		enum Type : UInt8 {  // Keep UInt8 to allow easly a compression + String::ToNumber usage!
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
			static const char* Strings[] = { "Unknown", "AMF", "AMF0", "JSON", "XMLRPC", "QUERY", "Text" };
			if (UInt8(type) >= (sizeof(Strings) / sizeof(Strings[0])))
				return "Unknown";
			return Strings[type];
		}
		static const char* TypeToSubMime(Type type) {
			static const char* SubMimes[] = { "octet-stream", "x-amf", "x-amf", "json", "xml", "x-www-form-urlencoded","text" };
			if (type >= (sizeof(SubMimes) / sizeof(SubMimes[0])))
				return "octet-stream";
			return SubMimes[type];
		}

		template<typename DataType>
		static Type ToType() { return ToType(typeid(DataType)); }
		static Type ToType(DataWriter& writer) { return ToType(typeid(writer)); }
		static Type ToType(DataReader& reader) { return ToType(typeid(reader)); }
		static Type ToType(const std::string& subMime) { return ToType(subMime.c_str()); }
		static Type ToType(const char* subMime);

		
		static unique<DataReader> NewReader(Type type, const Packet& packet);
		template<typename AlternateReader>
		static unique<DataReader> NewReader(Type& type, const Packet& packet) {
			unique<DataReader> pReader = NewReader(type, packet);
			if (pReader)
				return pReader;
			type = Data::TYPE_UNKNOWN;
			return new AlternateReader(packet);
		}
		static unique<DataWriter> NewWriter(Type type, Buffer& buffer);
		template<typename AlternateWriter>
		static unique<DataWriter> NewWriter(Type& type, Buffer& buffer) {
			unique<DataWriter> pWriter = NewWriter(type, buffer);
			if (pWriter)
				return pWriter;
			type = Data::TYPE_UNKNOWN;
			return new AlternateWriter(buffer);
		}

		Data(Media::Data::Type type, const Packet& packet, UInt8 track = 0, bool isProperties=false) : Base(TYPE_DATA, packet, track), isProperties(isProperties), tag(type) {}

		Media::Data::Type	tag;
		const bool			isProperties;
	private:
		static Type ToType(const std::type_info& info);
	};

	struct Video : Base, virtual Object {
		enum Codec : UInt8 {  // Keep UInt8 to allow easly a compression + String::ToNumber usage!
			// Aligned values with FLV codec value
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
			static const char* Strings[] = { "RAW", "JPEG", "SORENSON", "SCREEN1", "VP6", "VP6_ALPHA", "SCREEN2", "H264", "H263", "MPEG4_2", "UNKNOWN", "UNKNOWN", "HEVC" };
			if (UInt8(codec) >= (sizeof(Strings) / sizeof(Strings[0])))
				return "UNKNOWN";
			return Strings[codec];
		}
		
		enum Frame : UInt8 {  // Keep UInt8 to allow easly a compression + String::ToNumber usage!
			// Aligned values with FLV frame type value, and ignore FRAME_GENERATED_KEYFRAME which is redundant with FRAME_KEY for a "key frame" test condition
			FRAME_UNSPECIFIED = 0,
			FRAME_KEY = 1,
			FRAME_INTER = 2, // Used too by H264 for Config sequence
			FRAME_DISPOSABLE_INTER = 3, // just for H263
			FRAME_INFO = 5,
			FRAME_CONFIG = 7
		};
		struct Tag : virtual Object {
			explicit Tag() : frame(FRAME_UNSPECIFIED), compositionOffset(0), time(0) {}
			explicit Tag(Media::Video::Codec codec) : codec(codec), frame(FRAME_UNSPECIFIED), compositionOffset(0), time(0) {}
			explicit Tag(const Tag& other) { set(other); }

			Tag& set(const Tag& other) {
				codec = other.codec;
				frame = other.frame;
				time = other.time;
				compositionOffset = other.compositionOffset;
				return self;
			}
			void reset() {
				frame = FRAME_UNSPECIFIED;
				compositionOffset = 0;
				time = 0;
			}

			UInt32				 time;
			Media::Video::Codec  codec;
			Frame				 frame;
			UInt16				 compositionOffset;
		};
		struct Config : Tag, Packet, virtual Object {
			NULLABLE(!Packet::operator bool())
			explicit Config() { frame = FRAME_CONFIG; }
			explicit Config(const Tag& tag, const Packet& packet) { frame = FRAME_CONFIG; set(tag, packet); }
			void reset() { Packet::reset(); }
			Config& set(const Tag& tag, const Packet& packet) {
				time = 0; // always time=0 for config save, because will be the first packet given (subscription starts to 0)
				codec = tag.codec;
				Packet::set(std::move(packet));
				return self;
			}
		};

		Video(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) : Base(TYPE_VIDEO, packet, track), tag(tag) {}
		Media::Video::Tag	tag;
	};
	static const char* CodecToString(Media::Video::Codec codec) { return Video::CodecToString(codec); }

	struct Audio : Base, virtual Object {
		enum Codec : UInt8 { // Keep UInt8 to allow easly a compression + String::ToNumber usage!
			// Aligned values with FLV codec value
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
			if (UInt8(codec) >= (sizeof(Strings) / sizeof(Strings[0])))
				return "UNKNOWN";
			return Strings[codec];
		}
		struct Tag : virtual Object {
			explicit Tag() : rate(0), channels(0), isConfig(false), time(0) {}
			explicit Tag(Media::Audio::Codec codec) : codec(codec), rate(0), channels(0), isConfig(false), time(0) {}
			explicit Tag(const Tag& other) { set(other); }
	
			Tag& set(const Tag& other) {
				codec = other.codec;
				isConfig = other.isConfig;
				time = other.time;
				channels = other.channels;
				rate = other.rate;
				return self;
			}
			void reset() {
				time = rate = 0;
				channels = 0;
				isConfig = false;
			}

			UInt32				time;
			Media::Audio::Codec codec;
			bool				isConfig;
			UInt8				channels;
			UInt32				rate;
		};
		struct Config : Tag, Packet, virtual Object {
			NULLABLE(time!=0)
			explicit Config() { isConfig = true; time = 1;  }
			explicit Config(const Tag& tag, const Packet& packet) { isConfig = true; set(tag, packet); }

			void reset() {
				time = 1;
				Packet::reset();
			}
			Config& set(const Tag& tag, const Packet& packet=Packet::Null()) {
				time = 0; // always time=0 for config save, because will be the first packet given (subscription starts to 0)
				codec = tag.codec;
				channels = tag.channels;
				rate = tag.rate;
				Packet::set(std::move(packet));
				return self;
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
		Properties() {}
		Properties(UInt8 track, Media::Data::Type type, const Packet& packet) { addProperties(track, type, packet); }
	
		const Packet& data(Media::Data::Type& type) const;

		/*!
		Add properties but without overloading of existing, alone way to clear properties is a explicit "clear" (done by publication/source reset) */
		UInt32		addProperties(UInt8 track, DataReader& reader);
		void		addProperties(UInt8 track, Media::Data::Type type, const Packet& packet);

		void    clearTracks();
		void	clear(UInt8 track);
		void	clear() { Parameters::clear(); }
	protected:
		void onParamInit();
		virtual void onParamChange(const std::string& key, const std::string* pValue);
		virtual void onParamClear();

	private:
		mutable std::deque<Packet>	_packets;
		UInt8						_track;
	};


	/*!
	To write a media part from source (just a part of one media, so no beginMedia/endMedia and writeProperties) */
	struct Source : virtual Object {
		virtual const std::string&	name() const { return typeof(self); }

		virtual void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) = 0;
		virtual void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) = 0;
		virtual void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) = 0;
		virtual void addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) = 0;
		virtual void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) = 0;
		virtual void flush() = 0;
		virtual void reset() = 0;

		
		void reportLost(UInt32 lost) { reportLost(Media::TYPE_NONE, lost); }
		/*!
		Add Properties from one map prefilled, no signature with a "track" or "DataReader" argument to minimize call to addProperties (often used between thread, prefer fill a Media::Properties before!)*/
		void addProperties(const Media::Properties& properties);
		void writeMedia(const Media::Base& media);
		void writeMedia(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet) { writeAudio(tag, packet, track); }
		void writeMedia(UInt8 track, const Media::Video::Tag& tag, const Packet& packet) { writeVideo(tag, packet, track); }
		void writeMedia(UInt8 track, Media::Data::Type type, const Packet& packet) { writeData(type, packet, track); }

		static Source& Null();
	};

	/*!
	Complete media target, has begin/end and Properties */
	struct Target : virtual Object {
		/*!
		If Target is sending queueable (bufferize), returns queueing size to allow to detect congestion */
		virtual UInt64 queueing() const { return 0; }
		/*!
		beginMedia, returns false if target fails
		/!\ can be called multiple times (without one call to endMedia) when media change (MBR switch for example) */
		virtual bool beginMedia(const std::string& name);
		virtual bool writeProperties(const Media::Properties& properties) { return true; }
		virtual bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable);
		virtual bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable);
		/*!
		endMedia, tolerates a this deletion (and subscription deletion), should returns flase in this case (else execute a target.flush) */
		virtual bool endMedia() { return true; }
	
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

};

} // namespace Mona
