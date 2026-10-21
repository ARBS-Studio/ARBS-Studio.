// =============================================================================
// ARBS Studio - Core : Catalog.cpp
// =============================================================================
#include "Catalog.hpp"

#include <fstream>
#include <sstream>

namespace ARBS {

const std::vector<std::string>& Catalog::FileNames ()
{
	static const std::vector<std::string> names = {
		"materials.json", "composites.json", "cities.json", "holidays.json",
		"plan_template.json", "phases.json", "layers.json", "documents.json", "prices.json",
		"destinations.json"
	};
	return names;
}

bool Catalog::LoadFromDirectory (const std::string& dir, std::string* error)
{
	std::map<std::string, std::string> files;
	for (const std::string& name : FileNames ()) {
		std::ifstream in (dir + "/" + name, std::ios::binary);
		if (!in) {
			if (error) *error = "Mungon skedari " + dir + "/" + name;
			return false;
		}
		std::stringstream ss;
		ss << in.rdbuf ();
		files[name] = ss.str ();
	}
	return LoadFromStrings (files, error);
}

bool Catalog::LoadFromStrings (const std::map<std::string, std::string>& files, std::string* error)
{
	auto parse = [&] (const std::string& name, Json::Value& target) -> bool {
		auto it = files.find (name);
		if (it == files.end ()) { if (error) *error = "Mungon " + name; return false; }
		try {
			target = Json::Value::Parse (it->second);
		} catch (const Json::ParseError& e) {
			if (error) *error = name + ": " + e.what ();
			return false;
		}
		return true;
	};
	if (!parse ("materials.json", materialsJson) || !parse ("composites.json", compositesJson) ||
		!parse ("cities.json", citiesJson) || !parse ("holidays.json", holidaysJson) ||
		!parse ("plan_template.json", planJson) || !parse ("phases.json", phasesJson) ||
		!parse ("layers.json", layersJson) || !parse ("documents.json", documentsJson) ||
		!parse ("prices.json", pricesJson) || !parse ("destinations.json", destinationsJson))
		return false;
	return Build (error);
}

static HeatFlow ParseFlow (const std::string& s)
{
	if (s == "upward")   return HeatFlow::Upward;
	if (s == "downward") return HeatFlow::Downward;
	return HeatFlow::Horizontal;
}

static BoundaryType ParseBoundary (const std::string& s)
{
	if (s == "ground")   return BoundaryType::Ground;
	if (s == "unheated") return BoundaryType::Unheated;
	if (s == "heated")   return BoundaryType::Heated;
	return BoundaryType::Exterior;
}

bool Catalog::Build (std::string* error)
{
	materials.clear ();
	composites.clear ();
	cities.clear ();
	fills.clear ();

	for (const Json::Value& m : materialsJson["materials"].AsArray ()) {
		materials.push_back (Material::FromJson (m));
		std::vector<std::string>& f = fills[materials.back ().id];
		if (m["fill"].IsString ()) f.push_back (m["fill"].AsString ());
		for (const Json::Value& name : m["fill"].AsArray ()) f.push_back (name.AsString ());
		if (!f.empty ()) materials.back ().fill = f.front ();
	}

	for (const Json::Value& c : compositesJson["composites"].AsArray ()) {
		Composite comp;
		comp.code = c.GetString ("code");
		comp.name = c.GetString ("name");
		comp.usage = c.GetString ("usage", "wall");
		comp.umaxKey = c.GetString ("umax");
		comp.element = comp.umaxKey;
		comp.description = c.GetString ("description");
		comp.flow = ParseFlow (c.GetString ("flow", "horizontal"));
		comp.boundary = ParseBoundary (c.GetString ("boundary", "exterior"));
		comp.insideIsLast = !c.GetBool ("insideFirst", false);
		if (!c.GetBool ("envelope", true) && comp.boundary == BoundaryType::Exterior)
			comp.boundary = BoundaryType::Unheated;	// jashte mbeshtjellesit: pa kontroll Glaser
		for (const Json::Value& l : c["layers"].AsArray ()) {
			Layer layer;
			layer.material = FindMaterial (l.GetString ("m"));
			if (layer.material == nullptr) {
				if (error) *error = "Kompozita " + comp.code + ": materiali i panjohur " + l.GetString ("m");
				return false;
			}
			layer.thickness = l.GetNumber ("d") / 1000.0;
			layer.core = l.GetBool ("core", false);
			layer.finish = l.GetBool ("finish", false);
			layer.ventilated = l.GetBool ("vent", false);
			comp.layers.push_back (layer);
		}
		composites.push_back (comp);
	}

	for (const Json::Value& c : citiesJson["cities"].AsArray ()) {
		City city;
		city.id = c.GetString ("id");
		city.name = c.GetString ("name");
		city.lat = c.GetNumber ("lat");
		city.lon = c.GetNumber ("lon");
		city.alt = c.GetNumber ("alt");
		cities.push_back (city);
	}
	return true;
}

const Material* Catalog::FindMaterial (const std::string& id) const
{
	for (const Material& m : materials)
		if (m.id == id) return &m;
	return nullptr;
}

const Json::Value* Catalog::FindDestination (const std::string& code) const
{
	for (const Json::Value& d : destinationsJson["destinations"].AsArray ())
		if (d.GetString ("code") == code || d.GetString ("name") == code) return &d;
	return nullptr;
}

const Composite* Catalog::FindComposite (const std::string& code) const
{
	for (const Composite& c : composites)
		if (c.code == code) return &c;
	return nullptr;
}

const City* Catalog::FindCity (const std::string& key) const
{
	for (const City& c : cities)
		if (c.id == key || c.name == key) return &c;
	return nullptr;
}

double Catalog::UMax (const std::string& key) const
{
	if (key.empty ()) return 0.0;
	return compositesJson["umax"][key].GetNumber ("value", 0.0);
}

std::string Catalog::UMaxLabel (const std::string& key) const
{
	if (key.empty ()) return "";
	return compositesJson["umax"][key].GetString ("label");
}

Site Catalog::SiteFor (const std::string& cityId) const
{
	Site s;
	const City* c = FindCity (cityId);
	if (c == nullptr && !cities.empty ()) c = &cities.front ();
	if (c != nullptr) {
		s.name = c->name;
		s.latitude = c->lat;
		s.longitude = c->lon;
		s.altitude = c->alt;
	}
	s.baseUtcOffset = citiesJson["timezone"].GetNumber ("baseUtcOffset", 1.0);
	s.useEuDst = citiesJson["timezone"].GetBool ("euDst", true);
	return s;
}

GlaserClimate Catalog::GlaserClimateFor (const std::string& /*cityId*/) const
{
	GlaserClimate g;
	const Json::Value& d = citiesJson["glaser"]["default"];
	g.thetaI = d.GetNumber ("thetaI", g.thetaI);
	g.phiI = d.GetNumber ("phiI", g.phiI);
	g.thetaE = d.GetNumber ("thetaE", g.thetaE);
	g.phiE = d.GetNumber ("phiE", g.phiE);
	g.condDays = d.GetNumber ("condDays", g.condDays);
	g.dryTheta = d.GetNumber ("dryTheta", g.dryTheta);
	g.dryPhi = d.GetNumber ("dryPhi", g.dryPhi);
	g.dryDays = d.GetNumber ("dryDays", g.dryDays);
	return g;
}

std::vector<std::string> Catalog::FillCandidates (const std::string& materialId) const
{
	auto it = fills.find (materialId);
	return it != fills.end () ? it->second : std::vector<std::string> ();
}

Json::Value Catalog::CompositeSummary () const
{
	Json::Value arr = Json::Value::MakeArray ();
	const GlaserClimate climate = GlaserClimateFor ("");
	for (const Composite& c : composites) {
		const UResult u = BuildingPhysics::ComputeU (c, UMax (c.umaxKey));
		const GlaserResult g = BuildingPhysics::Glaser (c, climate);
		Json::Value o = Json::Value::MakeObject ();
		o.Set ("code", c.code);
		o.Set ("name", c.name);
		o.Set ("usage", c.usage);
		o.Set ("description", c.description);
		o.Set ("thickness", c.TotalThickness () * 1000.0);
		o.Set ("u", u.u);
		o.Set ("umax", u.umax);
		o.Set ("umaxLabel", UMaxLabel (c.umaxKey));
		o.Set ("compliant", u.compliant);
		o.Set ("condensation", g.condensation);
		o.Set ("glaserOk", g.ok);
		Json::Value layers = Json::Value::MakeArray ();
		for (const Layer& l : c.layers) {
			Json::Value lo = Json::Value::MakeObject ();
			lo.Set ("material", l.material->name);
			lo.Set ("group", l.material->group);
			lo.Set ("d", l.thickness * 1000.0);
			lo.Set ("lambda", l.material->lambda);
			lo.Set ("core", l.core);
			lo.Set ("vent", l.ventilated);
			layers.Push (lo);
		}
		o.Set ("layers", layers);
		arr.Push (o);
	}
	return arr;
}

}	// namespace ARBS
