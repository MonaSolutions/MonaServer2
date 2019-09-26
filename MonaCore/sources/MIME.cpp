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

#include "Mona/MIME.h"
#include "Mona/String.h"

using namespace std;


namespace Mona {

MIME::Type MIME::Read(const Path& file, const char*& subType) {
	if (file.extension().empty())
		return TYPE_UNKNOWN;
	if (file.isFolder()) {
		// List folder!
		subType = "html; charset=utf-8";
		return TYPE_TEXT;
	}
	static const map<const char*, pair<Type,const char*>, String::IComparator> Types({
		{ "html",{ TYPE_TEXT, "html; charset=utf-8" } },
		{ "htm",{ TYPE_TEXT, "html; charset=utf-8" } },
		{ "txt",{ TYPE_TEXT, "plain; charset=utf-8" } },
		{ "vtt",{ TYPE_TEXT, "vtt; charset=utf-8" } },
		{ "srt",{ TYPE_APPLICATION, "x-subrip; charset=utf-8" } },
		{ "dat",{ TYPE_TEXT, "plain; charset=utf-8" } },
		{ "csv",{ TYPE_TEXT, "csv" } },
		{ "css",{ TYPE_TEXT, "css" } },
		{ "wasm",{ TYPE_APPLICATION, "wasm" } },
		{ "js", { TYPE_APPLICATION, "javascript"} }, // utf8 not required, done in HTML inclusion => <script src="/test.js" charset="utf-8"></script>
		{ "mona", { TYPE_VIDEO, "mona" } },
		{ "flv", { TYPE_VIDEO, "x-flv"} },
		{ "f4v",{ TYPE_VIDEO, "x-f4v" } },
		{ "ts", { TYPE_VIDEO, "mp2t"} },
		{ "mp4",{ TYPE_VIDEO, "mp4" } },
		{ "264", { TYPE_VIDEO, "h264" } },
		{ "265",{ TYPE_VIDEO, "hevc" } },
		{ "mp3",{ TYPE_AUDIO, "mp3" } },
		{ "m3u8",{ TYPE_APPLICATION, "x-mpegURL" } },
		{ "aac",{ TYPE_AUDIO, "aac" } },
		{ "svg", { TYPE_APPLICATION, "svg+xml"} },
		{ "m3u", { TYPE_AUDIO, "m3u"} },
		{ "swf", { TYPE_APPLICATION, "x-shockwave-flash"} },
		{ "jpg", { TYPE_IMAGE, "jpeg"} },
		{ "jpeg", { TYPE_IMAGE, "jpeg"} },
		{ "png", { TYPE_IMAGE, "png"} },
		{ "gif",{  TYPE_IMAGE, "gif"} },
		{ "tif", { TYPE_IMAGE, "tiff"} },
		{ "tiff", { TYPE_IMAGE, "tiff"} },
		{ "bin",{ TYPE_APPLICATION, "octet-stream" } },
		{ "exe",{ TYPE_APPLICATION, "octet-stream" } },
		{ "pdf",{ TYPE_APPLICATION, "pdf" } },
		{ "ogg",{ TYPE_APPLICATION, "ogg" } },
		{ "xml",{ TYPE_APPLICATION, "xml" } },
		{ "zip",{ TYPE_APPLICATION, "zip" } }
	});
	const auto& it = Types.find(file.extension().c_str());
	if (it == Types.end())
		return TYPE_UNKNOWN;
	subType = it->second.second;
	return it->second.first;
}


MIME::Type MIME::Read(const char* value, const char*& subType) {
	// subtype
	const char* comma = strchr(value, ';');
	const char* slash = (const char*)memchr(value, '/', comma ? comma - value : strlen(value));
	if (slash)
		subType = slash + 1;
	else if (comma)
		subType = comma;
	else
		subType = "html; charset=utf-8";

	// type
	if (String::ICompare(value, EXPAND("text"))==0)
		return TYPE_TEXT;
	if (String::ICompare(value, EXPAND("image"))==0)
		return TYPE_IMAGE;
	if (String::ICompare(value, EXPAND("application"))==0)
		return TYPE_APPLICATION;
	if (String::ICompare(value, EXPAND("multipart"))==0)
		return TYPE_MULTIPART;
	if (String::ICompare(value, EXPAND("audio"))==0)
		return TYPE_AUDIO;
	if (String::ICompare(value, EXPAND("video"))==0)
		return TYPE_VIDEO;
	if (String::ICompare(value, EXPAND("message"))==0)
		return TYPE_MODEL;
	if (String::ICompare(value, EXPAND("model"))==0)
		return TYPE_MESSAGE;
	if (String::ICompare(value, EXPAND("example"))==0)
		return TYPE_EXAMPLE;
	return TYPE_TEXT;
}




} // namespace Mona
