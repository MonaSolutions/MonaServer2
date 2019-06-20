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


#pragma once

#include "Mona/Mona.h"
#include "Mona/Exceptions.h"
#include <vector>


namespace Mona {


class WinRegistryKey : public virtual Object {
public:
	typedef std::vector<std::string> Keys;
	typedef std::vector<std::string> Values;
	
	enum Type {
		REGT_NONE = 0, 
		REGT_STRING = 1, 
		REGT_STRING_EXPAND = 2, 
		REGT_DWORD = 4
	};

	WinRegistryKey(const std::string& key, bool readOnly = false, REGSAM extraSam = 0);
		/// Creates the WinRegistryKey.
		///
		/// The key must start with one of the root key names
		/// like HKEY_CLASSES_ROOT, e.g. HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services.
		///
		/// If readOnly is true, then only read access to the registry
		/// is available and any attempt to write to the registry will
		/// result in an exception.
		///
		/// extraSam is used to pass extra flags (in addition to KEY_READ and KEY_WRITE)
		/// to the samDesired argument of RegOpenKeyEx() or RegCreateKeyEx().

	WinRegistryKey(HKEY hRootKey, const std::string& subKey, bool readOnly = false, REGSAM extraSam = 0);
	/// Creates the WinRegistryKey.
	///
	/// If readOnly is true, then only read access to the registry
	/// is available and any attempt to write to the registry will
	/// result in an exception.
	///
	/// extraSam is used to pass extra flags (in addition to KEY_READ and KEY_WRITE)
	/// to the samDesired argument of RegOpenKeyEx() or RegCreateKeyEx().

	virtual ~WinRegistryKey() { close(); }
		/// Destroys the WinRegistryKey.

	bool setString(Exception& ex,const std::string& name, const std::string& value);
		/// Sets the string value (REG_SZ) with the given name.
		/// An empty name denotes the default value.
		
	const std::string& getString(Exception& ex, const std::string& name,std::string& value);
		/// Returns the string value (REG_SZ) with the given name.
		/// An empty name denotes the default value.
		///
        /// Throws an exception if the value does not exist.

	bool setStringExpand(Exception& ex,const std::string& name, const std::string& value);
		/// Sets the expandable string value (REG_EXPAND_SZ) with the given name.
		/// An empty name denotes the default value.
		
	const std::string& getStringExpand(Exception& ex, const std::string& name, std::string& value);
		/// Returns the string value (REG_EXPAND_SZ) with the given name.
		/// An empty name denotes the default value.
		/// All references to environment variables (%VAR%) in the string
		/// are expanded.
		///
        /// Throws an exception if the value does not exist.

	bool setInt(Exception& ex, const std::string& name, int value);
		/// Sets the numeric (REG_DWORD) value with the given name.
		/// An empty name denotes the default value.
		
	int getInt(Exception& ex, const std::string& name);
		/// Returns the numeric value (REG_DWORD) with the given name.
		/// An empty name denotes the default value.
		///
        /// Throws an exception if the value does not exist.

	void deleteValue(const std::string& name);
		/// Deletes the value with the given name.
		///
        /// Throws an exception if the value does not exist.

	void deleteKey();
		/// Recursively deletes the key and all subkeys.

	bool exists();
		/// Returns true iff the key exists.

	Type type(Exception& ex, const std::string& name);
		/// Returns the type of the key value.
		
	bool exists(const std::string& name);
		/// Returns true iff the given value exists under that key.

	void subKeys(Keys& keys);
		/// Appends all subKey names to keys.

	void values(Values& vals);
		/// Appends all value names to vals;
		
	bool isReadOnly() const { return _readOnly; }
		/// Returns true iff the key has been opened for read-only access only.

protected:
	bool open(Exception& ex);
	void close();

private:
	HKEY        _hRootKey;
	const char* _subKey;
	std::string _key;
	HKEY        _hKey;
	bool        _readOnly;
	REGSAM      _extraSam;
};

} // namespace Mona
