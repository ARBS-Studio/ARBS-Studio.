// =============================================================================
// ARBS Studio - gjeneron dokumentet shembull pa Archicad (per kontroll vizual)
// perdorimi: arbs_samples <Data> <dosja-dalese>
// =============================================================================
#include "Core/Catalog.hpp"
#include "Core/DynamicPlan.hpp"
#include "Core/Reports.hpp"

#include <cstdio>
#include <fstream>

using namespace ARBS;

static void Save (const std::string& dir, const ReportOutput& r)
{
	std::ofstream out (dir + "/" + r.fileName, std::ios::binary);
	out << r.content;
	std::printf ("  %s (%zu B)\n", r.fileName.c_str (), r.content.size ());
}

int main (int argc, char** argv)
{
	if (argc < 3) { std::printf ("perdorimi: arbs_samples <Data> <dalja>\n"); return 2; }
	Catalog cat;
	std::string err;
	if (!cat.LoadFromDirectory (argv[1], &err)) { std::printf ("%s\n", err.c_str ()); return 1; }
	const std::string outDir = argv[2];

	Json::Value project = Json::Value::Parse (R"({
		"emri": "Shtëpi banimi individuale", "nr": "2026-014", "investitori": "Arian Krasniqi",
		"destinimi": "Objekt banimi individual B+P+1", "kategoria": "I", "faza": "Projekti Ideor", "faza_kodi": "PI",
		"komuna": "Prishtinë", "komuna_id": "prishtine", "vendi": "Prishtinë", "zona_kadastrale": "Prishtinë",
		"parcela": "P-71914059-01234-0", "sip_parceles": 520, "gjurma": 142.5, "bts": 386, "katet": "B+P+1",
		"lartesia": 8.4, "vrr": 5, "distanca_kufi": 3, "parkingu": "2", "plani": "PZHK Prishtinë",
		"ngrohja": "Ngrohje qendrore me pompë termike ajër-ujë dhe dysheme rrezatuese",
		"konstruksioni": "Skelet b/a (kolona, trarë, pllaka) me mure mbushëse prej blloqesh qeramike",
		"data": "17.10.2026" })");
	Json::Value office = Json::Value::Parse (R"({
		"kompania": "ARBS Studio", "arkitekti": "Emri Mbiemri", "titulli": "Arkitekt", "adresa": "Prishtinë, Kosovë",
		"telefoni": "", "email": "", "web": "", "licenca": "" })");

	Json::Value model = Json::Value::Parse (R"({
		"stories": [ {"index": -1, "name": "Bodrumi", "level": -2.8}, {"index": 0, "name": "Përdhesa", "level": 0}, {"index": 1, "name": "Kati 1", "level": 3.2} ],
		"zones": [
			{"story": -1, "number": "-1.01", "name": "Garazha", "area": 38.6}, {"story": -1, "number": "-1.02", "name": "Depo", "area": 12.4},
			{"story": -1, "number": "-1.03", "name": "Lavanderia / kaldaja", "area": 9.8},
			{"story": 0, "number": "0.01", "name": "Hyrja", "area": 7.2}, {"story": 0, "number": "0.02", "name": "Dhoma e ndenjes", "area": 34.5},
			{"story": 0, "number": "0.03", "name": "Kuzhina me ngrënie", "area": 22.1}, {"story": 0, "number": "0.04", "name": "WC", "area": 3.1},
			{"story": 0, "number": "0.05", "name": "Dhoma e mysafirëve", "area": 12.6}, {"story": 0, "number": "0.06", "name": "Shkallët", "area": 8.4},
			{"story": 1, "number": "1.01", "name": "Dhoma e prindërve", "area": 18.2}, {"story": 1, "number": "1.02", "name": "Garderoba", "area": 5.4},
			{"story": 1, "number": "1.03", "name": "Banjo", "area": 7.6}, {"story": 1, "number": "1.04", "name": "Dhoma e fëmijëve 1", "area": 13.1},
			{"story": 1, "number": "1.05", "name": "Dhoma e fëmijëve 2", "area": 12.8}, {"story": 1, "number": "1.06", "name": "Korridori", "area": 9.3}
		],
		"openings": [
			{"kind": "door", "id": "D-01", "type": "Derë hyrëse alumini", "width": 1.10, "height": 2.20, "sill": 0, "story": 0},
			{"kind": "door", "id": "D-02", "type": "Derë e brendshme druri", "width": 0.90, "height": 2.10, "sill": 0, "story": 0},
			{"kind": "door", "id": "D-03", "type": "Derë e brendshme druri", "width": 0.90, "height": 2.10, "sill": 0, "story": 0},
			{"kind": "door", "id": "D-04", "type": "Derë e brendshme druri", "width": 0.80, "height": 2.10, "sill": 0, "story": 0},
			{"kind": "door", "id": "D-05", "type": "Derë e brendshme druri", "width": 0.90, "height": 2.10, "sill": 0, "story": 1},
			{"kind": "door", "id": "D-06", "type": "Derë e brendshme druri", "width": 0.90, "height": 2.10, "sill": 0, "story": 1},
			{"kind": "door", "id": "D-07", "type": "Derë e brendshme druri", "width": 0.80, "height": 2.10, "sill": 0, "story": 1},
			{"kind": "door", "id": "D-08", "type": "Derë garazhe seksionale", "width": 2.50, "height": 2.25, "sill": 0, "story": -1},
			{"kind": "window", "id": "W-01", "type": "Dritare PVC me dy krahë", "width": 1.40, "height": 1.40, "sill": 0.90, "story": 0},
			{"kind": "window", "id": "W-02", "type": "Dritare PVC me dy krahë", "width": 1.40, "height": 1.40, "sill": 0.90, "story": 0},
			{"kind": "window", "id": "W-03", "type": "Derë-dritare rrëshqitëse", "width": 2.80, "height": 2.30, "sill": 0, "story": 0},
			{"kind": "window", "id": "W-04", "type": "Dritare PVC me një krah", "width": 0.60, "height": 0.60, "sill": 1.60, "story": 0},
			{"kind": "window", "id": "W-05", "type": "Dritare PVC me dy krahë", "width": 1.40, "height": 1.40, "sill": 0.90, "story": 1},
			{"kind": "window", "id": "W-06", "type": "Dritare PVC me dy krahë", "width": 1.40, "height": 1.40, "sill": 0.90, "story": 1},
			{"kind": "window", "id": "W-07", "type": "Dritare PVC me dy krahë", "width": 1.40, "height": 1.40, "sill": 0.90, "story": 1},
			{"kind": "window", "id": "W-08", "type": "Dritare PVC me një krah", "width": 0.60, "height": 0.60, "sill": 1.60, "story": 1}
		],
		"objects": [
			{"type": "Divan këndor", "story": 0}, {"type": "Tavolinë ngrënieje 6 vende", "story": 0}, {"type": "Karrige", "story": 0},
			{"type": "Karrige", "story": 0}, {"type": "Karrige", "story": 0}, {"type": "Karrige", "story": 0}, {"type": "Karrige", "story": 0}, {"type": "Karrige", "story": 0},
			{"type": "WC – shoqë", "story": 0}, {"type": "Lavaman", "story": 0}, {"type": "Krevat dopio 180×200", "story": 1}, {"type": "Krevat teke 90×200", "story": 1},
			{"type": "Krevat teke 90×200", "story": 1}, {"type": "WC – shoqë", "story": 1}, {"type": "Lavaman", "story": 1}, {"type": "Vaskë", "story": 1}, {"type": "Lavatriçe", "story": -1}
		],
		"composites": [
			{"code": "MJ-01", "area": 236.4}, {"code": "MT-01", "area": 92.0}, {"code": "MN-01", "area": 120.0},
			{"code": "DY-01", "area": 61.0}, {"code": "DY-02", "area": 142.5}, {"code": "DY-06", "area": 142.5}, {"code": "KP-02", "area": 168.0}, {"code": "TH-01", "area": 81.5}
		],
		"elements": { "E:column:v": 6.8, "E:beam:v": 9.4, "E:stair:n": 2, "E:roof:eaves:l": 52.0, "E:roof:ridge:l": 14.0 } })");

	std::printf ("Dokumentet:\n");
	Save (outDir, Reports::TechnicalDescription (cat, project, office, model));
	for (const ReportOutput& r : Reports::BuildingPhysics (cat, project, office, model, {})) Save (outDir, r);
	InsolationOptions io;
	io.cityId = "prishtine";
	io.projectNorthDeg = 12.0;
	io.buildingHeight = 8.4;
	for (const ReportOutput& r : Reports::Insolation (cat, project, office, io)) Save (outDir, r);
	for (const ReportOutput& r : Reports::OpeningSchedule (project, office, model, "door")) Save (outDir, r);
	for (const ReportOutput& r : Reports::OpeningSchedule (project, office, model, "window")) Save (outDir, r);
	for (const ReportOutput& r : Reports::InventorySchedule (project, office, model)) Save (outDir, r);
	for (const ReportOutput& r : Reports::ZoneSchedule (project, office, model)) Save (outDir, r);

	BoqOptions bo;
	for (const ReportOutput& r : Reports::QuantitiesAndCost (cat, project, office, model, bo)) Save (outDir, r);
	const BoqResult br = BillOfQuantities::Compute (cat, model, project, bo);
	std::printf ("Parallogaria: punimet %s EUR, pa TVSH %s EUR, me TVSH %s EUR, %s EUR/m2 BTS\n", BillOfQuantities::Money (br.works).c_str (),
				 BillOfQuantities::Money (br.net).c_str (), BillOfQuantities::Money (br.total).c_str (), BillOfQuantities::Money (br.perM2).c_str ());
	for (const BoqGroup& g : br.groups) std::printf ("  %-5s %-45s %12s\n", g.id.c_str (), g.name.c_str (), BillOfQuantities::Money (g.total).c_str ());

	PlanParameters p;
	p.start = Date (2027, 3, 1);
	p.grossArea = 386.0;
	p.floorsAbove = 2;
	p.floorsBelow = 1;
	KosovoCalendar cal;
	cal.LoadRules (cat.Holidays ());
	DynamicPlan::ConfigureCalendar (cal, p);
	std::vector<PlanTask> tasks = DynamicPlan::FromTemplate (cat.PlanTemplate (), p);
	const std::string e = DynamicPlan::Schedule (tasks, cal, p.start);
	if (!e.empty ()) std::printf ("%s\n", e.c_str ());
	for (const ReportOutput& r : Reports::DynamicPlanReport (project, office, tasks, cal, p)) Save (outDir, r);
	return 0;
}
