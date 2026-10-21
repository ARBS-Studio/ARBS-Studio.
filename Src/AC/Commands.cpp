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
#include "Core/Structure.hpp"

#include <exception>
#include <functional>
#include <map>
#include <set>

#ifndef ADDON_VERSION
#define ADDON_VERSION "0.1.0"
#endif

namespace ARBS {
namespace AC {

namespace {

std::string g_pendingSection;
bool        g_pendingAuto = false;

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
	// destinimi sipas listes ligjore
	if (const Json::Value* d = GetCatalog ().FindDestination (p.GetString ("destinimi_kodi"))) {
		p.Set ("destinimi_emri", d->GetString ("name"));
		if (p.GetString ("destinimi").empty ()) {
			std::string t = d->GetString ("name");
			if (!p.GetString ("nen_destinimi").empty ()) t += " \xE2\x80\x93 " + p.GetString ("nen_destinimi");
			if (!p.GetString ("katet").empty ()) t += " " + p.GetString ("katet");
			p.Set ("destinimi", t);
		}
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
	c.Set ("destinations", cat.Destinations ());
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
	const auto parts = BillOfQuantities::ComputeParts (GetCatalog (), model, project, opt);
	const Json::Value pj = BillOfQuantities::PartsToJson (parts, model);
	Json::Value r = req.GetBool ("export", true)
		? SaveAndOpen (project, Reports::QuantitiesAndCost (GetCatalog (), project, ProjectStore::OfficeForReports (), model, opt), true)
		: Ok ();
	if (!r.GetBool ("ok")) return r;
	// "boq" = totali (per projekt me nje objekt = rezultati i plote), "parts" = sipas blloqeve
	if (!pj.GetBool ("multi")) r.Set ("boq", pj["parts"][0]);
	else {
		Json::Value tot = pj["total"];
		tot.Set ("groups", Json::Value::MakeArray ());
		tot.Set ("notes", Json::Value::MakeArray ());
		r.Set ("boq", tot);
		r.Set ("parts", pj["parts"]);
	}
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

Json::Value CmdUnits (const Json::Value& req)
{
	const Json::Value project = ProjectForReports ();
	const Json::Value model = ModelReader::Read (false);
	const Json::Value units = Structure::Units (model, Structure::OptionsFromProject (project), project["njesite_manual"]);
	Json::Value r = req.GetBool ("export", true)
		? SaveAndOpen (project, Reports::UnitSchedule (project, ProjectStore::OfficeForReports (), model), true)
		: Ok ();
	if (!r.GetBool ("ok")) return r;
	r.Set ("units", units["units"]);
	r.Set ("source", units["source"]);
	return r;
}

// blloqet, hyrjet dhe zonat e njesive te gjetura ne model (per paleten)
Json::Value CmdStructureInfo (const Json::Value&)
{
	const Json::Value model = ModelReader::Read (false);
	const Json::Value project = ProjectStore::LoadProject ();
	Json::Value r = Ok ();
	Json::Value blocks = Json::Value::MakeArray ();
	const auto parts = Structure::SplitModel (model, Structure::OptionsFromProject (project));
	for (const auto& p : parts) {
		if (p.first.empty ()) continue;
		Json::Value b = Json::Value::MakeObject ();
		b.Set ("code", p.first);
		b.Set ("name", Structure::BlockName (model, p.first));
		double net = 0.0;
		for (const Json::Value& z : p.second["zones"].AsArray ()) net += z.GetNumber ("area");
		b.Set ("net", net);
		double foot = 0.0;
		for (const Json::Value& bl : p.second["blocks"].AsArray ()) foot += bl.GetNumber ("area");
		b.Set ("area", foot);
		b.Set ("stories", static_cast<int> (p.second["stories"].Size ()));
		blocks.Push (b);
	}
	r.Set ("blocks", blocks);
	r.Set ("entrances", static_cast<int> (model["entrances"].Size ()));
	r.Set ("unitZones", static_cast<int> (model["unitZones"].Size ()));
	r.Set ("rooms", static_cast<int> (model["zones"].Size ()));
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

	const bool custom = req["tasks"].IsArray () && req["tasks"].Size () > 0;
	std::vector<PlanTask> tasks;
	std::vector<PlanBlock> blocks;
	PlanParameters pp = p;
	if (!custom && req.GetBool ("byBlocks", true)) {
		// kompleks me blloqe: blloqet nga zonat "Blloku ..." te modelit
		const Json::Value model = ModelReader::Read (false);
		const Json::Value project = ProjectStore::LoadProject ();
		const StructureOptions so = Structure::OptionsFromProject (project);
		if (Structure::HasBlocks (model)) {
			int below = 0;
			for (const Json::Value& s : model["stories"].AsArray ()) if (s.GetInt ("index") < 0) ++below;
			double common = 0.0, foot = 0.0;
			for (const auto& part : Structure::SplitModel (model, so)) {
				double net = 0.0;
				std::set<int> st;
				for (const Json::Value& z : part.second["zones"].AsArray ()) { net += z.GetNumber ("area"); st.insert (z.GetInt ("story")); }
				if (part.first == Structure::CommonPart ()) { common = net * 1.10; continue; }
				PlanBlock b;
				b.code = part.first;
				b.name = Structure::BlockName (model, part.first);
				const Json::Value& given = project["blloqet_bts"][part.first];
				b.grossArea = given.IsNumber () && given.AsNumber () > 0 ? given.AsNumber () : std::max (50.0, net * 1.20);
				int above = 0;
				for (int x : st) if (x >= 0) ++above;
				b.floorsAbove = std::max (1, above);
				b.floorsBelow = below;
				for (const Json::Value& bl : part.second["blocks"].AsArray ()) foot += bl.GetNumber ("area");
				blocks.push_back (b);
			}
			pp.floorsBelow = below;
			pp.grossArea = std::max (common, foot);
			if (pp.grossArea <= 0.0) pp.grossArea = p.grossArea;
		}
	}
	const int offset = req.GetInt ("offset", 20);
	if (custom) tasks = DynamicPlan::FromJson (req["tasks"]);
	else if (!blocks.empty ()) tasks = DynamicPlan::FromTemplateMulti (cat.PlanTemplate (), pp, blocks, Structure::OptionsFromProject (ProjectStore::LoadProject ()).sharedBasement, offset);
	else tasks = DynamicPlan::FromTemplate (cat.PlanTemplate (), p);
	const std::string err = DynamicPlan::Schedule (tasks, cal, p.start);
	if (!err.empty ()) return Fail (err);

	Json::Value stored = Json::Value::MakeObject ();
	stored.Set ("params", req);
	stored.Set ("tasks", DynamicPlan::ToJson (tasks));
	ProjectStore::SavePlan (stored);

	Json::Value r = Ok ();
	if (req.GetBool ("export", true)) {
		const Json::Value project = ProjectForReports ();
		std::vector<ReportOutput> outs = Reports::DynamicPlanReport (project, ProjectStore::OfficeForReports (), tasks, cal, p);
		// plani i vecante per cdo bllok
		for (const PlanBlock& b : blocks) {
			std::vector<PlanTask> sub;
			for (const PlanTask& t : tasks) if (DynamicPlan::BlockOf (t) == b.code) sub.push_back (t);
			if (sub.empty ()) continue;
			for (const ReportOutput& o : Reports::DynamicPlanReport (project, ProjectStore::OfficeForReports (), sub, cal, p, "-" + Html::Slug (b.code), b.name))
				outs.push_back (o);
		}
		r = SaveAndOpen (project, outs, req.GetBool ("open", true));
	}
	const Date finish = DynamicPlan::ProjectFinish (tasks);
	r.Set ("tasks", DynamicPlan::ToJson (tasks));
	r.Set ("start", p.start.ToIso ());
	r.Set ("finish", finish.ToIso ());
	r.Set ("workingDays", cal.CountWorkingDays (p.start, finish));
	r.Set ("calendarDays", static_cast<int> (finish.ToDays () - p.start.ToDays () + 1));
	r.Set ("holidays", HolidaysJson (cal, p.start, finish));
	Json::Value bl = Json::Value::MakeArray ();
	for (const PlanBlock& b : blocks) {
		Date a, z;
		bool any = false;
		for (const PlanTask& t : tasks) {
			if (DynamicPlan::BlockOf (t) != b.code) continue;
			if (!any || t.es < a) a = t.es;
			if (!any || z < t.ef) z = t.ef;
			any = true;
		}
		if (!any) continue;
		Json::Value o = Json::Value::MakeObject ();
		o.Set ("code", b.code);
		o.Set ("name", b.name);
		o.Set ("start", a.ToIso ());
		o.Set ("finish", z.ToIso ());
		o.Set ("workingDays", cal.CountWorkingDays (a, z));
		bl.Push (o);
	}
	r.Set ("blocks", bl);
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

Json::Value CmdPending (const Json::Value&)
{
	Json::Value r = Ok ();
	r.Set ("section", g_pendingSection);
	r.Set ("auto", g_pendingAuto);
	g_pendingSection.clear ();
	g_pendingAuto = false;
	return r;
}

Json::Value CmdOpen (const Json::Value& req)
{
	std::string path = req.GetString ("path");
	if (path.empty ()) path = LocationToUtf8 (OutputFolder (ProjectStore::LoadProject ()));
	return OpenWithSystem (path) ? Ok () : Fail ("Skedari/dosja nuk u hap: " + path);
}

}	// namespace

void Commands::SetPending (const std::string& section, bool autoRun)
{
	g_pendingSection = section;
	g_pendingAuto = autoRun;
}

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
		{ "units", CmdUnits },
		{ "structureInfo", CmdStructureInfo },
		{ "modelSummary", CmdModelSummary },
		{ "dynamicPlan", CmdDynamicPlan },
		{ "holidays", CmdHolidays },
		{ "open", CmdOpen },
		{ "pending", CmdPending },
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
