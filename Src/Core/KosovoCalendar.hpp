// =============================================================================
// ARBS Studio - Core : KosovoCalendar.hpp
// Kalendari i punes ne Republiken e Kosoves:
//  - festat zyrtare sipas Ligjit Nr. 03/L-064 per Festat Zyrtare
//  - zhvendosja e festes qe bie ne fundjave ne diten e pare te punes (neni 4)
//  - Pashket Katolike dhe Ortodokse (e hena), Bajramet (tabele e konfigurueshme)
//  - periudha shtese jo-pune (pushim kolektiv, dimer per punet e ndjeshme ndaj motit)
// =============================================================================
#pragma once

#include "Date.hpp"
#include "Json.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ARBS {

struct Holiday {
	Date        date;				// data kalendarike e festes
	Date        observed;			// dita qe nuk punohet (e njejte ose e zhvendosur)
	std::string name;
	bool        shifted = false;	// true nese ra ne fundjave dhe u zhvendos
	bool        estimated = false;	// true per datat e Bajrameve qe nuk jane konfirmuar
};

struct DateRange {
	Date        from, to;			// perfshirese
	std::string name;
	bool        weatherOnly = false;	// vlen vetem per detyrat e ndjeshme ndaj motit
	bool Contains (const Date& d) const { return from <= d && d <= to; }
};

class KosovoCalendar {
public:
	KosovoCalendar ();

	// Ngarkon rregullat nga holidays.json (shih Data/holidays.json)
	void LoadRules (const Json::Value& rules);

	void SetSaturdayWorking (bool on) { saturdayWorking = on; }
	bool IsSaturdayWorking () const   { return saturdayWorking; }
	void SetWeekendShift (bool on)    { weekendShift = on; cache.clear (); }

	void AddNonWorkingRange (const DateRange& r) { extraRanges.push_back (r); }
	void ClearNonWorkingRanges () { extraRanges.clear (); }
	const std::vector<DateRange>& GetNonWorkingRanges () const { return extraRanges; }

	// Festat e nje viti (te renditura sipas dites qe nuk punohet)
	const std::vector<Holiday>& HolidaysOfYear (int year) const;
	std::vector<Holiday> HolidaysBetween (const Date& from, const Date& to) const;

	bool IsHoliday (const Date& d, std::string* name = nullptr) const;	// dite e lire zyrtare (observed)
	bool IsWorkingDay (const Date& d, bool weatherSensitive = false) const;

	Date NextWorkingDay (const Date& d, bool weatherSensitive = false) const;	// d nese eshte dite pune
	Date PrevWorkingDay (const Date& d, bool weatherSensitive = false) const;
	// Data e perfundimit per nje detyre me "duration" dite pune qe fillon ne "start"
	Date FinishDate (const Date& start, int duration, bool weatherSensitive = false) const;
	// Data e fillimit pas "lag" diteve pune nga "after" (dita e pare e punes pas "after")
	Date StartAfter (const Date& after, int lagDays = 0, bool weatherSensitive = false) const;
	int  CountWorkingDays (const Date& from, const Date& to, bool weatherSensitive = false) const;

	std::string LawReference () const { return lawReference; }

private:
	struct FixedHoliday { int month; int day; std::string name; };
	struct IslamicDate  { Date date; bool estimated; };

	void Build (int year) const;

	std::vector<FixedHoliday> fixed;
	bool catholicEasterMonday = true;
	bool orthodoxEasterMonday = true;
	std::string catholicName = "Pashk\xC3\xABt Katolike (e h\xC3\xABna)";
	std::string orthodoxName = "Pashk\xC3\xABt Ortodokse (e h\xC3\xABna)";
	std::string fitrName = "Bajrami i Vog\xC3\xABl (Fitr Bajrami)";
	std::string adhaName = "Bajrami i Madh (Kurban Bajrami)";
	std::map<int, std::vector<IslamicDate>> fitr;
	std::map<int, std::vector<IslamicDate>> adha;
	bool weekendShift = true;
	bool saturdayWorking = false;
	std::string lawReference;
	std::vector<DateRange> extraRanges;

	mutable std::map<int, std::vector<Holiday>> cache;
	mutable std::map<int, std::set<long>>       observedDays;
};

}	// namespace ARBS
