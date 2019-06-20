/*
This code is in part based on code from the POCO C++ Libraries,
licensed under the Boost software license :
https://www.boost.org/LICENSE_1_0.txt

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


#include "Mona/WinRegistryKey.h"


#if !defined(_WIN32_WCE)

using namespace std;


namespace Mona {

namespace {
	class AutoHandle {
	public:
		AutoHandle(HMODULE h) : _h(h) {}
		~AutoHandle() { FreeLibrary(_h); }

		HMODULE handle() { return _h; }
	private:
		HMODULE _h;
	};
}

static const map<string, HKEY> RootKeys({
	{ "HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
	{ "HKEY_CURRENT_USER", HKEY_CURRENT_USER },
	{ "HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE },
	{ "HKEY_USERS", HKEY_USERS },
	{ "HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA },
	{ "HKEY_PERFORMANCE_TEXT", HKEY_PERFORMANCE_TEXT },
	{ "HKEY_PERFORMANCE_NLSTEXT", HKEY_PERFORMANCE_NLSTEXT },
	{ "HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG },
	{ "HKEY_DYN_DATA", HKEY_DYN_DATA },
	{ "HKEY_CURRENT_USER_LOCAL_SETTINGS", HKEY_CURRENT_USER_LOCAL_SETTINGS }
});

WinRegistryKey::WinRegistryKey(const string& key, bool readOnly, REGSAM extraSam) :
	_hKey(0),
	_readOnly(readOnly),
	_extraSam(extraSam) {
		string::size_type pos = key.find('\\');
		if (pos == string::npos) {
			_hRootKey = HKEY_CURRENT_USER;
			String::Assign(_key, "HKEY_CURRENT_USER\\", key);
			_subKey = _key.c_str()+18;
		} else {
			_key = key;
			_subKey = _key.c_str() + pos + 1;
			auto& it = RootKeys.find(_subKey);
			_hRootKey = it == RootKeys.end() ? HKEY_CURRENT_USER : it->second;
		}	
}

WinRegistryKey::WinRegistryKey(HKEY hRootKey, const string& subKey, bool readOnly, REGSAM extraSam) :
	_hRootKey(hRootKey),
	_hKey(0),
	_readOnly(readOnly),
	_extraSam(extraSam) {
	switch ((ULONG_PTR)hRootKey) {
		case ULONG_PTR(HKEY_CLASSES_ROOT):
			_key = "HKEY_CLASSES_ROOT\\";
			break;
		case ULONG_PTR(HKEY_CURRENT_USER):
			_key = "HKEY_CURRENT_USER\\";
			break;
		case ULONG_PTR(HKEY_LOCAL_MACHINE):
			_key = "HKEY_LOCAL_MACHINE\\";
			break;
		case ULONG_PTR(HKEY_USERS):
			_key = "HKEY_USERS\\";
			break;
		case ULONG_PTR(HKEY_PERFORMANCE_DATA):
			_key = "HKEY_PERFORMANCE_DATA\\";
			break;
		case ULONG_PTR(HKEY_PERFORMANCE_TEXT):
			_key = "HKEY_PERFORMANCE_TEXT\\";
			break;
		case ULONG_PTR(HKEY_PERFORMANCE_NLSTEXT):
			_key = "HKEY_PERFORMANCE_NLSTEXT\\";
			break;
		case ULONG_PTR(HKEY_CURRENT_CONFIG):
			_key = "HKEY_CURRENT_CONFIG\\";
			break;
		case ULONG_PTR(HKEY_DYN_DATA):
			_key = "HKEY_DYN_DATA\\";
			break;
		case ULONG_PTR(HKEY_CURRENT_USER_LOCAL_SETTINGS):
			_key = "HKEY_CURRENT_USER_LOCAL_SETTINGS\\";
			break;
		default:
			_key = "(UNKNOWN)\\";
	}
	_key.append(subKey);
	_subKey = _key.c_str() + _key.size() - subKey.size();
}


bool WinRegistryKey::setString(Exception& ex, const string& name, const string& value) {
	if (!open(ex))
		return false;
	if (RegSetValueExA(_hKey, name.c_str(), 0, REG_SZ, (CONST BYTE*) value.c_str(), (DWORD) value.size() + 1) == ERROR_SUCCESS)
		return true;
	ex.set<Ex::System::Registry>("Failed to set registry value ", _key, '\\', name);
	return false;
}


const string& WinRegistryKey::getString(Exception& ex,const string& name, string& value) {
	if (!open(ex))
		return value;
	DWORD type;
	DWORD size;
	if (RegQueryValueExA(_hKey, name.c_str(), NULL, &type, NULL, &size) != ERROR_SUCCESS || type != REG_SZ && type != REG_EXPAND_SZ) {
		ex.set<Ex::System::Registry>("Key ", _key, '\\', name, " not found");
		return value;
	}
	value.resize(size);
	if (size)
		RegQueryValueExA(_hKey, name.c_str(), NULL, NULL, (BYTE*)value.data(), &size);
	return value;
}


bool WinRegistryKey::setStringExpand(Exception& ex,const string& name, const string& value) {
	if (!open(ex))
		return false;
	if (RegSetValueExA(_hKey, name.c_str(), 0, REG_EXPAND_SZ, (CONST BYTE*) value.c_str(), (DWORD) value.size() + 1) == ERROR_SUCCESS)
		return true;
	ex.set<Ex::System::Registry>("Failed to set registry value ", _key, '\\', name);
	return false;
}


const string& WinRegistryKey::getStringExpand(Exception& ex, const string& name, string& value) {
	if (!open(ex))
		return value;
	DWORD type;
	DWORD size;
	if (RegQueryValueExA(_hKey, name.c_str(), NULL, &type, NULL, &size) != ERROR_SUCCESS || type != REG_SZ && type != REG_EXPAND_SZ) {
		ex.set<Ex::System::Registry>("Key ", _key, '\\', name, " not found");
		return value;
	}
	value.clear();
	if (size) {
		char* buffer = new char[size + 1];
		RegQueryValueExA(_hKey, name.c_str(), NULL, NULL, (BYTE*) buffer, &size);
		buffer[size] = 0;
		char temp;
		DWORD expSize = ExpandEnvironmentStringsA(buffer, &temp, 1);
		value.resize(expSize);
		ExpandEnvironmentStringsA(buffer, (LPSTR)value.data(), value.size());
		delete [] buffer;
	}
	return value;
}


bool WinRegistryKey::setInt(Exception& ex,const string& name, int value) {
	if (!open(ex))
		return false;
	DWORD data = value;
	if (RegSetValueExA(_hKey, name.c_str(), 0, REG_DWORD, (CONST BYTE*) &data, sizeof(data)) == ERROR_SUCCESS)
		return true;
	ex.set<Ex::System::Registry>("Failed to set registry value ", _key, '\\', name);
	return false;
}


int WinRegistryKey::getInt(Exception& ex,const string& name) {
	if (!open(ex))
		return 0;
	int value(0);
	DWORD type;
	DWORD size = sizeof(value);
	if (RegQueryValueExA(_hKey, name.c_str(), NULL, &type, (BYTE*)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
		return value;
	ex.set<Ex::System::Registry>("Key ", _key, '\\', name, " not found");
	return 0;
}


void WinRegistryKey::deleteValue(const string& name) {
	if (!_hKey)
		return;
	RegDeleteValueA(_hKey, name.c_str());
}


void WinRegistryKey::deleteKey() {
	Keys keys;
	subKeys(keys);
	close();
	for (Keys::iterator it = keys.begin(); it != keys.end(); ++it) {
		string subKey(_subKey);
		subKey += "\\";
		subKey += *it;
		WinRegistryKey(_hRootKey, subKey).deleteKey();
	}

	// NOTE: RegDeleteKeyEx is only available on Windows XP 64-bit SP3, Windows Vista or later.
	// We cannot call it directly as this would prevent the code running on Windows XP 32-bit.
	// Therefore, if we need to call RegDeleteKeyEx (_extraSam != 0) we need to check for
	// its existence in ADVAPI32.DLL and call it indirectly.
	typedef LONG (WINAPI *RegDeleteKeyExAFunc)(HKEY hKey, const char* lpSubKey, REGSAM samDesired, DWORD Reserved);
	if (_extraSam != 0) {
		AutoHandle advAPI32(LoadLibraryA("ADVAPI32.DLL"));
		if (advAPI32.handle()) {
			RegDeleteKeyExAFunc pRegDeleteKeyExA = reinterpret_cast<RegDeleteKeyExAFunc>(GetProcAddress(advAPI32.handle() , "RegDeleteKeyExA"));
			if (pRegDeleteKeyExA) {
				(*pRegDeleteKeyExA)(_hRootKey, _subKey, _extraSam, 0);
				return;
			}
		}
	}
	RegDeleteKey(_hRootKey, _subKey);
}


bool WinRegistryKey::exists() {
	HKEY hKey;
	if (RegOpenKeyExA(_hRootKey, _subKey, 0, KEY_READ | _extraSam, &hKey) == ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return true;
	}
	return false;
}


WinRegistryKey::Type WinRegistryKey::type(Exception& ex,const string& name) {
	if (!open(ex))
		return REGT_NONE;
	DWORD type = REG_NONE;
	DWORD size;
	if (RegQueryValueExA(_hKey, name.c_str(), NULL, &type, NULL, &size) != ERROR_SUCCESS) {
		ex.set<Ex::System::Registry>("Key ", _key, '\\', name, " not found");
		return REGT_NONE;
	}
	if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_DWORD) {
		ex.set<Ex::System::Registry>(_key, '\\', name, ", type not supported");
		return REGT_NONE;
	}
	return (Type)type;
}


bool WinRegistryKey::exists(const string& name) {
	bool exists = false;
	HKEY hKey;
	if (RegOpenKeyExA(_hRootKey, _subKey, 0, KEY_READ | _extraSam, &hKey) == ERROR_SUCCESS) {
		exists = RegQueryValueExA(hKey, name.c_str(), NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
		RegCloseKey(hKey);
	}
	return exists;
}


bool WinRegistryKey::open(Exception& ex) {
	if (_hKey)
		return true;
	if (_readOnly) {
		if (RegOpenKeyExA(_hRootKey, _subKey, 0, KEY_READ | _extraSam, &_hKey) == ERROR_SUCCESS)
			return true;
	} else {
		if (RegCreateKeyExA(_hRootKey, _subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE | _extraSam, NULL, &_hKey, NULL) == ERROR_SUCCESS)
			return true;
	}
	ex.set<Ex::System::Registry>("Cannot open registry ", _key);
	return false;
}


void WinRegistryKey::close() {
	if (_hKey) {
		RegCloseKey(_hKey);
		_hKey = 0;
	}
}

void WinRegistryKey::subKeys(WinRegistryKey::Keys& keys) {
	Exception ex; // ignore here
	if (!open(ex))
		return;

	DWORD subKeyCount = 0;
	DWORD valueCount = 0;

	if (RegQueryInfoKey(_hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
		return;
	char buf[256];
	DWORD bufSize = sizeof(buf);
	for (DWORD i = 0; i< subKeyCount; ++i)
	{
		if (RegEnumKeyExA(_hKey, i, buf, &bufSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			keys.emplace_back(buf);
		bufSize = sizeof(buf);
	}
}


void WinRegistryKey::values(WinRegistryKey::Values& vals) {
	Exception ex; // ignore here
	if (!open(ex))
		return;

	DWORD valueCount = 0;

	if (RegQueryInfoKey(_hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
		return;
	char buf[256];
	DWORD bufSize = sizeof(buf);
	for (DWORD i = 0; i< valueCount; ++i)
	{
		if (RegEnumValueA(_hKey, i, buf, &bufSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			vals.emplace_back(buf);
		bufSize = sizeof(buf);
	}
}


} // namespace Mona


#endif // !defined(_WIN32_WCE)
