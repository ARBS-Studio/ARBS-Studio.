// =============================================================================
// ARBS Studio - Core : Date.cpp
// =============================================================================
#include "Date.hpp"

#include <cstdio>
#include <cstdlib>

namespace ARBS {

bool IsLeapYear (int year)
{
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth (int year, int month)
{
	static const int dim[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (month < 1 || month > 12) return 0;
	if (month == 2 && IsLeapYear (year)) return 29;
	return dim[month - 1];
}

long Date::ToDays () const
{
	int yy = y - (m <= 2 ? 1 : 0);
	const long era = (yy >= 0 ? yy : yy - 399) / 400;
	const unsigned yoe = static_cast<unsigned> (yy - era * 400);
	const unsigned doy = (153u * static_cast<unsigned> (m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned> (d) - 1u;
	const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
	return era * 146097L + static_cast<long> (doe) - 719468L;
}

Date Date::FromDays (long z)
{
	z += 719468;
	const long era = (z >= 0 ? z : z - 146096) / 146097;
	const unsigned doe = static_cast<unsigned> (z - era * 146097);
	const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const long yy = static_cast<long> (yoe) + era * 400;
	const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const unsigned mp = (5 * doy + 2) / 153;
	const unsigned dd = doy - (153 * mp + 2) / 5 + 1;
	const unsigned mm = mp < 10 ? mp + 3 : mp - 9;
	return Date (static_cast<int> (yy + (mm <= 2 ? 1 : 0)), static_cast<int> (mm), static_cast<int> (dd));
}

int Date::Weekday () const
{
	long days = ToDays ();				// 1970-01-01 ishte e enjte (3)
	long w = (days + 3) % 7;
	if (w < 0) w += 7;
	return static_cast<int> (w);
}

bool Date::IsValid () const
{
	return y > 1800 && y < 2200 && m >= 1 && m <= 12 && d >= 1 && d <= DaysInMonth (y, m);
}

std::string Date::ToIso () const
{
	char buf[16];
	std::snprintf (buf, sizeof (buf), "%04d-%02d-%02d", y, m, d);
	return buf;
}

std::string Date::ToAlb () const
{
	char buf[16];
	std::snprintf (buf, sizeof (buf), "%02d.%02d.%04d", d, m, y);
	return buf;
}

std::string Date::ToAlbLong () const
{
	std::string month = MonthNameAlb (m);
	if (!month.empty ()) month[0] = static_cast<char> (month[0] - 'A' + 'a');
	return std::to_string (d) + " " + month + " " + std::to_string (y);
}

bool Date::Parse (const std::string& text, Date& out)
{
	int a = 0, b = 0, c = 0;
	Date r;
	if (std::sscanf (text.c_str (), "%d-%d-%d", &a, &b, &c) == 3 && a > 1000)
		r = Date (a, b, c);
	else if (std::sscanf (text.c_str (), "%d.%d.%d", &a, &b, &c) == 3 && c > 1000)
		r = Date (c, b, a);
	else
		return false;
	if (!r.IsValid ()) return false;
	out = r;
	return true;
}

const char* MonthNameAlb (int month)
{
	static const char* names[] = { "Janar", "Shkurt", "Mars", "Prill", "Maj", "Qershor",
								   "Korrik", "Gusht", "Shtator", "Tetor", "N\xC3\xABntor", "Dhjetor" };
	return (month >= 1 && month <= 12) ? names[month - 1] : "";
}

const char* MonthShortAlb (int month)
{
	static const char* names[] = { "Jan", "Shk", "Mar", "Pri", "Maj", "Qer", "Kor", "Gus", "Sht", "Tet", "N\xC3\xABn", "Dhj" };
	return (month >= 1 && month <= 12) ? names[month - 1] : "";
}

const char* WeekdayNameAlb (int weekday)
{
	static const char* names[] = { "E h\xC3\xABn\xC3\xAB", "E mart\xC3\xAB", "E m\xC3\xABrkur\xC3\xAB", "E enjte",
								   "E premte", "E shtun\xC3\xAB", "E diel" };
	return (weekday >= 0 && weekday <= 6) ? names[weekday] : "";
}

const char* WeekdayShortAlb (int weekday)
{
	static const char* names[] = { "H\xC3\xAB", "Ma", "M\xC3\xAB", "En", "Pr", "Sh", "Di" };
	return (weekday >= 0 && weekday <= 6) ? names[weekday] : "";
}

Date CatholicEaster (int year)
{
	// algoritmi "Anonymous Gregorian" (Meeus/Jones/Butcher)
	const int a = year % 19;
	const int b = year / 100;
	const int c = year % 100;
	const int d = b / 4;
	const int e = b % 4;
	const int f = (b + 8) / 25;
	const int g = (b - f + 1) / 3;
	const int h = (19 * a + b - d - g + 15) % 30;
	const int i = c / 4;
	const int k = c % 4;
	const int l = (32 + 2 * e + 2 * i - h - k) % 7;
	const int m = (a + 11 * h + 22 * l) / 451;
	const int month = (h + l - 7 * m + 114) / 31;
	const int day = ((h + l - 7 * m + 114) % 31) + 1;
	return Date (year, month, day);
}

Date OrthodoxEaster (int year)
{
	// algoritmi Julian (Meeus), pastaj +13 dite per 1900-2099
	const int a = year % 4;
	const int b = year % 7;
	const int c = year % 19;
	const int d = (19 * c + 15) % 30;
	const int e = (2 * a + 4 * b - d + 34) % 7;
	const int month = (d + e + 114) / 31;
	const int day = ((d + e + 114) % 31) + 1;
	Date julian (year, month, day);
	// Data Juliane -> Gregoriane: e njejta "etikete" + 13 dite
	return Date::FromDays (julian.ToDays () + 13);
}

}	// namespace ARBS
