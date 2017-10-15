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

#include "Mona/PersistentData.h"
#include "Mona/FileSystem.h"
#include "Mona/Util.h"
#include <fstream>
#include <openssl/evp.h>


using namespace std;

namespace Mona {

void PersistentData::load(Exception& ex, const string& rootDir, const ForEach& forEach, bool disableTransaction) {
	flush();
	_disableTransaction = disableTransaction;
	FileSystem::MakeFile(_rootPath=rootDir);
	loadDirectory(ex,_rootPath , "", forEach);
	_disableTransaction = false;
}

bool PersistentData::run(Exception& ex, const volatile bool& stopping) {

	for (;;) {

		bool timeout(!wakeUp.wait(60000)); // 1 min timeout

		deque<shared<Entry>> entries;
		{	// stop() must be encapsulated by _mutex!
			std::lock_guard<std::mutex> lock(_mutex);
			if (_entries.empty()) {
				if (timeout)
					stop();
				if (stopping)
					return true;
				continue;
			}
			entries = move(_entries);
		}
		for (shared<Entry>& pEntry : entries) {
			processEntry(ex, *pEntry);
			if (ex) {
				// stop to display the error!
				stop();
				return false;
			}
		}
	}
}


void PersistentData::processEntry(Exception& ex,Entry& entry) {
	string directory(_rootPath);

	// Security => formalize entry.path to avoid possible /../.. issue
	if (entry.path.empty() || (entry.path.front() != '/' && entry.path.front() != '\\'))
		entry.path.insert(0, "/");

	FileSystem::MakeFolder(directory.append(FileSystem::Resolve(entry.path)));
	string name,file;

	if (entry) {

		// add entry
		if (!FileSystem::CreateDirectory(ex, directory, FileSystem::MODE_HEAVY)) {
			ex.set<Ex::System::File>("Impossible to create database directory ", directory);
			return;
		}

		// compute md5
		UInt8 result[16];
		EVP_Digest(entry.data(), entry.size(), result, NULL, EVP_md5(), NULL);

		String::Assign(name, String::Hex(result, sizeof(result)));

		// write the file
		file.assign(directory).append(name);
#if defined(_WIN32)
		wchar_t wFile[PATH_MAX];
		MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, wFile, sizeof(wFile));
		ofstream ofile(wFile, ios::out | ios::binary);
#else
		ofstream ofile(file.c_str(), ios::out | ios::binary);
#endif
		if (!ofile.good()) {
			ex.set<Ex::System::File>("Impossible to write file ", file);
			return;
		}
		ofile.write(STR entry.data(), entry.size());
	}

	// remove possible old value after writing to be safe!
	bool hasData=false;
	FileSystem::ForEach forEach([&ex, name, &file, &hasData](const string& path, UInt16 level){
		// Delete just hex file
		if (FileSystem::IsFolder(path) || FileSystem::GetName(path, file).size() != 32 || file == name) {
			hasData = true;
			return;
		}
		// is MD5?
		for (char c : file) {
			if (!isxdigit(c) || (c > '9' && isupper(c))) {
				hasData = true;
				return;
			}
		}
		if (!FileSystem::Delete(ex, path))
			hasData = true;
	});
	Exception ignore;
	// if directory doesn't exists it's already deleted so ignore ListFiles exception
	FileSystem::ListFiles(ignore, directory, forEach);
	// if remove and no sub value, erase the folder
	if (!entry && !hasData)
		FileSystem::Delete(ex,directory);
}


bool PersistentData::loadDirectory(Exception& ex, const string& directory, const string& path, const ForEach& forEach) {
	
	bool hasData = false;

	FileSystem::ForEach forEachFile([&ex, this, &hasData, path, &forEach](const string& file, UInt16 level) {
		/// directory

		string name, value;
		FileSystem::GetName(file, name);
		if (FileSystem::IsFolder(file)) {
			if (loadDirectory(ex, file, String::Assign(value, path, '/', name), forEach))
				hasData = true;
			return;
		}
	
		/// file
		if (name.size() != 32) {// ignore this file (not create by Database)
			hasData = true;
			return;
		}

		// read the file
		#if defined(_WIN32)
			wchar_t wFile[PATH_MAX];
			MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, wFile, sizeof(wFile));
			ifstream ifile(wFile, ios::in | ios::binary | ios::ate);
		#else
			ifstream ifile(file, ios::in | ios::binary | ios::ate);
		#endif
		if (!ifile.good()) {
			ex.set<Ex::System::File>("Impossible to read file ", file);
			return;
		}
		UInt32 size = (UInt32)ifile.tellg();
		ifile.seekg(0);
		vector<char> buffer(size);
		if (size>0)
			ifile.read(buffer.data(), size);
		// compute md5
		UInt8 result[16];
		EVP_Digest(buffer.data(), size, result, NULL, EVP_md5(), NULL);
		String::Assign(value, String::Hex(result, sizeof(result)));  // HEX lower case
		// compare with file name
		if (value != name) {
			// erase this data! (bad serialization)
			// is MD5?
			for (char c : name) {
				if (!isxdigit(c) || (c > '9' && isupper(c))) {
					hasData = true;
					return;
				}
			}
			if(!FileSystem::Delete(ex, file))
				hasData = true;
			return;
		}

		hasData = true;
		forEach(path, (const UInt8*)buffer.data(), buffer.size());
	});

	Exception ignore;
	FileSystem::ListFiles(ignore, directory, forEachFile);
	return hasData || !FileSystem::Delete(ex,directory);
}


} // namespace Mona

