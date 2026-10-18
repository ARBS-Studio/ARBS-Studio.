// =============================================================================
// ARBS Studio - shtresa Archicad : Commands.cpp
// =============================================================================
#include "AC/Commands.hpp"

#include "AC/ArbsUtils.hpp"
#include "AC/CompositeBuilder.hpp"
#include "AC/ModelReader.hpp"
#include "AC/ProjectStore.hpp"
#include "AC/TemplateSetup.hpp"

#include "Core/BillOfQuantities.hpp"
#include "Core/DynamicPlan.hpp"
#include "Core/KosovoCalendar.hpp"
#include "Core/Reports.hpp"
#include "Core/Solar.hpp"

#include <exception>
#include <functional>
#include <map>

#ifndef ADDON_VERSION
#define ADDON_VERSION "0.1.0"
#endif

namespace ARBS {
namespace AC {

namespace {

Json::Value Ok ()
{
	Json::Value v = Json::Value::MakeObject ();
	v.Set ("ok", true);
	return v;
}

Json::Value Fail (const std::string& msg)
{
	Json::Value v = Json::Value::MakeObject ();
	v.Set ("ok", false);
	v.Set ("error", msg);
	return v;
}

// projekti + kodi i fazes per emrat e skedareve
Json::Value ProjectForReports ()
{
	Json::Value p = ProjectStore::LoadProject ();
	std::string phase = p.GetString ("faza_kodi");
	if (phase.empty ()) {
		const std::string f = p.GetString ("faza");
		for (const Json::Value& ph : GetCatalog ().Phases ()["phases"].AsArray ())
			if (ph.GetString ("name") == f || ph.GetString ("code") == f) phase = ph.GetString ("code");
		if (!phase.empty ()) p.Set ("faza_kodi", phase);
	}
	return p;
}

Json::Value SaveAndOpen (const Json::Value& project, const std::vector<ReportOutput>& outs, bool openFirst)
{
	GSErrCode err = NoError;
	const std::vector<std::string> paths = SaveOutputs (project, outs, &err);
	if (err != NoError) return Fail ("Dokumentet nuk u ruajten (kontrolloni lejet e dosjes Dokumentet).");
	Json::Value r = Ok ();
	Json::Value files = Json::Value::MakeArray ();
	for (const std::string& p : paths) files.Push (Json::Value (p));
	r.Set ("files", files);
	r.Set ("folder", LocationToUtf8 (OutputFolder (project)));
	if (openFirst && !paths.empty ()) OpenWithSystem (paths.front ());
	return r;
}

Json::Value CatalogForUi ()
{
	const Catalog& cat = GetCatalog ();
	Json::Value c = Json::Value::MakeObject ();
	c.Set ("phases", cat.Phases ()["phases"]);
	c.Set ("naming", cat.Phases ()["naming"]);
	c.Set ("disciplines", cat.Phases ()["disciplines"]);
	c.Set ("layers", cat.Layers ()["layers"]);
	Json::Value combos = Json::Value::MakeArray ();
	for (const Json::Value& cb : cat.Layers ()["combinations"].AsArray ()) combos.Push (Json::Value (cb.GetString ("name")));
	c.Set ("combinations", combos);
	c.Set ("composites", cat.CompositeSummary ());
	c.Set ("umaxSource", cat.UMaxSource ());
	Json::Value cities = Json::Value::MakeArray ();
	for (const City& city : cat.Cities ()) {
		Json::Value o = Json::Value::MakeObject ();
		o.Set ("id", city.id);
		o.Set ("name", city.name);
		o.Set ("lat", city.lat);
		o.Set ("lon", city.lon);
		o.Set ("alt", city.alt);
		cities.Push (o);
	}
	c.Set ("cities", cities);
	c.Set ("projectFields", cat.Documents ()["projectFields"]);
	c.Set ("law", cat.Holidays ().GetString ("law"));
	c.Set ("prices", BillOfQuantities::PriceList (cat));
	return c;
}

Json::Value HolidaysJson (const KosovoCalendar& cal, const Date& from, const Date& to)
{
	Json::Value arr = Json::Value::MakeArray ();
	for (const Holiday& h : cal.HolidaysBetween (from, to)) {
		Json::Value o = Json::Value::MakeObject ();
		o.Set ("name", h.name);
		o.Set ("date", h.date.ToIso ());
		o.Set ("observed", h.observed.ToIso ());
		o.Set ("weekday", WeekdayNameAlb (h.observed.Weekday ()));
		o.Set ("shifted", h.shifted);
		o.Set ("estimated", h.estimated);
		arr.Push (o);
	}
	return arr;
}

// ----------------------------------------------------------------------------- komandat

Json::Value CmdInit (const Json::Value&)
{
	Json::Value r = Ok ();
	r.Set ("version", ADDON_VERSION);
	const std::string err = CatalogError ();
	if (!err.empty ()) r.Set ("catalogError", err);
	r.Set ("catalog", CatalogForUi ());
	r.Set ("office", ProjectStore::LoadOffice ());
	r.Set ("project", ProjectStore::LoadProject ());
	r.Set ("plan", ProjectStore::LoadPlan ());
	r.Set ("logo", LogoDataUri ());
	r.Set ("folder", LocationToUtf8 (OutputFolder (ProjectStore::LoadProject ())));
	return r;
}

Json::Value CmdSaveOffice (const Json::Value& req)
{
	if (!ProjectStore::SaveOffice (req["office"])) return Fail ("Profili i zyr\xC3\xABs nuk u ruajt.");
	return Ok ();
}

Json::Value CmdSaveProject (const Json::Value& req)
{
	if (!ProjectStore::SaveProject (req["project"])) return Fail ("T\xC3\xAB dh\xC3\xABnat e projektit nuk u ruajt\xC3\xABn n\xC3\xAB skedarin e projektit.");
	Json::Value r = Ok ();
	if (req.GetBool ("pushProjectInfo", true))
		r.Set ("projectInfo", ProjectStore::PushToProjectInfo (req["project"], ProjectStore::LoadOffice ()));
	return r;
}

Json::Value CmdSetupTemplate (const Json::Value& req)
{
	SetupOptions opt;
	opt.layers = req.GetBool ("layers", true);
	opt.combinations = req.GetBool ("combinations", true);
	opt.viewMap = req.GetBool ("viewMap", true);
	opt.layoutBook = req.GetBool ("layoutBook", true);
	opt.perStoreySheets = req.GetBool ("perStorey", true);
	if (req["phases"].IsArray ()) {
		opt.phases.clear ();
		for (const Json::Value& p : req["phases"].AsArray ()) opt.phases.push_back (p.AsString ());
	}
	Json::Value r = Ok ();
	r.Set ("report", TemplateSetup::Run (opt));
	return r;
}

Json::Value CmdCreateComposites (const Json::Value& req)
{
	std::vector<std::string> codes;
	for (const Json::Value& c : req["codes"].AsArray ()) codes.push_back (c.AsString ());
	Json::Value r = Ok ();
	r.Set ("report", CompositeBuilder::Create (codes, req.GetBool ("update", false)));
	return r;
}

Site SiteFromRequest (const Json::Value& req)
{
	Site s = GetCatalog ().SiteFor (req.GetString ("cityId", "prishtine"));
	if (req.GetNumber ("lat") != 0.0 && req.GetNumber ("lon") != 0.0) {
		s.latitude = req.GetNumber ("lat");
		s.longitude = req.GetNumber ("lon");
	}
	if (req.GetNumber ("alt") != 0.0) s.altitude = req.GetNumber ("alt");
	return s;
}

Json::Value CmdSetLocation (const Json::Value& req)
{
	const Site s = SiteFromRequest (req);
	API_PlaceInfo pi {};
	if (ACAPI_GeoLocation_GetPlaceSets (&pi) != NoError) return Fail ("Lokacioni i projektit nuk mund t\xC3\xAB lexohet.");
	pi.latitude = s.latitude;
	pi.longitude = s.longitude;
	pi.altitude = s.altitude;
	pi.timeZoneInMinutes = static_cast<short> (s.baseUtcOffset * 60.0);
	if (req.Has ("date")) {
		Date d;
		if (Date::Parse (req.GetString ("date"), d)) {
			pi.year = static_cast<unsigned short> (d.y);
			pi.month = static_cast<unsigned short> (d.m);
			pi.day = static_cast<unsigned short> (d.d);
			pi.hour = static_cast<unsigned short> (req.GetInt ("hour", 12));
			pi.minute = 0;
			pi.second = 0;
			pi.sumTime = Solar::IsEuDst (d, req.GetNumber ("hour", 12.0), s.baseUtcOffset);
		}
	}
	const GSErrCode err = TemplateSetup::RunCommand ("ARBS: Lokacioni", [&] () -> GSErrCode {
		API_PlaceInfo copy = pi;
		return ACAPI_GeoLocation_ChangePlaceSets (&copy);
	});
	if (err != NoError) return Fail ("Lokacioni nuk u ndryshua (kodi " + std::to_string (static_cast<long long> (err)) + ").");
	Json::Value r = Ok ();
	r.Set ("north", ModelReader::NorthDeg ());
	return r;
}

Json::Value CmdInsolation (const Json::Value& req)
{
	InsolationOptions opt;
	opt.cityId = req.GetString ("cityId", "prishtine");
	opt.latitude = req.GetNumber ("lat");
	opt.longitude = req.GetNumber ("lon");
	opt.altitude = req.GetNumber ("alt");
	opt.buildingHeight = req.GetNumber ("height", 9.0);
	opt.year = req.GetInt ("year", 2026);
	opt.minSunHours = req.GetNumber ("minSunHours", 2.0);
	opt.projectNorthDeg = req.Has ("north") ? req.GetNumber ("north") : ModelReader::NorthDeg ();
	const Json::Value project = ProjectForReports ();
	return SaveAndOpen (project, Reports::Insolation (GetCatalog (), project, ProjectStore::OfficeForReports (), opt), true);
}

Json::Value CmdSchedule (const Json::Value& req)
{
	const std::string kind = req.GetString ("kind", "door");
	const Json::Value model = ModelReader::Read (req.GetBool ("selectionOnly", false));
	const Json::Value project = ProjectForReports ();
	const Json::Value office = ProjectStore::OfficeForReports ();
	std::vector<ReportOutput> outs;
	if (kind == "door" || kind == "window") outs = Reports::OpeningSchedule (project, office, model, kind);
	else if (kind == "inventory")          outs = Reports::InventorySchedule (project, office, model);
	else if (kind == "zones")              outs = Reports::ZoneSchedule (project, office, model);
	else return Fail ("Lloj i panjohur skeme: " + kind);
	Json::Value r = SaveAndOpen (project, outs, true);
	r.Set ("count", static_cast<int> (kind == "inventory" ? model["objects"].Size () : kind == "zones" ? model["zones"].Size () : model["openings"].Size ()));
	return r;
}

Json::Value CmdTechnicalDescription (const Json::Value&)
{
	const Json::Value project = ProjectForReports ();
	const Json::Value model = ModelReader::Read (false);
	return SaveAndOpen (project, { Reports::TechnicalDescription (GetCatalog (), project, ProjectStore::OfficeForReports (), model) }, true);
}

Json::Value CmdBuildingPhysics (const Json::Value& req)
{
	std::vector<std::string> codes;
	for (const Json::Value& c : req["codes"].AsArray ()) codes.push_back (c.AsString ());
	const Json::Value project = ProjectForReports ();
	const Json::Value model = ModelReader::Read (false);
	return SaveAndOpen (project, Reports::BuildingPhysics (GetCatalog (), project, ProjectStore::OfficeForReports (), model, codes), true);
}

Json::Value CmdModelSummary (const Json::Value&)
{
	const Json::Value model = ModelReader::Read (false);
	double area = 0.0;
	for (const Json::Value& z : model["zones"].AsArray ()) area += z.GetNumber ("area");
	int doors = 0, windows = 0;
	for (const Json::Value& o : model["openings"].AsArray ()) (o.GetString ("kind") == "door" ? doors : windows)++;
	int above = 0, below = 0;
	for (const Json::Value& s : model["stories"].AsArray ()) (s.GetInt ("index") < 0 ? below : above)++;
	Json::Value r = Ok ();
	r.Set ("stories", model["stories"]);
	r.Set ("floorsAbove", above);
	r.Set ("floorsBelow", below);
	r.Set ("netArea", area);
	r.Set ("doors", doors);
	r.Set ("windows", windows);
	r.Set ("objects", static_cast<int> (model["objects"].Size ()));
	r.Set ("composites", model["composites"]);
	r.Set ("north", model["north"]);
	return r;
}

// cmimet e zyres (profili) + parametrat e kerkeses
BoqOptions BoqOptionsFrom (const Json::Value& req)
{
	BoqOptions opt = BillOfQuantities::OptionsFromJson (req, GetCatalog ());
	const Json::Value office = ProjectStore::LoadOffice ();
	Json::Value merged = Json::Value::MakeObject ();
	if (office["cmimet"].IsObject ())
		for (const auto& kv : office["cmimet"].AsObject ()) if (kv.second.IsNumber ()) merged.Set (kv.first, kv.second);
	if (req["overrides"].IsObject ())
		for (const auto& kv : req["overrides"].AsObject ()) if (kv.second.IsNumber ()) merged.Set (kv.first, kv.second);
	opt.overrides = merged;
	return opt;
}

Json::Value CmdBillOfQuantities (const Json::Value& req)
{
	const Json::Value project = ProjectForReports ();
	const Json::Value model = ModelReader::Read (false);
	const BoqOptions opt = BoqOptionsFrom (req);
	if (!req.GetBool ("export", true)) {
		Json::Value r = Ok ();
		r.Set ("boq", BillOfQuantities::ToJson (BillOfQuantities::Compute (GetCatalog (), model, project, opt)));
		return r;
	}
	Json::Value r = SaveAndOpen (project, Reports::QuantitiesAndCost (GetCatalog (), project, ProjectStore::OfficeForReports (), model, opt), true);
	if (r.GetBool ("ok")) r.Set ("boq", BillOfQuantities::ToJson (BillOfQuantities::Compute (GetCatalog (), model, project, opt)));
	return r;
}

Json::Value CmdSavePrices (const Json::Value& req)
{
	Json::Value office = ProjectStore::LoadOffice ();
	Json::Value prices = Json::Value::MakeObject ();
	if (req["prices"].IsObject ())
		for (const auto& kv : req["prices"].AsObject ()) if (kv.second.IsNumber () && kv.second.AsNumber () >= 0.0) prices.Set (kv.first, kv.second);
	office.Set ("cmimet", prices);
	if (!ProjectStore::SaveOffice (office)) return Fail ("\xC3\x87mimet e zyr\xC3\xABs nuk u ruajt\xC3\xABn.");
	Json::Value r = Ok ();
	r.Set ("count", static_cast<int> (prices.AsObject ().size ()));
	return r;
}

PlanParameters PlanParamsFrom (const Json::Value& req)
{
	PlanParameters p;
	if (!Date::Parse (req.GetString ("start"), p.start)) p.start = Date (2027, 3, 1);
	p.grossArea = req.GetNumber ("area", 300.0);
	p.floorsAbove = req.GetInt ("floorsAbove", 2);
	p.floorsBelow = req.GetInt ("floorsBelow", 0);
	p.saturdayWorking = req.GetBool ("saturday", false);
	p.winterStopForWeatherTasks = req.GetBool ("winter", true);
	for (const Json::Value& l : req["leave"].AsArray ()) {
		DateRange r;
		if (Date::Parse (l.GetString ("from"), r.from) && Date::Parse (l.GetString ("to"), r.to)) {
			r.name = l.GetString ("name", "Pushim kolektiv");
			p.collectiveLeave.push_back (r);
		}
	}
	return p;
}

Json::Value CmdDynamicPlan (const Json::Value& req)
{
	const Catalog& cat = GetCatalog ();
	const PlanParameters p = PlanParamsFrom (req);
	KosovoCalendar cal;
	cal.LoadRules (cat.Holidays ());
	DynamicPlan::ConfigureCalendar (cal, p);

	std::vector<PlanTask> tasks = (req["tasks"].IsArray () && req["tasks"].Size () > 0)
		? DynamicPlan::FromJson (req["tasks"])
		: DynamicPlan::FromTemplate (cat.PlanTemplate (), p);
	const std::string err = DynamicPlan::Schedule (tasks, cal, p.start);
	if (!err.empty ()) return Fail (err);

	Json::Value stored = Json::Value::MakeObject ();
	stored.Set ("params", req);
	stored.Set ("tasks", DynamicPlan::ToJson (tasks));
	ProjectStore::SavePlan (stored);

	Json::Value r = Ok ();
	if (req.GetBool ("export", true)) {
		const Json::Value project = ProjectForReports ();
		r = SaveAndOpen (project, Reports::DynamicPlanReport (project, ProjectStore::OfficeForReports (), tasks, cal, p), req.GetBool ("open", true));
	}
	const Date finish = DynamicPlan::ProjectFinish (tasks);
	r.Set ("tasks", DynamicPlan::ToJson (tasks));
	r.Set ("start", p.start.ToIso ());
	r.Set ("finish", finish.ToIso ());
	r.Set ("workingDays", cal.CountWorkingDays (p.start, finish));
	r.Set ("calendarDays", static_cast<int> (finish.ToDays () - p.start.ToDays () + 1));
	r.Set ("holidays", HolidaysJson (cal, p.start, finish));
	return r;
}

Json::Value CmdHolidays (const Json::Value& req)
{
	KosovoCalendar cal;
	cal.LoadRules (GetCatalog ().Holidays ());
	const int year = req.GetInt ("year", 2026);
	Json::Value r = Ok ();
	r.Set ("holidays", HolidaysJson (cal, Date (year, 1, 1), Date (year, 12, 31)));
	r.Set ("workingDays", cal.CountWorkingDays (Date (year, 1, 1), Date (year, 12, 31)));
	return r;
}

Json::Value CmdOpen (const Json::Value& req)
{
	std::string path = req.GetString ("path");
	if (path.empty ()) path = LocationToUtf8 (OutputFolder (ProjectStore::LoadProject ()));
	return OpenWithSystem (path) ? Ok () : Fail ("Skedari/dosja nuk u hap: " + path);
}

}	// namespace

std::string Commands::Handle (const std::string& request)
{
	static const std::map<std::string, std::function<Json::Value (const Json::Value&)>> handlers = {
		{ "init", CmdInit },
		{ "saveOffice", CmdSaveOffice },
		{ "saveProject", CmdSaveProject },
		{ "setupTemplate", CmdSetupTemplate },
		{ "createComposites", CmdCreateComposites },
		{ "setLocation", CmdSetLocation },
		{ "insolation", CmdInsolation },
		{ "schedule", CmdSchedule },
		{ "technicalDescription", CmdTechnicalDescription },
		{ "buildingPhysics", CmdBuildingPhysics },
		{ "billOfQuantities", CmdBillOfQuantities },
		{ "savePrices", CmdSavePrices },
		{ "modelSummary", CmdModelSummary },
		{ "dynamicPlan", CmdDynamicPlan },
		{ "holidays", CmdHolidays },
		{ "open", CmdOpen },
	};
	Json::Value result;
	try {
		const Json::Value req = Json::Value::Parse (request);
		auto it = handlers.find (req.GetString ("cmd"));
		result = (it == handlers.end ()) ? Fail ("Komand\xC3\xAB e panjohur: " + req.GetString ("cmd")) : it->second (req);
	} catch (const Json::ParseError& e) {
		result = Fail (std::string ("K\xC3\xABrkes\xC3\xAB e pavlefshme: ") + e.what ());
	} catch (const std::exception& e) {
		result = Fail (std::string ("Gabim: ") + e.what ());
	} catch (...) {
		result = Fail ("Gabim i papritur n\xC3\xAB ARBS Studio.");
	}
	return result.Dump ();
}

}	// namespace AC
}	// namespace ARBS
