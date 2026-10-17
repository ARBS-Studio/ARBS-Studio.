// =============================================================================
// ARBS Studio - Core : Catalog.hpp
// Katalogu i te dhenave (Data/*.json): materialet, kompozitat, shtresat, fazat,
// komunat, festat, shablloni i planit dinamik dhe shabllonet e dokumenteve.
// =============================================================================
#pragma once

#include "BuildingPhysics.hpp"
#include "Json.hpp"
#include "Solar.hpp"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace ARBS {

struct City {
	std::string id;
	std::string name;
	double lat = 0.0, lon = 0.0, alt = 0.0;
};

class Catalog {
public:
	static const std::vector<std::string>& FileNames ();	// emrat e skedareve te te dhenave

	bool LoadFromDirectory (const std::string& dir, std::string* error = nullptr);
	bool LoadFromStrings (const std::map<std::string, std::string>& files, std::string* error = nullptr);

	const std::deque<Material>&  Materials () const  { return materials; }
	const std::vector<Composite>& Composites () const { return composites; }
	const std::vector<City>&      Cities () const     { return cities; }
	const Material*  FindMaterial (const std::string& id) const;
	const Composite* FindComposite (const std::string& code) const;
	const City*      FindCity (const std::string& idOrName) const;

	double      UMax (const std::string& key) const;
	std::string UMaxLabel (const std::string& key) const;
	std::string UMaxSource () const { return compositesJson.GetString ("umaxSource"); }

	Site          SiteFor (const std::string& cityId) const;
	GlaserClimate GlaserClimateFor (const std::string& cityId) const;

	const Json::Value& Holidays () const      { return holidaysJson; }
	const Json::Value& PlanTemplate () const  { return planJson; }
	const Json::Value& Phases () const        { return phasesJson; }
	const Json::Value& Layers () const        { return layersJson; }
	const Json::Value& Documents () const     { return documentsJson; }
	const Json::Value& CitiesJson () const    { return citiesJson; }
	const Json::Value& MaterialsJson () const { return materialsJson; }
	const Json::Value& CompositesJson () const { return compositesJson; }

	// Lista e emrave te shrafurave te preferuara per materialin
	std::vector<std::string> FillCandidates (const std::string& materialId) const;

	// Permbledhje per UI (materiale + kompozita me U te llogaritur)
	Json::Value CompositeSummary () const;

private:
	bool Build (std::string* error);

	Json::Value materialsJson, compositesJson, citiesJson, holidaysJson, planJson, phasesJson, layersJson, documentsJson;
	std::deque<Material>   materials;		// deque: adresat mbeten te qendrueshme
	std::vector<Composite> composites;
	std::vector<City>      cities;
	std::map<std::string, std::vector<std::string>> fills;
};

}	// namespace ARBS
