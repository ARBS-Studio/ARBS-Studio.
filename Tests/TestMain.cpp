// =============================================================================
// ARBS Studio - testet e berthames (kompilohen ne Linux/Windows/macOS pa Archicad)
// =============================================================================
#include "Core/BuildingPhysics.hpp"
#include "Core/Catalog.hpp"
#include "Core/Date.hpp"
#include "Core/DynamicPlan.hpp"
#include "Core/Json.hpp"
#include "Core/KosovoCalendar.hpp"
#include "Core/Reports.hpp"
#include "Core/Solar.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace ARBS;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf ("  DESHTOI: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; double _a = (a), _b = (b); if (std::fabs (_a - _b) > (tol)) { ++failures; std::printf ("  DESHTOI: %s = %.4f, pritej %.4f  (%s:%d)\n", #a, _a, _b, __FILE__, __LINE__); } } while (0)

static void TestJson ()
{
	std::printf ("Json\n");
	Json::Value v = Json::Value::Parse (u8"{\"a\": [1, 2.5, \"\u00ebsht\u00eb\"], \"b\": {\"c\": true}, // koment\n \"d\": null}");
	CHECK (v["a"].Size () == 3);
	CHECK_NEAR (v["a"][1].AsNumber (), 2.5, 1e-12);
	CHECK (v["a"][2].AsString () == u8"\u00ebsht\u00eb");
	CHECK (v["b"]["c"].AsBool ());
	CHECK (v["d"].IsNull ());
	Json::Value copy = v;
	copy["b"].Set ("c", false);
	CHECK (v["b"]["c"].AsBool ());		// kopje e thelle
	CHECK (Json::Value::Parse (v.Dump ()).Dump () == v.Dump ());
	CHECK (Json::Value::Parse ("\"\\u00eb\"").AsString () == u8"\u00eb");
	bool threw = false;
	try { Json::Value::Parse ("{\"a\":}"); } catch (const Json::ParseError&) { threw = true; }
	CHECK (threw);
}

static void TestDates ()
{
	std::printf ("Data dhe Pashket\n");
	CHECK (Date (2026, 10, 17).Weekday () == 5);	// e shtune
	CHECK (Date (1970, 1, 1).ToDays () == 0);
	CHECK (Date::FromDays (Date (2024, 2, 29).ToDays ()) == Date (2024, 2, 29));
	CHECK (CatholicEaster (2025) == Date (2025, 4, 20));
	CHECK (OrthodoxEaster (2025) == Date (2025, 4, 20));
	CHECK (CatholicEaster (2026) == Date (2026, 4, 5));
	CHECK (OrthodoxEaster (2026) == Date (2026, 4, 12));
	CHECK (CatholicEaster (2027) == Date (2027, 3, 28));
	CHECK (OrthodoxEaster (2027) == Date (2027, 5, 2));
	Date p;
	CHECK (Date::Parse ("17.10.2026", p) && p == Date (2026, 10, 17));
	CHECK (Date (2026, 10, 17).ToAlbLong () == u8"17 tetor 2026");
}

static void TestCalendar (const Catalog& cat)
{
	std::printf ("Kalendari i Kosoves\n");
	KosovoCalendar cal;
	cal.LoadRules (cat.Holidays ());
	std::string name;
	// 2026: 9 maj (e shtune) -> e hene 11 maj
	CHECK (!cal.IsHoliday (Date (2026, 5, 9)));
	CHECK (cal.IsHoliday (Date (2026, 5, 11), &name));
	CHECK (name.find ("Evrop") != std::string::npos);
	// 2024: 7 janar (e diel) -> 8 janar ; 17 shkurt (e shtune) -> 19 shkurt
	CHECK (cal.IsHoliday (Date (2024, 1, 8)));
	CHECK (cal.IsHoliday (Date (2024, 2, 19)));
	// 2023: 1 janar (e diel), 2 janar (e hene) -> 3 janar
	CHECK (cal.IsHoliday (Date (2023, 1, 3)));
	// Pashket e hena 2026
	CHECK (cal.IsHoliday (Date (2026, 4, 6)));
	CHECK (cal.IsHoliday (Date (2026, 4, 13)));
	// Bajramet 2026
	CHECK (cal.IsHoliday (Date (2026, 3, 20)));
	CHECK (cal.IsHoliday (Date (2026, 5, 27)));
	// 2025: Fitr 30 mars (e diel) -> 31 mars
	CHECK (cal.IsHoliday (Date (2025, 3, 31)));
	// 2027: 1 maj (e shtune) -> 3 maj eshte Pashket Ortodokse -> 4 maj
	CHECK (cal.IsHoliday (Date (2027, 5, 3)));
	CHECK (cal.IsHoliday (Date (2027, 5, 4)));
	// dite pune
	CHECK (cal.IsWorkingDay (Date (2026, 10, 19)));
	CHECK (!cal.IsWorkingDay (Date (2026, 10, 17)));
	// 10 dite pune nga 2026-05-04 (e hene): 4-8, 12-15 (11 feste), 18 => 18 maj
	CHECK (cal.FinishDate (Date (2026, 5, 4), 10) == Date (2026, 5, 18));
	const int holidays2026 = static_cast<int> (cal.HolidaysOfYear (2026).size ());
	CHECK (holidays2026 == 12);
	// dite pune ne vitin 2026: 261 dite te javes - festat ne dite te javes
	CHECK (cal.CountWorkingDays (Date (2026, 1, 1), Date (2026, 12, 31)) == 261 - 12);
}

static void TestPlan (const Catalog& cat)
{
	std::printf ("Plani dinamik\n");
	PlanParameters p;
	p.start = Date (2026, 11, 2);
	p.grossArea = 420.0;
	p.floorsAbove = 3;
	p.floorsBelow = 1;
	KosovoCalendar cal;
	cal.LoadRules (cat.Holidays ());
	DynamicPlan::ConfigureCalendar (cal, p);
	std::vector<PlanTask> tasks = DynamicPlan::FromTemplate (cat.PlanTemplate (), p);
	CHECK (tasks.size () > 20);
	std::string err = DynamicPlan::Schedule (tasks, cal, p.start);
	CHECK (err.empty ());
	if (!err.empty ()) std::printf ("  %s\n", err.c_str ());
	bool anyCritical = false;
	for (const PlanTask& t : tasks) {
		if (t.critical) anyCritical = true;
		CHECK (t.es <= t.ef);
		if (t.duration > 0) {
			CHECK (cal.IsWorkingDay (t.es, t.weatherSensitive));
			CHECK (cal.IsWorkingDay (t.ef, t.weatherSensitive));
		}
		if (t.weatherSensitive && t.duration > 0) {
			CHECK (!(t.es.m == 1 || (t.es.m == 12 && t.es.d >= 15)));
		}
	}
	CHECK (anyCritical);
	CHECK (p.start < DynamicPlan::ProjectFinish (tasks));

	// pa bodrum: detyrat e bodrumit hiqen dhe varesite trashegohen
	PlanParameters p2 = p;
	p2.floorsBelow = 0;
	std::vector<PlanTask> t2 = DynamicPlan::FromTemplate (cat.PlanTemplate (), p2);
	CHECK (t2.size () < tasks.size ());
	CHECK (DynamicPlan::Schedule (t2, cal, p2.start).empty ());

	// cikel
	std::vector<PlanTask> cyc (2);
	cyc[0].id = "A"; cyc[0].links.push_back ({ "B", "FS", 0 });
	cyc[1].id = "B"; cyc[1].links.push_back ({ "A", "FS", 0 });
	CHECK (!DynamicPlan::Schedule (cyc, cal, p.start).empty ());
}

static void TestPhysics (const Catalog& cat)
{
	std::printf ("Fizika e nderteses\n");
	// muri: llac 2cm (0.87), bllok 25cm (0.40), EPS 10cm (0.038), llac armues 0.5cm(0.8) + dekorativ 0.5 (0.7)
	CHECK_NEAR (BuildingPhysics::SaturationPressure (20.0), 2337.0, 3.0);
	CHECK_NEAR (BuildingPhysics::SaturationPressure (-10.0), 260.0, 2.0);

	int compliant = 0, total = 0;
	for (const Composite& c : cat.Composites ()) {
		const UResult u = BuildingPhysics::ComputeU (c, cat.UMax (c.umaxKey));
		const GlaserResult g = BuildingPhysics::Glaser (c, cat.GlaserClimateFor ("prishtine"));
		++total;
		if (u.compliant) ++compliant;
		std::printf ("  %-6s U=%.3f  Umax=%.2f  %s  %s  m=%.0f kg/m2  kond=%.3f\n", c.code.c_str (), u.u, u.umax,
					 u.compliant ? "OK " : "JO ", g.condensation ? (g.ok ? "kond(ok)" : "KOND!") : "-", u.arealMass, g.condensedKgM2);
		CHECK (u.u > 0.05 && u.u < 6.0);
		CHECK (c.TotalThickness () > 0.0);
		CHECK (g.ok);
	}
	CHECK (compliant == total);
}

static void TestSolar (const Catalog& cat)
{
	std::printf ("Dielli\n");
	Site s = cat.SiteFor ("prishtine");
	SunDay jun = Solar::Day (s, Date (2026, 6, 21));
	CHECK_NEAR (jun.noonAltitude, 90.0 - s.latitude + 23.44, 0.6);
	CHECK_NEAR (jun.solarNoon, 12.62, 0.06);
	CHECK_NEAR (jun.dayLength, 15.15, 0.2);
	CHECK (jun.dst);
	SunDay dec = Solar::Day (s, Date (2026, 12, 21));
	CHECK_NEAR (dec.noonAltitude, 90.0 - s.latitude - 23.44, 0.6);
	CHECK (!dec.dst);
	CHECK_NEAR (dec.dayLength, 9.2, 0.25);
	const SunPosition noon = Solar::Position (s, Date (2026, 6, 21), jun.solarNoon);
	CHECK_NEAR (noon.azimuth, 180.0, 1.0);
	const double south = Solar::FacadeSunHours (s, Date (2026, 3, 21), 180.0);
	const double north = Solar::FacadeSunHours (s, Date (2026, 12, 21), 0.0);
	CHECK (south > 9.0 && south < 12.5);
	CHECK (north < 0.1);
	std::printf ("  21.06 lindja %s perendimi %s  21.12 lindja %s perendimi %s\n",
				 Solar::FormatHour (jun.sunrise).c_str (), Solar::FormatHour (jun.sunset).c_str (),
				 Solar::FormatHour (dec.sunrise).c_str (), Solar::FormatHour (dec.sunset).c_str ());
}

int main (int argc, char** argv)
{
	const std::string dataDir = argc > 1 ? argv[1] : "Data";
	Catalog cat;
	std::string err;
	if (!cat.LoadFromDirectory (dataDir, &err)) {
		std::printf ("Nuk u ngarkuan te dhenat: %s\n", err.c_str ());
		return 2;
	}
	std::printf ("Katalogu: %zu materiale, %zu kompozita, %zu qytete\n", cat.Materials ().size (), cat.Composites ().size (), cat.Cities ().size ());
	TestJson ();
	TestDates ();
	TestCalendar (cat);
	TestPlan (cat);
	TestPhysics (cat);
	TestSolar (cat);
	std::printf ("\n%d kontrolle, %d deshtime\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
