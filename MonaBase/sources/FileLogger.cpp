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

#include "Mona/FileLogger.h"
#include "Mona/Logs.h"


using namespace std;

namespace Mona {

FileLogger::FileLogger(string&& dir, UInt32 sizeByFile, UInt16 rotation) : _sizeByFile(sizeByFile), _rotation(rotation),
	_pFile(SET, Path(MAKE_FOLDER(dir), "0.log"), File::MODE_APPEND) {
	_written = range<UInt32>(_pFile->size());
}

bool FileLogger::log(LOG_LEVEL level, const Path& file, long line, const string& message) {
	static string Buffer; // max size controlled by Logs system!
	static Exception Ex;
	String::Assign(Buffer, String::Log(Logs::LevelToString(level), file, line, message, Thread::CurrentId()));
	if (!_pFile->write(Ex, Buffer.data(), Buffer.size())) {
		_pFile.reset();
		return false;
	}
	manage(Buffer.size());
	return true;
}

bool FileLogger::dump(const string& header, const UInt8* data, UInt32 size) {
	String buffer(String::Date("%d/%m %H:%M:%S.%c  "), header, '\n');
	Exception ex;
	if (!_pFile->write(ex, buffer.data(), buffer.size()) || !_pFile->write(ex, data, size)) {
		_pFile.reset();
		return false;
	}
	manage(buffer.size() + size);
	return true;
}

void FileLogger::manage(UInt32 written) {
	if (!_sizeByFile || (_written += written) <= _sizeByFile) // don't use _pLogFile->size(true) to avoid disk access on every file log!
		return; // _logSizeByFile==0 => inifinite log file! (user choice..)

	_written = 0;
	_pFile.set(Path(_pFile->parent(), "0.log"), File::MODE_WRITE); // override 0.log file!

	// delete more older file + search the older file name (usefull when _rotation==0 => no rotation!) 
	string name;
	UInt32 maxNum(0);
	Exception ex;
	FileSystem::ForEach forEach([this, &ex, &name, &maxNum](const string& path, UInt16 level) {
		UInt16 num;
		if (!String::ToNumber(FileSystem::GetBaseName(path, name), num))
			return true;
		if (_rotation && num >= (_rotation - 1))
			FileSystem::Delete(ex, String(_pFile->parent(), num, ".log"));
		else if (num > maxNum)
			maxNum = num;
		return true;
	});
	FileSystem::ListFiles(ex, _pFile->parent(), forEach);
	// rename log files
	do {
		FileSystem::Rename(String(_pFile->parent(), maxNum, ".log"), String(_pFile->parent(), maxNum + 1, ".log"));
	} while (maxNum--);

}

} // namespace Mona
