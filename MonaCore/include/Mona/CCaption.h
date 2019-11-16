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
#include "Mona/Media.h"

namespace Mona {


// https://android.googlesource.com/platform/packages/apps/TV/+/android-live-tv/usbtuner/src/com/google/android/exoplayer/text/eia608/Eia608Parser.java
// https://en.wikipedia.org/wiki/EIA-608
// https://www.gpo.gov/fdsys/pkg/CFR-2007-title47-vol1/pdf/CFR-2007-title47-vol1-sec15-119.pdf
// test files => http://streams.videolan.org/samples/sub/subcc/

struct CCaption : virtual Object {
	CCaption();

	typedef std::function<void(UInt8 channel, shared<Buffer>& pBuffer)>				OnText;
	/*!
	Channel will of 1 to 4 */
	typedef std::function<void(UInt8 channel, const char* lang)>					OnLang;

	UInt32 extract(const Media::Video::Tag& tag, const Packet& packet, const CCaption::OnText& onText, const CCaption::OnLang& onLang);
	void   flush(const CCaption::OnText& onText = nullptr);

private:
	void   decode(const Media::Video::Tag& tag, const Packet& packet, const CCaption::OnLang& onLang);

	typedef UInt8 FLAGS;
	enum {
		FLAG_ITALIC = 1,
		FLAG_UNDERLINE = 2,
		FLAG_SKIP = 4
	};

	struct CC : virtual Object {
		CC(UInt8 hi, UInt8 lo) : hi(hi), lo(lo) {}
		UInt8   hi;
		UInt8   lo;
	};

	struct Track : virtual Object {
		NULLABLE(!_pBuffer || _pBuffer->empty())

		Track() : _flags(FLAG_SKIP) {} // by default the mode is TEXT (wait first DIRECT or LOAD caption mode!)
	
		UInt8 channel;

		Buffer* operator->() { return _pBuffer.get(); }

		Track& operator=(std::nullptr_t) { _flags = FLAG_SKIP; _pBuffer.reset();  return self; }
		Track& operator=(FLAGS flags) { _flags = flags; return self; }
		Track& operator&=(FLAGS flags) { _flags &= flags; return self; }
		Track& operator|=(FLAGS flags) { _flags |= flags; return self; }
		FLAGS operator&(FLAGS flags) const { return _flags & flags; }
		FLAGS operator|(FLAGS flags) const { return _flags | flags; }
	
		Track& append(const char* text);
		Track& erase(UInt32 count=0);

		Track& flush(const CCaption::OnText& onText);
	private:
		shared<Buffer>  _pBuffer;
		FLAGS			_flags;
	};


	struct Channel : virtual Object {
		Channel() : _pTrack(NULL) {}

		Track  tracks[2];

		void   write(const Media::Video::Tag& tag, UInt8 hi, UInt8 lo) { _ccs.emplace(SET, std::forward_as_tuple(tag.time + tag.compositionOffset), std::forward_as_tuple(hi, lo)); }
		void   flush(const CCaption::OnText& onText, const Media::Video::Tag* pTag);

	private:
		std::multimap<UInt32, CC>	_ccs;
		Track*						_pTrack;
	};


	char			_langs[4][2];
	Channel			_channels[2];
};


} // namespace Mona
