// =============================================================================
// ARBS Studio - Core : KosovoCalendar.cpp
// =============================================================================
#include "KosovoCalendar.hpp"

#include <algorithm>

namespace ARBS {

KosovoCalendar::KosovoCalendar ()
{
	// Vlerat e paracaktuara (Ligji Nr. 03/L-064). Mbishkruhen nga holidays.json.
	fixed = {
		{ 1, 1,  "Viti i Ri" },
		{ 1, 2,  "Viti i Ri (dita e dyt\xC3\xAB)" },
		{ 1, 7,  "Krishtlindjet Ortodokse" },
		{ 2, 17, "Dita e Pavar\xC3\xABsis\xC3\xAB s\xC3\xAB Republik\xC3\xABs s\xC3\xAB Kosov\xC3\xABs" },
		{ 4, 9,  "Dita e Kushtetut\xC3\xABs s\xC3\xAB Republik\xC3\xABs s\xC3\xAB Kosov\xC3\xABs" },
		{ 5, 1,  "Dita Nd\xC3\xABrkomb\xC3\xABtare e Pun\xC3\xABtor\xC3\xABve" },
		{ 5, 9,  "Dita e Evrop\xC3\xABs" },
		{ 12, 25, "Krishtlindjet Katolike" },
	};
	lawReference = "Ligji Nr. 03/L-064 p\xC3\xABr Festat Zyrtare n\xC3\xAB Republik\xC3\xABn e Kosov\xC3\xABs";
}

void KosovoCalendar::LoadRules (const Json::Value& rules)
{
	cache.clear ();
	observedDays.clear ();

	if (rules.Has ("law"))
		lawReference = rules.GetString ("law");
	weekendShift = rules.GetBool ("weekendShift", true);

	if (rules["fixed"].IsArray ()) {
		fixed.clear ();
		for (const Json::Value& f : rules["fixed"].AsArray ())
			fixed.push_back ({ f.GetInt ("month"), f.GetInt ("day"), f.GetString ("name") });
	}

	const Json::Value& mov = rules["movable"];
	if (mov.IsObject ()) {
		catholicEasterMonday = mov["catholicEasterMonday"].IsNull () ? true : mov["catholicEasterMonday"].GetBool ("enabled", true);
		orthodoxEasterMonday = mov["orthodoxEasterMonday"].IsNull () ? true : mov["orthodoxEasterMonday"].GetBool ("enabled", true);
		catholicName = mov["catholicEasterMonday"].GetString ("name", catholicName);
		orthodoxName = mov["orthodoxEasterMonday"].GetString ("name", orthodoxName);
	}

	const Json::Value& isl = rules["islamic"];
	if (isl.IsObject ()) {
		fitrName = isl.GetString ("fitrName", fitrName);
		adhaName = isl.GetString ("adhaName", adhaName);
		fitr.clear ();
		adha.clear ();
		for (const Json::Member& yearEntry : isl["dates"].AsObject ()) {
			const Json::Value& v = yearEntry.second;
			const bool est = v.GetBool ("estimated", true);
			auto addAll = [&] (const Json::Value& list, std::map<int, std::vector<IslamicDate>>& target) {
				std::vector<std::string> items;
				if (list.IsString ()) items.push_back (list.AsString ());
				for (const Json::Value& s : list.AsArray ()) items.push_back (s.AsString ());
				for (const std::string& s : items) {
					Date d;
					if (Date::Parse (s, d)) target[d.y].push_back ({ d, est });
				}
			};
			addAll (v["fitr"], fitr);
			addAll (v["adha"], adha);
		}
	}
}

void KosovoCalendar::Build (int year) const
{
	if (cache.count (year) > 0) return;

	std::vector<Holiday> list;
	for (const FixedHoliday& f : fixed) {
		Holiday h;
		h.date = Date (year, f.month, f.day);
		h.name = f.name;
		list.push_back (h);
	}
	if (catholicEasterMonday) {
		Holiday h; h.date = CatholicEaster (year).AddDays (1); h.name = catholicName; list.push_back (h);
	}
	if (orthodoxEasterMonday) {
		Holiday h; h.date = OrthodoxEaster (year).AddDays (1); h.name = orthodoxName; list.push_back (h);
	}
	auto addIslamic = [&] (const std::map<int, std::vector<IslamicDate>>& src, const std::string& name) {
		auto it = src.find (year);
		if (it == src.end ()) return;
		for (const IslamicDate& d : it->second) {
			Holiday h; h.date = d.date; h.name = name; h.estimated = d.estimated; list.push_back (h);
		}
	};
	addIslamic (fitr, fitrName);
	addIslamic (adha, adhaName);

	std::sort (list.begin (), list.end (), [] (const Holiday& a, const Holiday& b) { return a.date.ToDays () < b.date.ToDays (); });

	// Ditet e lira qe bien ne dite pune nuk zhvendosen
	std::set<long> taken;
	for (Holiday& h : list) {
		h.observed = h.date;
		if (!h.date.IsWeekend ())
			taken.insert (h.date.ToDays ());
	}
	// Neni 4: festa ne te shtune/te diel -> dita e pare e punes qe nuk eshte feste
	if (weekendShift) {
		for (Holiday& h : list) {
			if (!h.date.IsWeekend ()) continue;
			Date d = h.date;
			do { d = d.AddDays (1); } while (d.IsWeekend () || taken.count (d.ToDays ()) > 0);
			h.observed = d;
			h.shifted = true;
			taken.insert (d.ToDays ());
		}
	}

	std::sort (list.begin (), list.end (), [] (const Holiday& a, const Holiday& b) { return a.observed.ToDays () < b.observed.ToDays (); });

	std::set<long>& obs = observedDays[year];
	for (const Holiday& h : list)
		obs.insert (h.observed.ToDays ());
	cache[year] = list;
}

const std::vector<Holiday>& KosovoCalendar::HolidaysOfYear (int year) const
{
	Build (year);
	return cache[year];
}

std::vector<Holiday> KosovoCalendar::HolidaysBetween (const Date& from, const Date& to) const
{
	std::vector<Holiday> result;
	for (int y = from.y - 1; y <= to.y; ++y) {	// y-1: festa e dhjetorit e zhvendosur ne janar
		for (const Holiday& h : HolidaysOfYear (y))
			if (from <= h.observed && h.observed <= to)
				result.push_back (h);
	}
	return result;
}

bool KosovoCalendar::IsHoliday (const Date& d, std::string* name) const
{
	for (int y = d.y - 1; y <= d.y; ++y) {
		Build (y);
		if (observedDays[y].count (d.ToDays ()) == 0) continue;
		if (name != nullptr) {
			for (const Holiday& h : cache[y])
				if (h.observed == d) { *name = h.name; break; }
		}
		return true;
	}
	return false;
}

bool KosovoCalendar::IsWorkingDay (const Date& d, bool weatherSensitive) const
{
	const int wd = d.Weekday ();
	if (wd == 6) return false;
	if (wd == 5 && !saturdayWorking) return false;
	if (IsHoliday (d)) return false;
	for (const DateRange& r : extraRanges) {
		if (r.weatherOnly && !weatherSensitive) continue;
		if (r.Contains (d)) return false;
	}
	return true;
}

Date KosovoCalendar::NextWorkingDay (const Date& d, bool weatherSensitive) const
{
	Date x = d;
	for (int guard = 0; guard < 800 && !IsWorkingDay (x, weatherSensitive); ++guard)
		x = x.AddDays (1);
	return x;
}

Date KosovoCalendar::PrevWorkingDay (const Date& d, bool weatherSensitive) const
{
	Date x = d;
	for (int guard = 0; guard < 800 && !IsWorkingDay (x, weatherSensitive); ++guard)
		x = x.AddDays (-1);
	return x;
}

Date KosovoCalendar::FinishDate (const Date& start, int duration, bool weatherSensitive) const
{
	Date d = NextWorkingDay (start, weatherSensitive);
	if (duration <= 1) return d;
	int counted = 1;
	while (counted < duration) {
		d = NextWorkingDay (d.AddDays (1), weatherSensitive);
		++counted;
	}
	return d;
}

Date KosovoCalendar::StartAfter (const Date& after, int lagDays, bool weatherSensitive) const
{
	Date d = NextWorkingDay (after.AddDays (1), weatherSensitive);
	for (int i = 0; i < lagDays; ++i)
		d = NextWorkingDay (d.AddDays (1), weatherSensitive);
	return d;
}

int KosovoCalendar::CountWorkingDays (const Date& from, const Date& to, bool weatherSensitive) const
{
	int n = 0;
	for (long day = from.ToDays (); day <= to.ToDays (); ++day)
		if (IsWorkingDay (Date::FromDays (day), weatherSensitive)) ++n;
	return n;
}

}	// namespace ARBS
