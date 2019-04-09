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

#pragma once

#include "Mona/Mona.h"
#include "Mona/Time.h"
#include "Mona/Timezone.h"
#include "inttypes.h"

namespace Mona {

struct Exception;

struct Date : Time, virtual Object {
	static const char* FORMAT_ISO8601; 				/// 2005-01-01T12:00:00+01:00 | 2005-01-01T11:00:00Z
	static const char* FORMAT_ISO8601_FRAC;			/// 2005-01-01T12:00:00.000000+01:00 | 2005-01-01T11:00:00.000000Z
	static const char* FORMAT_ISO8601_SHORT; 		/// 20050101T120000+01:00 | 20050101T110000Z
	static const char* FORMAT_ISO8601_SHORT_FRAC;	/// 20050101T120000.000000+01:00 | 20050101T110000.000000Z
	static const char* FORMAT_RFC822;				/// Sat, 1 Jan 05 12:00:00 +0100 | Sat, 1 Jan 05 11:00:00 GMT
	static const char* FORMAT_RFC1123;				/// Sat, 1 Jan 2005 12:00:00 +0100 | Sat, 1 Jan 2005 11:00:00 GMT
	static const char* FORMAT_HTTP;					/// Sat, 01 Jan 2005 12:00:00 +0100 | Sat, 01 Jan 2005 11:00:00 GMT
	static const char* FORMAT_RFC850;				/// Saturday, 1-Jan-05 12:00:00 +0100 | Saturday, 1-Jan-05 11:00:00 GMT
	static const char* FORMAT_RFC1036;				/// Saturday, 1 Jan 05 12:00:00 +0100 | Saturday, 1 Jan 05 11:00:00 GMT
	static const char* FORMAT_ASCTIME;				/// Sat Jan  1 12:00:00 2005
	static const char* FORMAT_SORTABLE;				/// 2005-01-01 12:00:00

	static bool  IsLeapYear(Int32 year) { return (year % 400 == 0) || (!(year & 3) && year % 100); }

	// build a NOW date, not initialized (is null)
	// /!\ Keep 'Type' to avoid confusion with "build from time" constructor, if a explicit Int32 offset is to set, use Date::setOffset or "build from time" contructor
	Date(Timezone::Type offset=Timezone::LOCAL) : _isDST(false),_year(0), _month(0), _day(0), _weekDay(7),_hour(0), _minute(0), _second(0), _millisecond(0), _changed(false), _offset((Int32)offset),_isLocal(true) {}
	
	// build from time
	Date(Int64 time, Int32 offset= Timezone::LOCAL) : _isDST(false),_year(0), _month(0), _day(0),  _weekDay(7),_hour(0), _minute(0), _second(0), _millisecond(0), _changed(false), _offset(offset),_isLocal(true), Time(time) {}

	// build from other  date
	explicit Date(const Date& other) : Time((Time&)other), _isDST(other._isDST),_year(other._year), _month(other._month), _day(other._day),  _weekDay(other._weekDay),_hour(other._hour), _minute(other._minute), _second(other._second), _millisecond(other._millisecond), _changed(other._changed), _offset(other._offset),_isLocal(other._isLocal) {
	}
	
	// build from date
	explicit Date(Int32 year, UInt8 month, UInt8 day, Int32 offset= Timezone::LOCAL) : _isDST(false),_year(0), _month(0), _day(0), _weekDay(7),_hour(0), _minute(0), _second(0), _millisecond(0), _changed(true), _offset(0),_isLocal(true), Time(0) {
		update(year,month,day,offset);
	}

	// build from clock
	explicit Date(UInt8 hour, UInt8 minute, UInt8 second, UInt16 millisecond=0) : _isDST(false),_year(0), _month(1), _day(1), _weekDay(4),_hour(0), _minute(0), _second(0), _millisecond(0), _changed(false), _offset(0),_isLocal(false), Time(0) {
		setClock(hour,minute,second,millisecond);
	}

	// build from date+clock
	explicit Date(Int32 year, UInt8 month, UInt8 day, UInt8 hour=0, UInt8 minute=0, UInt8 second=0, UInt16 millisecond=0, Int32 offset= Timezone::LOCAL) : _isDST(false),_year(0), _month(0), _day(0), _weekDay(7),_hour(0), _minute(0), _second(0), _millisecond(0), _changed(true), _offset(0),_isLocal(true), Time(0) {
		update(year,month,day,hour,minute,second,millisecond,offset);
	}

	 // now
	Date& update() { return update(Time::Now()); }
	// /!\ Keep 'Type' to avoid confusion with 'update(Int64 time)'
	Date& update(Timezone::Type offset) { return update(Time::Now(),offset); }

	// from other date
	Date& update(const Date& date);

	// from time
	Date& update(Int64 time) { return update(time, _isLocal ? Int32(Timezone::LOCAL) : _offset); }
	Date& update(Int64 time,Int32 offset);

	// from date
	Date& update(Int32 year, UInt8 month, UInt8 day);
	Date& update(Int32 year, UInt8 month, UInt8 day, Int32 offset) { update(year, month, day); setOffset(offset); return *this; }

	// from date+clock
	Date& update(Int32 year, UInt8 month, UInt8 day, UInt8 hour, UInt8 minute, UInt8 second, UInt16 millisecond = 0);
	Date& update(Int32 year, UInt8 month, UInt8 day, UInt8 hour, UInt8 minute, UInt8 second, UInt16 millisecond, Int32 offset) { update(year, month, day, hour, minute, second,millisecond); setOffset(offset); return *this; }

	/// from string
	bool update(Exception& ex, const char* value, const char* format = NULL) { return update(ex, value, std::string::npos, format); }
	bool update(Exception& ex, const char* value, std::size_t count, const char* format = NULL);
	bool update(Exception& ex, const std::string& value, const char* format = NULL) { return update(ex, value.data(), format); }

	Date& operator=(Int64 time) { update(time); return *this; }
	Date& operator=(const Date& date) { update(date); return *this; }
	Date& operator+= (Int64 time) { update(this->time()+time); return *this; }
	Date& operator-= (Int64 time) { update(this->time()-time); return *this; }

	// to time
	Int64 time() const;

	/// GETTERS
	// date
	Int32	year() const			{ if (_day == 0) init(); return _year; }
	UInt8	month() const			{ if (_day == 0) init(); return _month; }
	UInt8	day() const				{ if (_day == 0) init(); return _day; }
	UInt8	weekDay() const;
	UInt16	yearDay() const;
	// clock
	UInt32  clock() const			{ if (_day == 0) init(); return _hour*3600000L + _minute*60000L + _second*1000L + _millisecond; }
	UInt8	hour() const			{ if (_day == 0) init(); return _hour; }
	UInt8	minute() const			{ if (_day == 0) init(); return _minute; }
	UInt8	second() const			{ if (_day == 0) init(); return _second; }
	UInt16	millisecond() const		{ if (_day == 0) init(); return _millisecond; }
	// offset
	Int32	offset() const;
	bool	isGMT() const			{ if (_day == 0) init(); return offset()==0 && !_isLocal; }
	bool	isDST() const			{ offset(); /* <= allow to refresh _isDST */ return _isDST; }

	/// SETTERS
	// date
	void	setYear(Int32 year);
	void	setMonth(UInt8 month);
	void	setDay(UInt8 day);
	void	setWeekDay(UInt8 weekDay);
	void	setYearDay(UInt16 yearDay);

	// clock
	void	setClock(UInt8 hour, UInt8 minute, UInt8 second, UInt16 millisecond=0);
	void	setClock(UInt32 clock);
	void	setHour(UInt8 hour);
	void	setMinute(UInt8 minute);
	void	setSecond(UInt8 second);
	void	setMillisecond(UInt16 millisecond);
	// offset
	void	setOffset(Int32 offset);


	template<typename OutType>
	OutType& format(const char* format, OutType& out) const {
		if (_day == 0)
			init();

		char buffer[32];
		UInt32 formatSize = strlen(format);
		UInt32 iFormat(0);

		while (iFormat < formatSize) {
			char c(format[iFormat++]);
			if (c != '%') {
				if (c != '[' && c != ']')
					out.append(&c, 1);
				continue;
			}

			if (iFormat == formatSize)
				break;

			switch (c = format[iFormat++]) {
				case 'w': out.append(_WeekDayNames[weekDay()], 3); break;
				case 'W': { const char* day(_WeekDayNames[weekDay()]); out.append(day, strlen(day)); break; }
				case 'b': out.append(_MonthNames[_month - 1], 3); break;
				case 'B': { const char* month(_MonthNames[_month - 1]); out.append(month, strlen(month)); break; }
				case 'd': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _day)); break;
				case 'e': out.append(buffer, snprintf(buffer, sizeof(buffer), "%u", _day)); break;
				case 'f': out.append(buffer, snprintf(buffer, sizeof(buffer), "%2d", _day)); break;
				case 'm': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _month)); break;
				case 'n': out.append(buffer, snprintf(buffer, sizeof(buffer), "%u", _month)); break;
				case 'o': out.append(buffer, snprintf(buffer, sizeof(buffer), "%2d", _month)); break;
				case 'y': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _year % 100)); break;
				case 'Y': out.append(buffer, snprintf(buffer, sizeof(buffer), "%04d", _year)); break;
				case 'H': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _hour)); break;
				case 'h': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", (_hour<1 ? 12 : (_hour>12 ? (_hour - 12) : _hour)))); break;
				case 'a': out.append((_hour < 12) ? "am" : "pm", 2); break;
				case 'A': out.append((_hour < 12) ? "AM" : "PM", 2); break;
				case 'M': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _minute)); break;
				case 'S': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _second)); break;
				case 's': out.append(buffer, snprintf(buffer, sizeof(buffer), "%02d", _second));
					out.append(EXPAND("."));
				case 'F':
				case 'i': out.append(buffer, snprintf(buffer, sizeof(buffer), "%03d", _millisecond)); break;
				case 'c': out.append(buffer, snprintf(buffer, sizeof(buffer), "%u", _millisecond / 100)); break;
				case 'z': Timezone::Format(isGMT() ? Timezone::GMT : offset(), out); break;
				case 'Z': Timezone::Format(isGMT() ? Timezone::GMT : offset(), out, false); break;
				case 't':
				case 'T': {
					if (iFormat == formatSize)
						break;
					UInt32 factor(1);
					switch (tolower(format[iFormat++])) {
						case 'h':
							factor = 3600000;
							break;
						case 'm':
							factor = 60000;
							break;
						case 's':
							factor = 1000;
							break;
					}
					out.append(buffer, snprintf(buffer, sizeof(buffer), "%02" PRIu64, UInt64(time() / factor)));
					break;
				}
				default: out.append(&c, 1);
			}
		}
		return out;
	}

private:
	void  init() const { _day = 1; ((Date*)this)->update(Time::time(), _offset); }
	void  computeWeekDay(Int64 days);
	bool  parseAuto(Exception& ex, const char* data, std::size_t count);

	Int32			_year;
	UInt8			_month; // 1 to 12
	mutable UInt8	_day; // 1 to 31
	mutable UInt8	_weekDay; // 0 to 6 (sunday=0, monday=1) + 7 (unknown)
	UInt8			_hour; // 0 to 23
	UInt8			_minute;  // 0 to 59
	UInt8			_second;	// 0 to 59
	UInt16			_millisecond; // 0 to 999
	mutable Int32	_offset; // gmt offset
	mutable bool	_isDST; // means that the offset is a Daylight Saving Time offset
	mutable bool	_isLocal; // just used when offset is on the special Local value!

	mutable bool	_changed; // indicate that date information has changed, we have to refresh time value

	static const char*    _WeekDayNames[];
	static const char*    _MonthNames[];
};


} // namespace Mona

