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
#include "Mona/MediaReader.h"
#include "Mona/MediaWriter.h"

namespace Mona {


struct MediaFile : virtual Static  {

	struct Reader : Media::Stream, virtual Object {
		Reader(const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const Timer& timer, IOFile& io);
		virtual ~Reader() { stop(); }

		static unique<MediaFile::Reader> New(const Path& path, Media::Source& source, const char* subMime, const Timer& timer, IOFile& io);
		static unique<MediaFile::Reader> New(const Path& path, Media::Source& source, const Timer& timer, IOFile& io) { return New(path, source, path.extension().c_str(), timer, io); }

		bool running() const { return _pFile.operator bool(); }

		IOFile&			io;
		const Timer&	timer;

	private:
		void starting(const Parameters& parameters);
		void stopping();

		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream source file://...", MAKE_FOLDER(Path(path.parent()).name()), path.name(), '|', _pReader->format()); }

		struct Lost : Media::Base, virtual Object {
			Lost(Media::Type type, UInt32 lost, UInt8 track) : Media::Base(type, Packet::Null(), track), _lost(lost) {} // lost
			operator const UInt32&() { return _lost; }
		private:
			UInt32 _lost;
		};
		struct Decoder : File::Decoder, private Media::Source, virtual Object {
			typedef Event<void()>	ON(Flush);

			Decoder(const Handler& handler, const shared<MediaReader>& pReader, const Path& path, const std::string& name, const shared<std::deque<unique<Media::Base>>>& pMedias) :
				_name(name), _handler(handler), _pReader(pReader), _path(path), _pMedias(pMedias) {}

		private:
			UInt32 decode(shared<Buffer>& pBuffer, bool end);

			void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { if(!_mediaTimeGotten) _mediaTimeGotten = !tag.isConfig; writeMedia<Media::Audio>(tag, packet, track); }
			void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { if (!_mediaTimeGotten) _mediaTimeGotten = tag.frame != Media::Video::FRAME_CONFIG; writeMedia<Media::Video>(tag, packet, track); }
			void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) { writeMedia<Media::Data>(type, packet, track); }
			void setProperties(Media::Data::Type type, const Packet& packet, UInt8 track = 1) { writeMedia<Media::Data>(type, packet, track, true); }
			void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { writeMedia<Lost>(type, lost, track); }
			void flush() { } // do nothing on source flush, to do the flush in end of file reading!
			void reset() { _pMedias->emplace_back(); }

			template <typename MediaType, typename ...Args>
			void writeMedia(Args&&... args) {
				_pMedias->emplace_back();
				_pMedias->back().set<MediaType>(std::forward<Args>(args)...);
			}
			
			shared<MediaReader>		_pReader;
			std::string				_name;
			const Handler&			_handler;
			Path					_path;
			bool					_mediaTimeGotten;
			
			shared<std::deque<unique<Media::Base>>> _pMedias;
		};

		Decoder::OnFlush		_onFlush;

		File::OnError			_onFileError;

		shared<MediaReader>		_pReader;
		shared<File>			_pFile;
		shared<Decoder>			_pDecoder;

		Timer::OnTimer			_onTimer;
		Time					_realTime;
		shared<std::deque<unique<Media::Base>>> _pMedias;
	};



	struct Writer : Media::Target, Media::Stream, virtual Object {
		Writer(const Path& path, unique<MediaWriter>&& pWriter, IOFile& io);
		virtual ~Writer() { stop(); }

		static unique<MediaFile::Writer> New(const Path& path, const char* subMime, IOFile& io);
		static unique<MediaFile::Writer> New(const Path& path, IOFile& io) { return New(path, path.extension().c_str(), io); }

		bool running() const { return _running; }
	
		IOFile&		io;
		UInt64		queueing() const { return _pFile ? _pFile->queueing() : 0; }
	
		void setMediaParams(const Parameters& parameters);
		bool beginMedia(const std::string& name);
		bool writeProperties(const Media::Properties& properties) { Media::Data::Type type; const Packet& packet(properties(type)); return write<MediaWrite<Media::Data>>(0, type, packet); }
		bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Audio>>(track, tag, packet); }
		bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Video>>(track, tag, packet); }
		bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Data>>(track, type, packet); }
		void endMedia();

	private:
		void starting(const Parameters& parameters);
		void stopping();

		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream target file://...", MAKE_FOLDER(Path(path.parent()).name()), path.name(), '|', _pWriter->format()); }

		template<typename WriteType, typename ...Args>
		bool write(Args&&... args) {
			if (!_pFile)
				return false; // Stream not begin or has failed!
			io.threadPool.queue<WriteType>(_writeTrack, _pName, io, _pFile, _pWriter, std::forward<Args>(args)...);
			return true;
		}

		struct Write : Runner, virtual Object {
			Write(const shared<std::string>& pName, IOFile& io, const shared<File>& pFile, const shared<MediaWriter>& pWriter);
		protected:
			MediaWriter::OnWrite	onWrite;
			shared<MediaWriter>		pWriter;
		private:
			virtual bool run(Exception& ex) { pWriter->beginMedia(onWrite); return true; }

			shared<File>		_pFile;
			shared<std::string>	_pName;
			IOFile&				_io;
		};

		template<typename MediaType>
		struct MediaWrite : Write, MediaType, virtual Object {
			MediaWrite(const shared<std::string>& pName, IOFile& io, const shared<File>& pFile, const shared<MediaWriter>& pWriter,
				UInt8 track, const typename MediaType::Tag& tag, const Packet& packet) : Write(pName, io, pFile, pWriter), MediaType(tag, packet, track) {}
			bool run(Exception& ex) { pWriter->writeMedia(*this, onWrite); return true; }
		};
		struct EndWrite : Write, virtual Object {
			EndWrite(const shared<std::string>& pName, IOFile& io, const shared<File>& pFile, const shared<MediaWriter>& pWriter) : Write(pName, io, pFile, pWriter) {}
			bool run(Exception& ex) { pWriter->endMedia(onWrite); return true; }
		};

		File::OnError			_onError;
		shared<File>			_pFile;
		shared<MediaWriter>		_pWriter;
		UInt16					_writeTrack;
		bool					_running;
		shared<std::string>		_pName;
		bool					_append;
	};
};

} // namespace Mona
