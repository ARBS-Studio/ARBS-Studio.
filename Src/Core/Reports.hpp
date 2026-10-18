// =============================================================================
// ARBS Studio - Core : Reports.hpp
// Gjeneruesit e dokumenteve ne shqip: HTML (printim ne PDF), Word (.doc HTML),
// CSV per Excel (UTF-8 me BOM, ndares ";", presje dhjetore).
//
// Te dhenat hyrese jane Json::Value:
//   project : fushat nga documents.json/projectFields (emri, investitori, komuna ...)
//   office  : profili i zyres (kompania, arkitekti, titulli, adresa, telefoni, email,
//             web, projektuesit[], licenca, logo (data URI))
//   model   : te dhenat e lexuara nga Archicad:
//     { "stories":[{index,name,level}],
//       "zones":[{story,number,name,area,category}],
//       "openings":[{kind:"door"|"window", id, type, width, height, sill, story}],
//       "objects":[{type, story, id}],
//       "composites":[{code, name, usage, area}] }
// =============================================================================
#pragma once

#include "BillOfQuantities.hpp"
#include "Catalog.hpp"
#include "DynamicPlan.hpp"
#include "Json.hpp"
#include "KosovoCalendar.hpp"
#include "Solar.hpp"

#include <string>
#include <vector>

namespace ARBS {

namespace Html {
	std::string Esc (const std::string& s);
	std::string Num (double v, int decimals = 2);		// 0,29 (presje dhjetore)
	std::string Slug (const std::string& s);			// per emra skedaresh (pa diakritike)
	std::string Fill (const std::string& tmpl, const Json::Value& project, const Json::Value& office);
}

struct ReportOutput {
	std::string fileName;		// emri i sugjeruar i skedarit (pa dosje)
	std::string content;		// UTF-8
	std::string mime;			// "text/html", "application/msword", "text/csv"
};

struct InsolationOptions {
	std::string cityId = "prishtine";
	double latitude = 0.0, longitude = 0.0, altitude = 0.0;	// 0 = merr nga komuna
	double projectNorthDeg = 0.0;	// rrotullimi i veriut te projektit (grade, kunder akrepave)
	double buildingHeight = 9.0;	// m, per hijet
	int    year = 2026;
	double minSunHours = 2.0;		// kriteri i projektuesit per hapesirat e banimit
};

class Reports {
public:
	static std::string Document (const std::string& title, const std::string& body,
								 const Json::Value& project, const Json::Value& office,
								 bool forWord, bool landscape = false);

	static ReportOutput TechnicalDescription (const Catalog& cat, const Json::Value& project,
											  const Json::Value& office, const Json::Value& model);

	static std::vector<ReportOutput> BuildingPhysics (const Catalog& cat, const Json::Value& project,
													  const Json::Value& office, const Json::Value& model,
													  const std::vector<std::string>& codes);

	static std::vector<ReportOutput> Insolation (const Catalog& cat, const Json::Value& project,
												 const Json::Value& office, const InsolationOptions& opt);

	static std::vector<ReportOutput> OpeningSchedule (const Json::Value& project, const Json::Value& office,
													  const Json::Value& model, const std::string& kind);

	static std::vector<ReportOutput> InventorySchedule (const Json::Value& project, const Json::Value& office,
														const Json::Value& model);

	static std::vector<ReportOutput> ZoneSchedule (const Json::Value& project, const Json::Value& office,
												   const Json::Value& model);

	static std::vector<ReportOutput> DynamicPlanReport (const Json::Value& project, const Json::Value& office,
														const std::vector<PlanTask>& tasks,
														const KosovoCalendar& cal, const PlanParameters& params);

	// Paramasa dhe parallogaria (HTML + Word), paramasa per ofertim (Word) dhe CSV per Excel
	static std::vector<ReportOutput> QuantitiesAndCost (const Catalog& cat, const Json::Value& project, const Json::Value& office,
														const Json::Value& model, const BoqOptions& opt);

	static std::string SunPathSvg (const Site& site, int year, double projectNorthDeg, int size = 520);
	static std::string GlaserSvg (const GlaserResult& g, int width = 640, int height = 300);
	static std::string CompositeSectionSvg (const Composite& c, int width = 640, int height = 120);
	static std::string OpeningElevationSvg (const std::string& kind, double width, double height, double sill);
};

}	// namespace ARBS
