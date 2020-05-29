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
#include "Mona/MediaWriter.h"

namespace Mona {

/*!
MP4Writer writes a progressive fragmented MP4 compatible with VLC, HTML5 and Media Source Extension
/!\ No duration-is-empty implementation because it affects Edge (source invalid)
/!\ No multiple "sample description" because it's not supported by Chrome and Firefox */
struct MP4Writer : MediaWriter, virtual Object {
	// http://xhelmboyx.tripod.com/formats/mp4-layout.txt
	// https://www.w3.org/TR/mse-byte-stream-format-isobmff/
	// http://l.web.umkc.edu/lizhu/teaching/2016sp.video-communication/ref/mp4.pdf
	// https://www.adobe.com/content/dam/Adobe/en/devnet/flv/pdfs/video_file_format_spec_v10.pdf
	// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html
	// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html
	// ffmpeg -re -i Sintel.ts -c copy -listen 1 -f mp4 -frag_duration 100000 -movflags empty_moov+default_base_moof http://127.0.0.1:80/test.mp4

	enum : UInt16 {
		BUFFER_MIN_SIZE		 = 100,
		BUFFER_RESET_SIZE	 = 1000 // wait one second to get at less one video frame the first time (1fps is the min possibe for video)
	};

	MP4Writer(UInt16 bufferTime = BUFFER_RESET_SIZE);

	UInt32 currentTime() const { return _timeFront; }
	UInt32 lastTme() const { return _timeBack; }

	const UInt16 bufferTime;

	void beginMedia(const OnWrite& onWrite);
	void writeProperties(const Media::Properties& properties, const OnWrite& onWrite);
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeData(UInt8 track, Media::Data::Type type, const Packet& packet, const OnWrite& onWrite);
	void endMedia(const OnWrite& onWrite);

private:
	void flush(const OnWrite& onWrite, Int8 reset=0);

	struct Frame : Packet, virtual Object {
		Frame(UInt32 time, const Media::Video::Tag& tag, const Packet& packet) : Packet(std::move(packet)), isSync(tag.frame == Media::Video::FRAME_KEY), time(time), compositionOffset(tag.compositionOffset) {}
		Frame(UInt32 time, const Media::Audio::Tag& tag, const Packet& packet) : Packet(std::move(packet)), isSync(true), time(time), compositionOffset(0) {}
		Frame(UInt32 time, const Packet& packet) : Packet(std::move(packet)), time(time), isSync(true), compositionOffset(0) {}
	
		const bool			isSync;
		const UInt32		time;
		const UInt32		compositionOffset;
	};

	struct Frames : virtual Object, std::deque<Frame> {
		NULLABLE(!_started)

		Frames(UInt32 tagTime) : _started(false), rate(0), lastDuration(0), codec(0),
			lastTime(tagTime), // assign lastTime to tagTime to accept the packet on monotonic control
			hasCompositionOffset(false), hasKey(false), writeConfig(false) {
		}

		UInt8	codec;

		Packet	config;

		bool	writeConfig;
		bool	hasCompositionOffset;
		bool	hasKey;

		UInt32  rate;

		// fix time|duration + tfdt
		UInt32	lastTime;
		UInt32  lastDuration;

		UInt32  sizeTraf() const { return 60 + (size() * (hasCompositionOffset ? (hasKey ? 16 : 12) : (hasKey ? 12 : 8))); }
		Frames& operator=(std::nullptr_t) { _started = false; return self; }

		void push(UInt32 time, const Packet& packet) {
			emplace_back(time, packet);
			_started = true;
		}

		template<typename TagType>
		void push(UInt32 time, const TagType& tag, const Packet& packet) {
			emplace_back(time, tag, packet);
			codec = tag.codec;
			_started = true;
		}
		std::deque<Frame> flush();
	private:
		bool	_started;
	};

	bool	computeSizeMoof(std::deque<Frames>& tracks, Int8 reset, UInt32& sizeMoof);
	void	writeTrack(BinaryWriter& writer, UInt32 track, Frames& frames, UInt32& dataOffset, bool isEnd);
	Int32	writeFrame(BinaryWriter& writer, Frames& frames, UInt32 size, bool isSync, UInt32 duration, UInt32 compositionOffset, Int32 delta);

	std::deque<Frames>			_audios;
	std::deque<Frames>			_videos;
	std::deque<Frames>			_datas;
	UInt32						_sequence;
	UInt32						_timeFront;
	UInt32						_timeBack;
	UInt32						_seekTime;
	bool						_started;
	UInt16						_buffering;
	UInt16						_bufferMinSize;
	UInt8						_errors;
};



} // namespace Mona
