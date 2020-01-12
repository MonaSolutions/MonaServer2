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
#include "Mona/File.h"
#include OpenSSL(evp.h)


using namespace std;

namespace Mona {

void PersistentData::load(Exception& ex, const string& rootDir, const ForEach& forEach, bool disableTransaction) {
	flush();
	_disableTransaction = disableTransaction;
	FileSystem::MakeFile(_rootPath=rootDir);
	loadDirectory(ex,_rootPath , "", forEach);
	_disableTransaction = false;
}

bool PersistentData::run(Exception& ex, const volatile bool& requestStop) {

	for (;;) {
		bool timeout = !wakeUp.wait(60000); // 1 min timeout
		for (;;) {
			deque<shared<Entry>> entries;
			{	// stop() must be encapsulated by _mutex!
				std::lock_guard<std::mutex> lock(_mutex);
				if (_entries.empty()) {
					if (!timeout && !requestStop)
						break; // wait more
					stop(); // to set _stop immediatly!
					return true;
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
}


void PersistentData::processEntry(Exception& ex,Entry& entry) {
	if (entry.clearing) {
		FileSystem::Delete(ex, _rootPath, FileSystem::MODE_HEAVY);
		return;
	}

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
		File writer(file, File::MODE_WRITE);
		if (!writer.load(ex))
			return;
		writer.write(ex, entry.data(), entry.size());
	}

	// remove possible old value after writing to be safe!
	bool hasData=false;
	FileSystem::ForEach forEach([&ex, name, &file, &hasData](const string& path, UInt16 level){
		// Delete just hex file
		if (FileSystem::IsFolder(path) || FileSystem::GetName(path, file).size() != 32 || file == name) {
			hasData = true;
			return true;
		}
		// is MD5?
		for (char c : file) {
			if (!isxdigit(c) || (c > '9' && isupper(c))) {
				hasData = true;
				return true;
			}
		}
		if (!FileSystem::Delete(ex, path))
			hasData = true;
		return true;
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
			return true;
		}
	
		/// file
		if (name.size() != 32) {// ignore this file (not create by Database)
			hasData = true;
			return true;
		}

		// read the file
		File reader(file, File::MODE_READ);
		if (!reader.load(ex))
			return true;
		shared<Buffer> pBuffer(SET, range<UInt32>(reader.size()));
		if (pBuffer->size() > 0 && reader.read(ex, pBuffer->data(), pBuffer->size()) < 0)
			return true;

		// compute md5
		UInt8 result[16];
		EVP_Digest(pBuffer->data(), pBuffer->size(), result, NULL, EVP_md5(), NULL);
		String::Assign(value, String::Hex(result, sizeof(result)));  // HEX lower case
		// compare with file name
		if (value != name) {
			// erase this data! (bad serialization)
			// is MD5?
			for (char c : name) {
				if (!isxdigit(c) || (c > '9' && isupper(c))) {
					hasData = true;
					return true;
				}
			}
			if(!FileSystem::Delete(ex, file))
				hasData = true;
			return true;
		}

		hasData = true;
		forEach(path, Packet(pBuffer));
		return true;
	});

	Exception ignore;
	FileSystem::ListFiles(ignore, directory, forEachFile);
	return hasData || !FileSystem::Delete(ex,directory);
}


} // namespace Mona

