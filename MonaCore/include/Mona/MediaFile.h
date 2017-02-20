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
#include "Mona/FileReader.h"
#include "Mona/FileWriter.h"
#include "Mona/Logs.h"

namespace Mona {


struct MediaFile : virtual Static {

	struct Reader : Media::Stream, virtual Object {
		Reader(const Path& path, MediaReader* pReader, IOFile& io);
		virtual ~Reader() { stop(); }

		void start();
		void start(Media::Source& source) { _pSource = &source; return start(); }
		bool running() const { return _pFile.operator bool(); }
		void stop();

		const Path					path;
		IOFile&						io;

	private:
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream source file://...", MAKE_FOLDER(Path(path.parent()).name()), path.name(), '|', _pReader->format()); }

		struct Lost : Media::Base, virtual Object {
			Lost(Media::Type type, UInt32 lost) : Media::Base(type, 0, Packet::Null()), _lost(-Int32(lost)) {} // lost
			Lost(Media::Type type, UInt16 track, UInt32 lost) : Media::Base(type, track, Packet::Null()), _lost(lost) {} // lost
			void report(Media::Source& source) { _lost < 0 ? source.reportLost(type, -_lost) : source.reportLost(type, track, _lost); }
		private:
			Int32 _lost;
		};
		struct Decoder : File::Decoder, private Media::Source, virtual Object {
			typedef Event<void(Media::Base&)>		ON(Media);
			typedef Event<void(Lost& lost)>			ON(Lost);
			typedef Event<void()>					ON(Flush);
			typedef Event<void()>					ON(Reset);
			typedef Event<void()>					ON(End);

			Decoder(const Handler& handler, const shared<MediaReader>& pReader, const Path& path, const std::string& name) :
				_name(name), _handler(handler), _pReader(pReader), _path(path) {}
			~Decoder() { _pReader->flush(*this); }

		private:
			UInt32 decode(shared<Buffer>& pBuffer, bool end);

			void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) { _handler.queue<Media::Audio>(onMedia, track, tag, packet); }
			void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) { _handler.queue<Media::Video>(onMedia, track, tag, packet); }
			void writeData(UInt16 track, Media::Data::Type type, const Packet& packet) { _handler.queue<Media::Data>(onMedia, track, type, packet); }
			void writeProperties(UInt16 track, DataReader& reader) { _handler.queue<Media::Data>(onMedia, track, reader); }
			void reportLost(Media::Type type, UInt32 lost) { _handler.queue(onLost, type, lost); }
			void reportLost(Media::Type type, UInt16 track, UInt32 lost) { _handler.queue(onLost, type, track, lost); }
			void flush() { _handler.queue(onFlush); }
			void reset() { _handler.queue(onReset); }

			shared<MediaReader>		_pReader;
			std::string				_name;
			const Handler&			_handler;
			Path					_path;
		};

		Decoder::OnEnd			_onEnd;
		Decoder::OnLost			_onLost;
		Decoder::OnFlush		_onFlush;
		Decoder::OnReset		_onReset;
		Decoder::OnMedia		_onMedia;

		File::OnError			_onFileError;

		shared<MediaReader>		_pReader;
		Media::Source*			_pSource;
		shared<File>			_pFile;
		shared<Decoder>			_pDecoder;
	};



	struct Writer : Media::Target, Media::Stream, virtual Object {
		Writer(const Path& path, MediaWriter* pWriter, IOFile& io);
		virtual ~Writer() { stop(); }

		void start();
		bool running() const { return _running; }
		void stop();

		IOFile&		io;
		const Path	path;
		UInt64		queueing() const { return _pFile ? _pFile->queueing() : 0; }
	
		bool beginMedia(const std::string& name, const Parameters& parameters);
		bool writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Audio>>(track, tag, packet); }
		bool writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Video>>(track, tag, packet); }
		bool writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Data>>(track, type, packet); }
		void endMedia(const std::string& name);

	private:
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream target file://...", MAKE_FOLDER(Path(path.parent()).name()), path.name(), '|', _pWriter->format()); }

		template<typename WriteType, typename ...Args>
		bool write(Args&&... args) {
			if (!_running)
				return false; // Stream not started!
			Exception ex;
			bool success;
			AUTO_ERROR(success = io.threadPool.queue(ex, std::make_shared<WriteType>(_pName, io, _pFile, _pWriter, args ...), _writeTrack), description());
			if (success)
				return true;
			Stream::stop(ex);
			return false;
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
				   UInt16 track, const typename MediaType::Tag& tag, const Packet& packet) : Write(pName, io, pFile, pWriter), MediaType(track, tag, packet) {}
			bool run(Exception& ex) { pWriter->writeMedia(MediaType::track, MediaType::tag, *this, onWrite); return true; }
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
	};
};

} // namespace Mona
