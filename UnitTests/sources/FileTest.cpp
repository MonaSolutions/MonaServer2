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

#include "Test.h"
#include "Mona/FileReader.h"
#include "Mona/FileWriter.h"

using namespace std;
using namespace Mona;

namespace FileTest {

ADD_TEST(File) {

	Exception ex;
	const char* name("temp.mona");

	{
		File file(name, File::MODE_WRITE);
		CHECK(file.write(ex, EXPAND("Salut")) && !ex);
		CHECK(file.written() == 5 && file.size(true) == 5);
	}

	{
		CHECK(File(name, File::MODE_APPEND).write(ex, EXPAND("Salut")) && !ex);
		File file(name, File::MODE_READ);
		CHECK(file.size() == 10);
		char data[10];
		CHECK(file.read(ex, data, 20) == 10 && file.readen() == 10 && !ex && memcmp(data, EXPAND("SalutSalut")) == 0);
		CHECK(!file.write(ex, data, sizeof(data)) && ex && ex.cast<Ex::Permission>());
		ex = nullptr;
	}

	{
		File file(name, File::MODE_WRITE);
		CHECK(file.write(ex, EXPAND("Salut")) && !ex);
		CHECK(file.written() == 5 && file.size(true) == 5);
		char data[10];
		CHECK(file.read(ex, data, sizeof(data)) == -1 && ex && ex.cast<Ex::Permission>());
		ex = nullptr;
	}
}

static struct MainHandler : Handler {
	MainHandler() : Handler(_signal) {}
	bool join(UInt32 count) {
		UInt32 done(0);
		while ((done += Handler::flush()) < count) {
			if (!_signal.wait(14000))
				return false;
		};
		return count == done;
	}

private:
	void flush() {}
	Signal _signal;
} _Handler;
static ThreadPool	_ThreadPool;

ADD_TEST(FileReader) {
	IOFile		io(_Handler, _ThreadPool);

	const char* name("temp.mona");
	Exception ex;

	FileReader reader(io);
	reader.onError = [](const Exception& ex) {
		FATAL_ERROR("FileReader, ", ex);
	};
	reader.onReaden = [&](shared<Buffer>& pBuffer, bool end) {
		if (pBuffer->size() > 3) {
			CHECK(pBuffer->size() == 5 && memcmp(pBuffer->data(), EXPAND("Salut")) == 0 && end);
			reader.close();
			reader.open(name).read(3);
		} else if (pBuffer->size() == 3) {
			CHECK(memcmp(pBuffer->data(), EXPAND("Sal")) == 0 && !end);
			reader.read(3);
		} else {
			CHECK(pBuffer->size() == 2 && memcmp(pBuffer->data(), EXPAND("ut")) == 0 && end);
			reader.close();
		}
	};
	reader.open(name).read();
	CHECK(_Handler.join(3));
}

ADD_TEST(FileWriter) {
	IOFile		io(_Handler, _ThreadPool);
	const char* name("temp.mona");
	Exception ex;
	Packet salut(EXPAND("Salut"));

	FileWriter writer(io);
	writer.onError = [](const Exception& ex) {
		FATAL_ERROR("FileWriter, ", ex);
	};
	bool onFlush = false;
	writer.onFlush = [&onFlush, &writer](bool deletion) {
		onFlush = true;
		CHECK(!deletion);
	};
	writer.open(name).write(salut);
	io.join();
	CHECK(onFlush); // onFlush!
	CHECK(writer->written() == 5 && writer->size(true) == 5);
	writer.write(salut);
	io.join();
	CHECK(writer->written() == 10 && writer->size(true) == 10);
	writer.close();
	
	onFlush = false;
	writer.open(name, true).write(salut);
	io.join();
	CHECK(onFlush); // onFlush!
	CHECK(writer->written()==5 && writer->size(true) == 15);
	writer.close();

	CHECK(_Handler.join(2)); // 2 writing step = 2 onFlush!
	CHECK(FileSystem::Delete(ex, name) && !ex);
}

}
