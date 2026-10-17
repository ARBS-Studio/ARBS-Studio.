// =============================================================================
// ARBS Studio - Core : Date.hpp - data kalendarike (Gregoriane), pa varesi
// =============================================================================
#pragma once

#include <string>

namespace ARBS {

struct Date {
	int y = 1970, m = 1, d = 1;

	Date () = default;
	Date (int yy, int mm, int dd) : y (yy), m (mm), d (dd) {}

	// numri i dites nga 1970-01-01 (algoritmi i H. Hinnant)
	long ToDays () const;
	static Date FromDays (long days);

	// 0 = e hene ... 6 = e diel (ISO)
	int  Weekday () const;
	bool IsWeekend () const { return Weekday () >= 5; }

	Date AddDays (long n) const { return FromDays (ToDays () + n); }
	bool IsValid () const;

	std::string ToIso () const;			// 2026-10-17
	std::string ToAlb () const;			// 17.10.2026
	std::string ToAlbLong () const;		// 17 tetor 2026
	static bool Parse (const std::string& text, Date& out);	// pranon YYYY-MM-DD ose DD.MM.YYYY

	bool operator== (const Date& o) const { return y == o.y && m == o.m && d == o.d; }
	bool operator!= (const Date& o) const { return !(*this == o); }
	bool operator<  (const Date& o) const { return ToDays () < o.ToDays (); }
	bool operator<= (const Date& o) const { return ToDays () <= o.ToDays (); }
};

int DaysInMonth (int year, int month);
bool IsLeapYear (int year);

const char* MonthNameAlb (int month);		// "Janar" ...
const char* MonthShortAlb (int month);		// "Jan" ...
const char* WeekdayNameAlb (int weekday);	// "E hene" ...
const char* WeekdayShortAlb (int weekday);	// "He" ...

// Pashket (e diel)
Date CatholicEaster (int year);		// Gregoriane
Date OrthodoxEaster (int year);		// Juliane, e konvertuar ne Gregoriane (1900-2099)

}	// namespace ARBS
