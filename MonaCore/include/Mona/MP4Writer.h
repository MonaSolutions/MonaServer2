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

	MP4Writer(UInt16 bufferTime=1000) : bufferTime(bufferTime) {}

	const UInt16 bufferTime;

	void beginMedia(const OnWrite& onWrite);
	void writeProperties(const Media::Properties& properties, const OnWrite& onWrite);
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void endMedia(const OnWrite& onWrite);

private:
	void flush(const OnWrite& onWrite, Int8 reset=0);

	struct Frame : virtual Object {
		NULLABLE
		
		operator bool() const { return _pMedia ? true : false; }

		Frame(const Media::Video::Tag& tag, const Packet& packet) : isSync(tag.frame == Media::Video::FRAME_KEY) { _pMedia.set<Media::Video>(tag, packet); }
		Frame(const Media::Audio::Tag& tag, const Packet& packet) : isSync(true) { _pMedia.set<Media::Audio>(tag, packet); }
		Frame(const Packet& packet) : isSync(false) { _pMedia.set(Media::TYPE_NONE, packet); }

		const bool			isSync;
		const Media::Base*	operator->() const { return _pMedia.get(); }
		const Media::Base&	operator*() const { return *_pMedia; }
	private:
		unique<Media::Base> _pMedia;
		
	};

	struct Frames : virtual Object, std::deque<Frame> {
		NULLABLE

		Frames() : _sizeTraf(0), _started(false), rate(0), lastTime(0), lastDuration(0), codec(0),
			hasCompositionOffset(false), hasKey(false), writeConfig(false) {}

		UInt8	codec;

		Packet	config;

		bool	writeConfig;
		bool	hasCompositionOffset;
		bool	hasKey;

		UInt32  sizeTraf() const { return 60 + (size() * (hasCompositionOffset ? (hasKey ? 16 : 12) : (hasKey ? 12 : 8))); }
		
		UInt32  rate;

		// fix time|duration + tfdt
		UInt32	lastTime;
		UInt32  lastDuration;

		operator bool() const { return _started; }
		Frames& operator=(std::nullptr_t);

		template<typename TagType>
		void push(const TagType& tag, const Packet& packet) {
			emplace_back(tag, packet);
			if (!codec) // necessary first usage, tag.codec>0 by control in entry from writeAudio/writeVideo
				lastTime = front()->time(); // delta = lastDuration - (front().time() - lastTime) = 0
			codec = tag.codec;
			_started = true;
		}
		std::deque<Frame> flush();
	private:
		bool	_started;
		UInt32  _sizeTraf;
	};

	void	writeTrack(BinaryWriter& writer, UInt32 track, Frames& frames, UInt32& dataOffset);
	Int32	writeFrame(BinaryWriter& writer, Frames& frames, UInt32 size, bool isSync, UInt32 duration, UInt32 compositionOffset, Int32 delta);

	std::deque<Frames>			_audios;
	std::deque<Frames>			_videos;
	UInt32						_sequence;
	UInt32						_timeFront;
	UInt32						_timeBack;
	bool						_started;
	bool						_buffering;
	UInt8						_errors;
};



} // namespace Mona
