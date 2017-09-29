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
/!\ No duration-is-empty implementation because it affects Edge (source invalid)
/!\ No multiple "sample description" because it's not supported by Chrome and Firefox */
struct MP4Writer : MediaWriter, virtual Object {
	// http://xhelmboyx.tripod.com/formats/mp4-layout.txt
	// https://www.w3.org/TR/mse-byte-stream-format-isobmff/
	// http://l.web.umkc.edu/lizhu/teaching/2016sp.video-communication/ref/mp4.pdf
	// https://www.adobe.com/content/dam/Adobe/en/devnet/flv/pdfs/video_file_format_spec_v10.pdf
	// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html
	// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html

	void beginMedia(const OnWrite& onWrite);
	void writeProperties(const Media::Properties& properties, const OnWrite& onWrite);
	void writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, const OnWrite& onWrite);
	void endMedia(const OnWrite& onWrite);

private:
	void flush(const OnWrite& onWrite);

	struct Frame : virtual Object {
		Frame(const Media::Video::Tag& tag, const Packet& packet) : _pMedia(new Media::Video(tag, packet)) {}
		Frame(const Media::Audio::Tag& tag, const Packet& packet) : _pMedia(new Media::Audio(tag, packet)) {}
		Frame(Frame&& frame) : config(std::move(config)), _pMedia(std::move(frame._pMedia)) {}
		Frame& operator=(Frame&& frame) { config = std::move(frame.config); _pMedia = std::move(frame._pMedia); return *this; }

		Packet config;
		const Packet& data() const { return *_pMedia; }
		UInt32 size() const { return config.size() + _pMedia->size(); }
		UInt32 time() const { return _pMedia->time(); }
		UInt32 compositionOffset() const { return _pMedia->compositionOffset(); }
	
	private:
		unique<Media::Base> _pMedia;
		
	};

	struct Frames : virtual Object, std::deque<Frame> {
		Frames(Media::Audio::Codec codec) : hasCompositionOffset(false), _firstRate(0), type(Media::TYPE_AUDIO), codec(codec) {}
		Frames(Media::Video::Codec codec) : hasCompositionOffset(false), _firstRate(0), type(Media::TYPE_VIDEO), codec(codec) {}
		
		const Media::Type	type;
		const UInt8			codec;

		Packet	config;
		bool	hasCompositionOffset;
		UInt32  sizeTraf;
		UInt32  firstRate() const { return _firstRate; }
	
		void	emplace_back(const Media::Audio::Tag& tag, const Packet& packet);
		void	emplace_back(const Media::Video::Tag& tag, const Packet& packet);
		void	clear();
	private:
		UInt32 _firstRate;
	};

	void writeTrack(UInt32 track, Frames& frames, UInt32& dataOffset, Buffer& buffer);

	std::deque<Frames>			_audios; // Frames[0] = AAC, Frames[1] == MP3
	std::deque<Frames>			_videos;
	UInt32						_sequence;
	UInt32						_timeFront;
	UInt32						_timeBack;
	bool						_firstTime;

	bool						_audioOutOfRange;
	bool						_videoOutOfRange;
	UInt8						_errors;

	Media::Audio::Tag			_audioSilence;
};



} // namespace Mona
