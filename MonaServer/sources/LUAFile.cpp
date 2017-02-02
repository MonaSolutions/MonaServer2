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

#include "LUAFile.h"

using namespace std;
using namespace Mona;


int	LUAFile::Size(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		SCRIPT_WRITE_INT(file.size(SCRIPT_READ_BOOL(false)))
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::Exists(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		SCRIPT_WRITE_BOOL(file.exists(SCRIPT_READ_BOOL(false)))
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::LastModified(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		SCRIPT_WRITE_NUMBER(file.lastModified(SCRIPT_READ_BOOL(false)))
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::SetPath(lua_State *pState) {
	SCRIPT_CALLBACK(File, file)
		file.setPath(SCRIPT_READ_STRING(file.path().c_str()));
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::SetParent(lua_State *pState) {
	SCRIPT_CALLBACK(File, file)
		file.setParent(SCRIPT_READ_STRING(file.parent().c_str()));
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::SetName(lua_State *pState) {
	SCRIPT_CALLBACK(File, file)
		const char* name(SCRIPT_READ_STRING(NULL));
		SCRIPT_WRITE_BOOL(name && file.setName(name))
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::SetBaseName(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		const char* baseName(SCRIPT_READ_STRING(NULL));
		SCRIPT_WRITE_BOOL(baseName && file.setBaseName(baseName))
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::SetExtension(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		const char* extension(SCRIPT_READ_STRING(NULL));
		SCRIPT_WRITE_BOOL(extension && file.setExtension(extension))
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::MakeFolder(lua_State *pState) {
	SCRIPT_CALLBACK(File, file)
		file.makeFolder();
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::MakeFile(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		file.makeFile();
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::MakeAbsolute(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		file.makeAbsolute();
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::MakeRelative(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		file.makeRelative();
	SCRIPT_CALLBACK_RETURN
}

int	LUAFile::Resolve(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		file.resolve();
	SCRIPT_CALLBACK_RETURN
}

int LUAFile::Index(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		const char* name = SCRIPT_READ_STRING(NULL);
		
		if (name) {
			if(strcmp(name,"size")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::Size)
			} else if(strcmp(name,"lastModified")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::LastModified)
			} else if(strcmp(name,"exists")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::Size)
			} else if(strcmp(name,"name")==0) {
				SCRIPT_WRITE_STRING(file.name().c_str())
			} else if(strcmp(name,"baseName")==0) {
				SCRIPT_WRITE_STRING(file.baseName().c_str())
			} else if(strcmp(name,"parent")==0) {
				SCRIPT_WRITE_STRING(file.parent().c_str())
			} else if(strcmp(name,"extension")==0) {
				SCRIPT_WRITE_STRING(file.extension().c_str())
			} else if (strcmp(name,"isFolder")==0) {
				SCRIPT_WRITE_BOOL(file.isFolder())
			} else if (strcmp(name,"isAbsolute")==0) {
				SCRIPT_WRITE_BOOL(file.isAbsolute())
			} else if (strcmp(name,"path")==0) {
				SCRIPT_WRITE_STRING(file.path().c_str());
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAFile::IndexConst(lua_State *pState) {
	SCRIPT_CALLBACK(File,file)
		const char* name = SCRIPT_READ_STRING(NULL);
		
		if (name) {
			if(strcmp(name,"setPath")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::SetPath)
			} else if(strcmp(name,"setParent")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::SetParent)
			} else if(strcmp(name,"setName")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::SetName)
			} else if(strcmp(name,"setBaseName")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::SetBaseName)
			} else if(strcmp(name,"setExtension")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::SetExtension)
			} else if(strcmp(name,"makeFolder")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::MakeFolder)
			} else if(strcmp(name,"makeFile")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::MakeFile)
			} else if(strcmp(name,"makeAbsolute")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::MakeAbsolute)
			} else if(strcmp(name,"makeRelative")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::MakeRelative)
			} else if(strcmp(name,"resolve")==0) {
				SCRIPT_WRITE_FUNCTION(LUAFile::Resolve)
			}
		}
	SCRIPT_CALLBACK_RETURN
}
