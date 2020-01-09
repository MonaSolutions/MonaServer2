/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#include "Mona/CCaption.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {

enum {
	CHARSET_BASIC_AMERICAN = 0,
	CHARSET_SPECIAL_AMERICAN = 1,
	CHARSET_EXTENDED_SPANISH_FRENCH_MISC = 2,
	CHARSET_EXTENDED_PORTUGUESE_GERMAN_DANISH = 3,
	CHARSET_UNDERLINE = 0x0D,
	CHARSET_ITALIC = 0x0E,
	CHARSET_CTRL = 0x0F,
};
enum {
	CTRL_BACKSPACE,
	CTRL_CLEAR,
	CTRL_TEXT,
	CTRL_CAPTION_START,
	CTRL_CAPTION_END
};

static const char* _Charsets[4][128] = {
	{ // CHARSET_BASIC_AMERICAN
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		"\u0020", "\u0021", "\u0022", "\u0023", "\u0024", "\u0025", "\u0026", "\u0027",     //   ! " # $ % & '
		"\u0028", "\u0029",                                         // ( )
		"\u00E1",       // 2A: 225 'á' "Latin small letter A with acute"
		"\u002B", "\u002C", "\u002D", "\u002E", "\u002F",                       //       + , - . /
		"\u0030", "\u0031", "\u0032", "\u0033", "\u0034", "\u0035", "\u0036", "\u0037",     // 0 1 2 3 4 5 6 7
		"\u0038", "\u0039", "\u003A", "\u003B", "\u003C", "\u003D", "\u003E", "\u003F",     // 8 9 : ; < = > ?
		"\u0040", "\u0041", "\u0042", "\u0043", "\u0044", "\u0045", "\u0046", "\u0047",     // @ A B C D E F G
		"\u0048", "\u0049", "\u004A", "\u004B", "\u004C", "\u004D", "\u004E", "\u004F",     // H I J K L M N O
		"\u0050", "\u0051", "\u0052", "\u0053", "\u0054", "\u0055", "\u0056", "\u0057",     // P Q R S T U V W
		"\u0058", "\u0059", "\u005A", "\u005B",                             // X Y Z [
		"\u00E9",       // 5C: 233 'é' "Latin small letter E with acute"
		"\u005D",                                               //           ]
		"\u00ED",       // 5E: 237 'í' "Latin small letter I with acute"
		"\u00F3",       // 5F: 243 'ó' "Latin small letter O with acute"
		"\u00FA",       // 60: 250 'ú' "Latin small letter U with acute"
		"\u0061", "\u0062", "\u0063", "\u0064", "\u0065", "\u0066", "\u0067",           //   a b c d e f g
		"\u0068", "\u0069", "\u006A", "\u006B", "\u006C", "\u006D", "\u006E", "\u006F",     // h i j k l m n o
		"\u0070", "\u0071", "\u0072", "\u0073", "\u0074", "\u0075", "\u0076", "\u0077",     // p q r s t u v w
		"\u0078", "\u0079", "\u007A",                                   // x y z
		"\u00E7",       // 7B: 231 'ç' "Latin small letter C with cedilla"
		"\u00F7",       // 7C: 247 '÷' "Division sign"
		"\u00D1",       // 7D: 209 'Ñ' "Latin capital letter N with tilde"
		"\u00F1",       // 7E: 241 'ñ' "Latin small letter N with tilde"
		u8"\u25A0"    // 7F:         "Black Square" (NB: 2588 = Full Block)
	},
	{ // CHARSET_SPECIAL_AMERICAN
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		"\u00AE",    // 30: 174 '®' "Registered Sign" - registered trademark symbol
		"\u00B0",    // 31: 176 '°' "Degree Sign"
		"\u00BD",    // 32: 189 '½' "Vulgar Fraction One Half" (1/2 symbol)
		"\u00BF",    // 33: 191 '¿' "Inverted Question Mark"
		u8"\u2122",  // 34:         "Trade Mark Sign" (tm superscript)
		"\u00A2",    // 35: 162 '¢' "Cent Sign"
		"\u00A3",    // 36: 163 '£' "Pound Sign" - pounds sterling
		u8"\u266A",  // 37:         "Eighth Note" - music note
		"\u00E0",    // 38: 224 'à' "Latin small letter A with grave"
		"\u0020",    // 39:         TRANSPARENT SPACE - for now use ordinary space
		"\u00E8",    // 3A: 232 'è' "Latin small letter E with grave"
		"\u00E2",    // 3B: 226 'â' "Latin small letter A with circumflex"
		"\u00EA",    // 3C: 234 'ê' "Latin small letter E with circumflex"
		"\u00EE",    // 3D: 238 'î' "Latin small letter I with circumflex"
		"\u00F4",    // 3E: 244 'ô' "Latin small letter O with circumflex"
		"\u00FB",     // 3F: 251 'û' "Latin small letter U with circumflex"
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{ // CHARSET_EXTENDED_SPANISH_FRENCH_MISC
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		"\u00C1", "\u00C9", "\u00D3", "\u00DA", "\u00DC", "\u00FC", "\u2018", "\u00A1",
		"\u002A", "\u0027", u8"\u2014", "\u00A9", u8"\u2120", u8"\u2022", u8"\u201C", u8"\u201D",
		"\u00C0", "\u00C2", "\u00C7", "\u00C8", "\u00CA", "\u00CB", "\u00EB", "\u00CE",
		"\u00CF", "\u00EF", "\u00D4", "\u00D9", "\u00F9", "\u00DB", "\u00AB", "\u00BB",
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{ // CHARSET_EXTENDED_PORTUGUESE_GERMAN_DANISH
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		"\u00C3", "\u00E3", "\u00CD", "\u00CC", "\u00EC", "\u00D2", "\u00F2", "\u00D5",
		"\u00F5", "\u007B", "\u007D", "\u005C", "\u005E", "\u005F", "\u007C", "\u007E",
		"\u00C4", "\u00E4", "\u00D6", "\u00F6", "\u00DF", "\u00A5", "\u00A4", u8"\u2502",
		"\u00C5", "\u00E5", "\u00D8", "\u00F8", u8"\u250C", u8"\u2510", u8"\u2514", u8"\u2518",
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	}
};

CCaption::CCaption() {
	memset(_langs, 0, 8);
	UInt8 id(0);
	for (Channel& channel : _channels) {
		for (Track& track : channel.tracks)
			track.channel = ++id;
	}
}

UInt32 CCaption::extract(const Media::Video::Tag& tag, const Packet& packet, const CCaption::OnText& onText, const CCaption::OnLang& onLang) {
	// flush what is possible now (tag.time has certainly progressed)
	for (Channel& channel : _channels)
		channel.flush(onText, &tag);

	// Search SEI frame to parse!
	BinaryReader reader(packet.data(), packet.size());
	UInt32 offset = 0;
	switch (tag.codec) {
		case Media::Video::CODEC_H264: {	
			while (reader.available()>=4) {
				BinaryReader cc(reader.current()+4, reader.available()-4);
				cc.shrink(reader.next(reader.read32()));
				if ((cc.read8() & 0x1f)!=6 || cc.read8() != 4)
					continue;
				// SEI - user data registered
				UInt32 size = 0;
				while ((size += cc.read8()) == 255);
				cc.shrink(size);
				if(cc.read8()==0xFF)
					cc.read8(); // country code extension
				cc.next(2); // skip terminate provider code
				if (cc.available() < 10 || memcmp(cc.current(), EXPAND("GA94")) != 0)
					continue;
				// Caption closed!
				cc.next(4); // GA94
				if (cc.read8() != 3)
					continue;
				UInt8 value = cc.read8();
				if (value & 0x40) {
					offset = reader.position();
					cc.read8(); // reserved
					value = (value & 0x1F) * 3;
					while (cc.available() < value)
						value -= 3;
					decode(tag, Packet(packet, cc.current(), value), onLang);
				}
			}
			break;
		}
		default:;
	}
	return offset;
}

void CCaption::decode(const Media::Video::Tag& tag, const Packet& packet, const OnLang& onLang) {
	Packet content(packet);
	while (content.size() >= 3) {
		UInt8 type = content.data()[0];
		UInt8 hi = content.data()[1];
		UInt8 lo = content.data()[2];
		content += 3;
		if (!type && isalpha(hi) && isalpha(lo)) {
			type = (hi & 0x80) << 1 | (lo & 0x80);
			// custom lang info!
			Track& track = _channels[type >> 1].tracks[type & 1];
			hi = tolower(hi);
			lo = tolower(lo);
			if(_langs[type][0] != hi || _langs[type][1] != lo) {
				_langs[type][0] = hi;
				_langs[type][1] = lo;
				onLang(track.channel, hi=='x' && lo=='x' ? NULL : _langs[type]);
			}
			continue;
		}
		if ((type&=3) > 1) // process just EIA608 info
			continue;

		hi &= 0x7F;
		lo &= 0x7F;

		// EIA608
		if (!hi)
			continue; // ignore empty data

		// DEBUG(tag.time + tag.compositionOffset, " => ", String::Hex(&hi, 1), '(', (char)hi, ')', " ", String::Hex(&lo, 1), '(', (char)lo, ')', " ")

		Channel& channel = _channels[type];

		if (hi >= 0x20) {
			// Basic North American character set.
			channel.write(tag, hi, lo);
			continue;
		}

		if (hi < 0x10) {
			// eXtended Data Service, TODO? (missing file sample!)
			// http://www.theneitherworld.com/mcpoodle/SCC_TOOLS/DOCS/CC_XDS.HTML
			// DEBUG(tag.time + tag.compositionOffset, " => ", String::Hex(&hi, 1), '(', (char)hi, ')', " ", String::Hex(&lo, 1), '(', (char)lo, ')', " ")
			continue; 
		}

		UInt8 track = (hi&8) ? 0x10 : 0;

		if (lo & 0x40) {
			// Row Preamble Address and Style
			channel.write(tag, track | CHARSET_UNDERLINE, lo & 1);
			if (!(lo & 0x10)) // Row Preamble Extended Style
				channel.write(tag, track | CHARSET_ITALIC, (lo & 0x0E) == 0x0E ? 1 : 0);
			continue;
		}

		switch (hi &= 0xF7) { // the 5h bit is "channel toggle"
			case 0x10: // background color, volontary not supported (skinnable on client side)
				channel.write(tag, track, 0); // just for track toggle!
				continue;
			case 0x11:
				switch (lo & 0xF0) {
					case 0x30: // P|0|1|1|X|X|X|X
							   // Special North American character set.
						channel.write(tag, track | CHARSET_SPECIAL_AMERICAN, lo & 0x0F);
						continue;
					case 0x20: // P|0|1|0|X|X|X|X
							   // Mid row style change, mirow style
						channel.write(tag, track | CHARSET_ITALIC, (lo & 0x0E) == 0x0E ? 1 : 0);
						channel.write(tag, track | CHARSET_UNDERLINE, lo & 1);
						continue;
					default:;
				}
				break;
			case 0x12:
			case 0x13:
				// Extended Spanish/Miscellaneous and French character set.
				// ccData2 - P|0|1|X|X|X|X|X
				// OR
				// Extended Portuguese and German/Danish character set.
				// ccData2 - P|0|1|X|X|X|X|X
				if ((lo & 0xE0) == 0x20) {
					channel.write(tag, track | ((hi % 2) ? CHARSET_EXTENDED_PORTUGUESE_GERMAN_DANISH : CHARSET_EXTENDED_SPANISH_FRENCH_MISC), lo & 0x1F);
					continue;
				}
				break;
			case 0x14:
			case 0x15: {
				// commands
				switch (lo) {
					case 0x20: // resume caption loading
						channel.write(tag, track | CHARSET_CTRL, CTRL_CAPTION_START);
						continue;
					case 0x21: // backspace
						channel.write(tag, track | CHARSET_CTRL, CTRL_BACKSPACE);
						continue;
					case 0x22: // alarm off, unused
					case 0x23: // alarm on, unused
					case 0x24: // delete to end of row, nothing todo (unsupported for this simplified design)
					case 0x2C: // erase display memory (clear screen, unsupported for this simplified design because already sent!)
						channel.write(tag, track, 0); // just for track toggle!
						continue;
					case 0x25: // roll up 2
					case 0x26: // roll up 3
					case 0x27: // roll up 4
					case 0x29: // resume direct captionning
					case 0x2F: // end of caption
						channel.write(tag, track | CHARSET_CTRL, CTRL_CAPTION_END);
						continue;
					case 0x2A: // start non-caption text
					case 0x2B: // resume non-caption text
						channel.write(tag, track | CHARSET_CTRL, CTRL_TEXT);
						continue;
					case 0x2D: // carriage return, erase current line!
					case 0x2E: // erase non-displayed memory (clear buffer)
						channel.write(tag, track | CHARSET_CTRL, CTRL_CLEAR);
						continue;
					default:;
				}
				break;
			}
			case 0x17:
				if (lo >= 0x21 && lo <= 0x23) {
					// taboffset, useless
					channel.write(tag, track, 0); // just for track toggle!
					continue;
				}
				if ((lo & 0xFE) == 0x2E) { // Mid Row Style Change - black text => P|0|1|0|1|1|1|U
					channel.write(tag, track | CHARSET_UNDERLINE, lo & 1);
					continue;
				}
			default:;
		}
		channel.write(tag, track, 0); // just for track toggle!
		WARN("Unknown cc set ", String::Format<UInt8>("%.2x", hi), "-", String::Format<UInt8>("%.2x", lo));
	}
}

void CCaption::flush(const CCaption::OnText& onText) {
	for (Channel& channel : _channels)
		channel.flush(onText, NULL);
	memset(_langs,0, 8);
}

void CCaption::Channel::flush(const CCaption::OnText& onText, const Media::Video::Tag* pTag) {
	// flush all is inferior or equal to tag.time
	multimap<UInt32, CC>::iterator it = _ccs.begin();
	while (it != _ccs.end() && (!pTag || it->first<= pTag->time)) {
		CC& cc((it++)->second);
		if (cc.hi >= 0x20) {
			if (!_pTrack)
				continue; // trunked caption data, wait at less a control with track information
			_pTrack->append(_Charsets[CHARSET_BASIC_AMERICAN][cc.hi]);
			if (cc.lo)
				_pTrack->append(_Charsets[CHARSET_BASIC_AMERICAN][cc.lo]);
			continue;
		}
		Track& track = (cc.hi & 0x10) ? tracks[1] : tracks[0];
		_pTrack = &track;
		switch (cc.hi &= 0x0F) {
			case CHARSET_CTRL:
				switch (cc.lo) {
					case CTRL_BACKSPACE:
						if (track&FLAG_SKIP)
							continue;
						track.erase(1);
						continue;
					case CTRL_CLEAR:
						if (track&FLAG_SKIP)
							continue;
						track.erase();
						continue;
					case CTRL_CAPTION_START:
						track &= ~FLAG_SKIP;
						if (track && track->data()[track->size() - 1] != '\n')
							track->append(EXPAND("\n"));
						continue;
					case CTRL_CAPTION_END:
						track &= ~FLAG_SKIP;
						break;
					case CTRL_TEXT:
						track |= FLAG_SKIP;
						break;
				}
				// end of row => clear style and return line
				track.flush(onText);
				break;
			case CHARSET_UNDERLINE:
				if (track&FLAG_SKIP)
					break;
				if (cc.lo) {
					if (!(track&FLAG_UNDERLINE)) {
						if (track)
							track->append(EXPAND("<u>"));
						track |= FLAG_UNDERLINE;
					}
				} else if (track&FLAG_UNDERLINE) {
					if (track)
						track->append(EXPAND("</u>"));
					track &= ~FLAG_UNDERLINE;
				}
				break;
			case CHARSET_ITALIC:
				if (track&FLAG_SKIP)
					break;
				if (cc.lo) {
					if (!(track&FLAG_ITALIC)) {
						if (track) {
							if(isspace(track->data()[track->size() - 1]))
								track->append(EXPAND("<i>"));
							else
								track->append(EXPAND("<i> ")); // space after <i> to be trimright if buffer flushed without new write
						}
						track |= FLAG_ITALIC;
					}
				} else if (track&FLAG_ITALIC) {
					if (track) {
						if (isspace(track->data()[track->size() - 1]))
							track->append(EXPAND("</i>"));
						else
							track->append(EXPAND("</i> "));
					}
					track &= ~FLAG_ITALIC;
				}
				break;
			default: // cc.hi = CHARSET_..., cc.lo is the index in charset
				if (cc.hi) // can't be CHARSET_BASIC_AMERICAN here, if hi was 0 the message was just a channel switch!
					track.append(_Charsets[cc.hi][cc.lo]);
		}
	}
	_ccs.erase(_ccs.begin(), it);
	if (pTag)
		return;
	// is reseting!
	for (Track& track : tracks)
		track.flush(onText) = nullptr;
	_pTrack = NULL;
}


CCaption::Track& CCaption::Track::append(const char* text) {
	if (!text || _flags&FLAG_SKIP)
		return *this;
	size_t size = strlen(text);
	if (!size)
		return *this;
	if (!self) {
		String::TrimLeft(text, size);
		if (!size)
			return *this;
		BUFFER_RESET(_pBuffer,0);
		if (_flags & FLAG_UNDERLINE)
			_pBuffer->append(EXPAND("<u>"));
		if (_flags & FLAG_ITALIC)
			_pBuffer->append(EXPAND("<i>"));
	}
	_pBuffer->append(text, size);
	return *this;
}

CCaption::Track& CCaption::Track::erase(UInt32 count) {
	if (_pBuffer)
		_pBuffer->resize(min(count, _pBuffer->size()), true);
	return self;
}

CCaption::Track& CCaption::Track::flush(const CCaption::OnText& onText) {
	// end of row => clear style and flush buffer
	if (_pBuffer)
		_pBuffer->resize(String::TrimRight(STR _pBuffer->data(), _pBuffer->size()));
	if (_flags&FLAG_UNDERLINE) {
		if (_pBuffer)
			_pBuffer->append(EXPAND("</u>"));
		_flags &= ~FLAG_UNDERLINE;
	}
	if (_flags&FLAG_ITALIC) {
		if (_pBuffer)
			_pBuffer->append(EXPAND("</i>"));
		_flags &= ~FLAG_ITALIC;
	}
	if(onText && self)
		onText(channel, _pBuffer);
	return self;
}


} // namespace Mona
