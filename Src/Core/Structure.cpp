// =============================================================================
// ARBS Studio - Core : Structure.cpp
// =============================================================================
#include "Structure.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>

namespace ARBS {

namespace {

// shkronja te vogla + e->e, c->c (UTF-8), per krahasime te thjeshta
std::string Fold (const std::string& s)
{
	std::string r;
	r.reserve (s.size ());
	for (size_t i = 0; i < s.size (); ++i) {
		const unsigned char c = static_cast<unsigned char> (s[i]);
		if (c == 0xC3 && i + 1 < s.size ()) {
			const unsigned char d = static_cast<unsigned char> (s[i + 1]);
			if (d == 0xAB || d == 0x8B) { r += 'e'; ++i; continue; }	// e E
			if (d == 0xA7 || d == 0x87) { r += 'c'; ++i; continue; }	// c C
		}
		r += static_cast<char> (std::tolower (c));
	}
	// hapesirat ne fillim
	size_t p = r.find_first_not_of (" \t");
	return p == std::string::npos ? std::string () : r.substr (p);
}

bool StartsWith (const std::string& s, const char* prefix)
{
	return s.rfind (prefix, 0) == 0;
}

bool Has (const std::string& s, const char* part)
{
	return s.find (part) != std::string::npos;
}

// "blloku b1", "hyrja 2": fjala kyce + emertim (jo vetem fjala, qe "Hyrja" e nje shtepie te mbetet dhome)
bool KeywordWithCode (const std::string& f, std::initializer_list<const char*> words)
{
	for (const char* w : words) {
		if (!StartsWith (f, w)) continue;
		const std::string rest = f.substr (std::string (w).size ());
		const size_t p = rest.find_first_not_of (" \t-_:");
		if (p != std::string::npos && p > 0) return true;
	}
	return false;
}

double Num (const Json::Value& o, const char* key)
{
	const Json::Value& v = o[key];
	if (v.IsNumber ()) return v.AsNumber ();
	if (v.IsString ()) {
		std::string s = v.AsString ();
		for (char& c : s) if (c == ',') c = '.';
		return std::strtod (s.c_str (), nullptr);
	}
	return 0.0;
}

bool InsideZone (const Json::Value& zonePolyOwner, double x, double y)
{
	return Geo::PointInPolygon (Geo::FromJson (zonePolyOwner["poly"]), x, y);
}

}	// namespace

// -----------------------------------------------------------------------------
// Gjeometria
// -----------------------------------------------------------------------------

bool Geo::PointInPolygon (const std::vector<Pt>& poly, double x, double y)
{
	const size_t n = poly.size ();
	if (n < 3) return false;
	bool inside = false;
	for (size_t i = 0, j = n - 1; i < n; j = i++) {
		const Pt& a = poly[i];
		const Pt& b = poly[j];
		if (((a.y > y) != (b.y > y)) && (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x))
			inside = !inside;
	}
	return inside;
}

double Geo::Area (const std::vector<Pt>& poly)
{
	double s = 0.0;
	for (size_t i = 0, n = poly.size (); i < n; ++i) {
		const Pt& a = poly[i];
		const Pt& b = poly[(i + 1) % n];
		s += a.x * b.y - b.x * a.y;
	}
	return std::fabs (s) / 2.0;
}

std::vector<Pt> Geo::FromJson (const Json::Value& arr)
{
	std::vector<Pt> poly;
	for (const Json::Value& p : arr.AsArray ())
		if (p.IsArray () && p.Size () >= 2) poly.push_back ({ p[0].AsNumber (), p[1].AsNumber () });
	// poligoni i Archicad-it e perserit piken e pare ne fund
	if (poly.size () > 3 && std::fabs (poly.front ().x - poly.back ().x) < 1e-9 && std::fabs (poly.front ().y - poly.back ().y) < 1e-9)
		poly.pop_back ();
	return poly;
}

Json::Value Geo::ToJson (const std::vector<Pt>& poly)
{
	Json::Value arr = Json::Value::MakeArray ();
	for (const Pt& p : poly) {
		Json::Value xy = Json::Value::MakeArray ();
		xy.Push (Json::Value (std::round (p.x * 1000.0) / 1000.0));
		xy.Push (Json::Value (std::round (p.y * 1000.0) / 1000.0));
		arr.Push (xy);
	}
	return arr;
}

// -----------------------------------------------------------------------------
// Klasifikimi
// -----------------------------------------------------------------------------

std::string Structure::ZoneRole (const std::string& name, const std::string& category)
{
	const std::string c = Fold (category);
	if (StartsWith (c, "arbs blloku") || StartsWith (c, "arbs bllok") || c == "blloku" || c == "bllok" || c == "block") return "block";
	if (StartsWith (c, "arbs hyrja") || StartsWith (c, "arbs hyrje") || c == "hyrja" || c == "entrance") return "entrance";
	if (StartsWith (c, "arbs njesi") || c == "njesia" || c == "njesi" || c == "unit" || c == "apartment" || c == "apartament") return "unit";

	const std::string n = Fold (name);
	if (KeywordWithCode (n, { "blloku", "bllok", "block", "lamela" })) return "block";
	if (KeywordWithCode (n, { "hyrja", "hyrje", "entrance" })) return "entrance";
	if (StartsWith (n, "banesa") || StartsWith (n, "banese") || StartsWith (n, "njesia") || StartsWith (n, "apartament") ||
		StartsWith (n, "apartment") || StartsWith (n, "lokal") || KeywordWithCode (n, { "zyra", "zyre" }))
		return "unit";
	return "room";
}

std::string Structure::UnitKind (const std::string& name)
{
	const std::string n = Fold (name);
	if (Has (n, "banes") || Has (n, "apartam") || Has (n, "apartment") || Has (n, "stan")) return "banesa";
	if (Has (n, "lokal") || Has (n, "dyqan") || Has (n, "shop") || Has (n, "tregt")) return "lokal";
	if (Has (n, "zyr") || Has (n, "office")) return "zyre";
	if (Has (n, "garazh") || Has (n, "garage")) return "garazh";
	if (Has (n, "parking")) return "parking";
	if (Has (n, "depo") || Has (n, "storage") || Has (n, "bodrum")) return "depo";
	return "tjeter";
}

std::string Structure::UnitKindLabel (const std::string& kind)
{
	if (kind == "banesa") return "Banes\xC3\xAB";
	if (kind == "lokal") return "Lokal afarist";
	if (kind == "zyre") return "Zyr\xC3\xAB";
	if (kind == "garazh") return "Garazh";
	if (kind == "parking") return "Vendparkim";
	if (kind == "depo") return "Depo";
	return "Nj\xC3\xABsi tjet\xC3\xABr";
}

std::string Structure::OuterKind (const std::string& roomName)
{
	const std::string n = Fold (roomName);
	if (Has (n, "lozh") || Has (n, "loggia")) return "lozhe";
	if (Has (n, "teras") || Has (n, "terrace")) return "terase";
	if (Has (n, "ballkon") || Has (n, "balcon") || Has (n, "balkon")) return "ballkon";
	return "";
}

bool Structure::IsBedroom (const std::string& roomName)
{
	const std::string n = Fold (roomName);
	if (Has (n, "gjum") || Has (n, "bedroom") || Has (n, "prinder") || Has (n, "femij")) return true;
	if (Has (n, "dhom") && !Has (n, "ndenj") && !Has (n, "ditor") && !Has (n, "pun") && !Has (n, "ngrenie") && !Has (n, "lavand") &&
		!Has (n, "zhvesh") && !Has (n, "teknik") && !Has (n, "mbeturin"))
		return true;
	return false;
}

StructureOptions Structure::OptionsFromProject (const Json::Value& project)
{
	StructureOptions o;
	const Json::Value& sb = project["bodrumi_perbashket"];
	if (sb.IsBool ()) o.sharedBasement = sb.AsBool ();
	else if (sb.IsString ()) o.sharedBasement = !(sb.AsString () == "jo" || sb.AsString () == "0" || sb.AsString () == "false");
	const double kb = Num (project, "koef_ballkon"), kl = Num (project, "koef_lozhe"), kt = Num (project, "koef_terase");
	if (kb > 0.0) { o.kBalcony = kb; o.coefficientsSet = true; }
	if (kl > 0.0) { o.kLoggia = kl; o.coefficientsSet = true; }
	if (kt > 0.0) { o.kTerrace = kt; o.coefficientsSet = true; }
	return o;
}

// -----------------------------------------------------------------------------
// Caktimi ne blloqe
// -----------------------------------------------------------------------------

std::string Structure::BlockAt (const Json::Value& blocks, double x, double y)
{
	for (const Json::Value& b : blocks.AsArray ())
		if (InsideZone (b, x, y)) return b.GetString ("code");
	return "";
}

std::string Structure::EntranceAt (const Json::Value& entrances, double x, double y)
{
	for (const Json::Value& e : entrances.AsArray ())
		if (InsideZone (e, x, y)) return e.GetString ("code");
	return "";
}

std::string Structure::PartOf (const std::string& block, int story, bool hasBlocks, bool sharedBasement)
{
	if (!hasBlocks) return "";
	if (story < 0 && sharedBasement) return CommonPart ();
	if (block.empty ()) return CommonPart ();
	return block;
}

bool Structure::HasBlocks (const Json::Value& model)
{
	return model["blocks"].IsArray () && model["blocks"].Size () > 0;
}

std::string Structure::BlockName (const Json::Value& model, const std::string& code)
{
	if (code == CommonPart ()) return "Pjesa e p\xC3\xABrbashk\xC3\xABt";
	for (const Json::Value& b : model["blocks"].AsArray ())
		if (b.GetString ("code") == code) return b.GetString ("name", "Blloku " + code);
	return code.empty () ? std::string ("Objekti") : "Blloku " + code;
}

std::vector<std::pair<std::string, Json::Value>> Structure::SplitModel (const Json::Value& model, const StructureOptions& opt)
{
	std::vector<std::pair<std::string, Json::Value>> parts;
	if (!HasBlocks (model)) {
		parts.push_back ({ "", model });
		return parts;
	}
	const Json::Value& blocks = model["blocks"];

	std::vector<std::string> order;
	for (const Json::Value& b : blocks.AsArray ()) {
		const std::string code = b.GetString ("code");
		if (std::find (order.begin (), order.end (), code) == order.end ()) order.push_back (code);
	}
	order.push_back (CommonPart ());

	std::map<std::string, Json::Value> m;
	for (const std::string& code : order) {
		Json::Value p = Json::Value::MakeObject ();
		p.Set ("part", code);
		p.Set ("north", model["north"]);
		for (const char* key : { "zones", "openings", "objects", "composites", "unitZones", "entrances" }) p.Set (key, Json::Value::MakeArray ());
		p.Set ("elements", Json::Value::MakeObject ());
		Json::Value own = Json::Value::MakeArray ();
		for (const Json::Value& b : blocks.AsArray ()) if (b.GetString ("code") == code) own.Push (b);
		p.Set ("blocks", own);
		m[code] = p;
	}

	auto partForXY = [&] (const Json::Value& o) {
		return PartOf (BlockAt (blocks, o.GetNumber ("x"), o.GetNumber ("y")), o.GetInt ("story"), true, opt.sharedBasement);
	};
	std::map<std::string, std::set<int>> storiesUsed;
	for (const char* key : { "zones", "openings", "objects", "unitZones" }) {
		for (const Json::Value& o : model[key].AsArray ()) {
			const std::string part = partForXY (o);
			m[part][key].Push (o);
			if (std::string (key) == "zones") storiesUsed[part].insert (o.GetInt ("story"));
		}
	}
	for (const Json::Value& e : model["entrances"].AsArray ()) {
		const std::string b = BlockAt (blocks, e.GetNumber ("x"), e.GetNumber ("y"));
		m[b.empty () ? std::string (CommonPart ()) : b]["entrances"].Push (e);
	}

	// kompozitat dhe elementet: ModelReader i jep te ndara sipas bllokut dhe nentokes
	std::map<std::string, std::map<std::string, double>> compArea;
	for (const Json::Value& c : model["compositeParts"].AsArray ()) {
		const std::string part = PartOf (c.GetString ("block"), c.GetBool ("below") ? -1 : 0, true, opt.sharedBasement);
		compArea[part][c.GetString ("code")] += c.GetNumber ("area");
	}
	for (auto& kv : compArea)
		for (auto& ca : kv.second) {
			Json::Value c = Json::Value::MakeObject ();
			c.Set ("code", ca.first);
			c.Set ("area", std::round (ca.second * 10.0) / 10.0);
			m[kv.first]["composites"].Push (c);
		}
	for (const Json::Value& e : model["elementParts"].AsArray ()) {
		const std::string part = PartOf (e.GetString ("block"), e.GetBool ("below") ? -1 : 0, true, opt.sharedBasement);
		Json::Value& dst = m[part]["elements"];
		for (const auto& kv : e["values"].AsObject ())
			if (kv.second.IsNumber ()) dst.Set (kv.first, dst.GetNumber (kv.first) + kv.second.AsNumber ());
	}

	// katet e pjeses (per kohezgjatjet, bodrumin dhe pershkrimin)
	for (const std::string& code : order) {
		Json::Value st = Json::Value::MakeArray ();
		for (const Json::Value& s : model["stories"].AsArray ()) {
			const int idx = s.GetInt ("index");
			const bool used = storiesUsed[code].count (idx) > 0;
			if (code == CommonPart () ? (idx < 0 && used) : used) st.Push (s);
		}
		m[code].Set ("stories", st);
	}

	for (const std::string& code : order) {
		const Json::Value& p = m[code];
		const bool empty = p["zones"].Size () == 0 && p["composites"].Size () == 0 && p["elements"].AsObject ().empty () && p["openings"].Size () == 0;
		if (code == CommonPart () && empty) continue;
		parts.push_back ({ code, p });
	}
	return parts;
}

// -----------------------------------------------------------------------------
// Njesite
// -----------------------------------------------------------------------------

std::vector<int> Structure::ParseFloors (const std::string& specIn)
{
	std::vector<int> floors;
	std::string spec = Fold (specIn);
	for (char& c : spec) if (c == ';') c = ',';
	size_t pos = 0;
	auto toInt = [] (std::string t) -> int {
		size_t a = t.find_first_not_of (" "), b = t.find_last_not_of (" ");
		if (a == std::string::npos) return -9999;
		t = t.substr (a, b - a + 1);
		if (t == "p" || t == "perdhesa") return 0;
		if (t == "b" || t == "bodrumi") return -1;
		char* end = nullptr;
		const long v = std::strtol (t.c_str (), &end, 10);
		return (end == t.c_str ()) ? -9999 : static_cast<int> (v);
	};
	while (pos <= spec.size ()) {
		size_t comma = spec.find (',', pos);
		if (comma == std::string::npos) comma = spec.size ();
		const std::string tok = spec.substr (pos, comma - pos);
		const size_t dash = tok.find ('-', tok.find_first_not_of (" ") == std::string::npos ? 0 : tok.find_first_not_of (" ") + 1);
		if (dash != std::string::npos) {
			const int a = toInt (tok.substr (0, dash)), b = toInt (tok.substr (dash + 1));
			if (a != -9999 && b != -9999) for (int f = std::min (a, b); f <= std::max (a, b) && f - std::min (a, b) < 200; ++f) floors.push_back (f);
		} else {
			const int a = toInt (tok);
			if (a != -9999) floors.push_back (a);
		}
		pos = comma + 1;
	}
	std::sort (floors.begin (), floors.end ());
	floors.erase (std::unique (floors.begin (), floors.end ()), floors.end ());
	return floors;
}

Json::Value Structure::ManualUnits (const Json::Value& manual, const StructureOptions& opt)
{
	std::map<std::string, const Json::Value*> types;
	for (const Json::Value& t : manual["types"].AsArray ()) types[t.GetString ("code")] = &t;

	std::vector<Json::Value> units;
	std::map<std::string, int> counter;		// numerimi i njesive per bllok+hyrje
	for (const Json::Value& pl : manual["placements"].AsArray ()) {
		auto it = types.find (pl.GetString ("type"));
		if (it == types.end ()) continue;
		const Json::Value& t = *it->second;
		const std::string block = pl.GetString ("block");
		const std::string entrance = pl.GetString ("entrance", "H1");
		const int perFloor = std::max (1, pl.GetInt ("perFloor", 1));
		const std::string kind = t.GetString ("kind", "banesa");
		for (int floor : ParseFloors (pl.GetString ("floors", "1"))) {
			for (int k = 0; k < perFloor; ++k) {
				const std::string key = block + "/" + entrance;
				const int no = ++counter[key];
				Json::Value o = Json::Value::MakeObject ();
				o.Set ("number", (block.empty () ? std::string () : block + "-") + entrance + "-" + std::to_string (no));
				o.Set ("name", t.GetString ("name", "Tipi " + t.GetString ("code")));
				o.Set ("type", t.GetString ("code"));
				o.Set ("kind", kind);
				o.Set ("kindLabel", UnitKindLabel (kind));
				o.Set ("story", floor);
				o.Set ("block", block);
				o.Set ("entrance", entrance);
				o.Set ("gross", t.GetNumber ("gross"));
				o.Set ("manual", true);
				Json::Value rj = Json::Value::MakeArray ();
				double inner = 0.0, outerRaw = 0.0, outerRed = 0.0;
				int bedrooms = 0;
				for (const Json::Value& r : t["rooms"].AsArray ()) {
					const double a = Num (r, "area");
					const std::string outer = OuterKind (r.GetString ("name"));
					const double kk = outer == "ballkon" ? opt.kBalcony : outer == "lozhe" ? opt.kLoggia : outer == "terase" ? opt.kTerrace : 1.0;
					Json::Value ro = Json::Value::MakeObject ();
					ro.Set ("name", r.GetString ("name"));
					ro.Set ("area", a);
					ro.Set ("outer", outer);
					ro.Set ("k", kk);
					ro.Set ("reduced", std::round (a * kk * 100.0) / 100.0);
					rj.Push (ro);
					if (outer.empty ()) { inner += a; if (IsBedroom (r.GetString ("name"))) ++bedrooms; }
					else { outerRaw += a; outerRed += a * kk; }
				}
				o.Set ("rooms", rj);
				o.Set ("inner", std::round (inner * 100.0) / 100.0);
				o.Set ("outerRaw", std::round (outerRaw * 100.0) / 100.0);
				o.Set ("outerReduced", std::round (outerRed * 100.0) / 100.0);
				o.Set ("net", std::round ((inner + outerRed) * 100.0) / 100.0);
				o.Set ("bedrooms", bedrooms);
				o.Set ("structure", kind == "banesa" ? (bedrooms == 0 ? std::string ("Garsonier\xC3\xAB") : std::to_string (bedrooms) + "+1") : std::string ());
				units.push_back (o);
			}
		}
	}
	// hapesirat e perbashketa me dore: { "common": { "B1": 120.5 } }
	std::map<std::string, double> netByBlock;
	for (const Json::Value& u : units) netByBlock[u.GetString ("block")] += u.GetNumber ("net");
	for (Json::Value& u : units) {
		const std::string b = u.GetString ("block");
		const double common = Num (manual["common"], b.c_str ());
		const double share = netByBlock[b] > 0.0 ? common * u.GetNumber ("net") / netByBlock[b] : 0.0;
		u.Set ("commonShare", std::round (share * 100.0) / 100.0);
		u.Set ("withCommon", std::round ((u.GetNumber ("net") + share) * 100.0) / 100.0);
	}
	Json::Value out = Json::Value::MakeObject ();
	Json::Value arr = Json::Value::MakeArray ();
	for (const Json::Value& u : units) arr.Push (u);
	out.Set ("units", arr);
	out.Set ("commonByBlock", manual["common"].IsObject () ? manual["common"] : Json::Value::MakeObject ());
	out.Set ("source", "manual");
	return out;
}

Json::Value Structure::Units (const Json::Value& model, const StructureOptions& opt, const Json::Value& manual)
{
	if (model["unitZones"].Size () == 0 && manual["placements"].Size () > 0)
		return ManualUnits (manual, opt);

	const Json::Value& blocks = model["blocks"];
	const bool hasBlocks = HasBlocks (model);

	struct RoomRef { const Json::Value* z; bool used = false; };
	std::vector<RoomRef> rooms;
	for (const Json::Value& z : model["zones"].AsArray ()) rooms.push_back ({ &z, false });

	std::vector<Json::Value> units;
	for (const Json::Value& u : model["unitZones"].AsArray ()) {
		const std::vector<Pt> poly = Geo::FromJson (u["poly"]);
		const int story = u.GetInt ("story");
		Json::Value o = Json::Value::MakeObject ();
		const std::string number = u.GetString ("number");
		const std::string kind = UnitKind (u.GetString ("name"));
		o.Set ("number", number);
		o.Set ("name", u.GetString ("name"));
		o.Set ("kind", kind);
		o.Set ("kindLabel", UnitKindLabel (kind));
		o.Set ("story", story);
		const std::string block = hasBlocks ? BlockAt (blocks, u.GetNumber ("x"), u.GetNumber ("y")) : std::string ();
		o.Set ("block", block);
		std::string entrance = EntranceAt (model["entrances"], u.GetNumber ("x"), u.GetNumber ("y"));
		if (entrance.empty ()) {
			// numri i njesise me prefiks hyrjeje: "H2-3.04"
			const std::string f = Fold (number);
			if (f.size () >= 2 && f[0] == 'h' && std::isdigit (static_cast<unsigned char> (f[1]))) {
				size_t e = 1;
				while (e < f.size () && std::isdigit (static_cast<unsigned char> (f[e]))) ++e;
				entrance = "H" + f.substr (1, e - 1);
			}
		}
		o.Set ("entrance", entrance.empty () ? std::string ("H1") : entrance);
		o.Set ("gross", std::round (u.GetNumber ("area") * 100.0) / 100.0);

		Json::Value rj = Json::Value::MakeArray ();
		double inner = 0.0, outerRaw = 0.0, outerRed = 0.0;
		int bedrooms = 0;
		for (RoomRef& r : rooms) {
			if (r.used) continue;
			const Json::Value& z = *r.z;
			if (z.GetInt ("story") != story) continue;
			if (!Geo::PointInPolygon (poly, z.GetNumber ("x"), z.GetNumber ("y"))) continue;
			r.used = true;
			const double a = z.GetNumber ("area");
			const std::string outer = OuterKind (z.GetString ("name"));
			const double k = outer == "ballkon" ? opt.kBalcony : outer == "lozhe" ? opt.kLoggia : outer == "terase" ? opt.kTerrace : 1.0;
			Json::Value ro = Json::Value::MakeObject ();
			ro.Set ("number", z.GetString ("number"));
			ro.Set ("name", z.GetString ("name"));
			ro.Set ("area", a);
			ro.Set ("outer", outer);
			ro.Set ("k", k);
			ro.Set ("reduced", std::round (a * k * 100.0) / 100.0);
			rj.Push (ro);
			if (outer.empty ()) { inner += a; if (IsBedroom (z.GetString ("name"))) ++bedrooms; }
			else { outerRaw += a; outerRed += a * k; }
		}
		o.Set ("rooms", rj);
		o.Set ("inner", std::round (inner * 100.0) / 100.0);
		o.Set ("outerRaw", std::round (outerRaw * 100.0) / 100.0);
		o.Set ("outerReduced", std::round (outerRed * 100.0) / 100.0);
		o.Set ("net", std::round ((inner + outerRed) * 100.0) / 100.0);
		o.Set ("bedrooms", bedrooms);
		o.Set ("structure", kind == "banesa" ? (bedrooms == 0 ? std::string ("Garsonier\xC3\xAB") : std::to_string (bedrooms) + "+1") : std::string ());
		units.push_back (o);
	}

	// hapesirat e perbashketa (dhoma jashte njesive, mbi toke ose ne bodrum jo te perbashket) sipas bllokut
	std::map<std::string, double> commonByBlock, netByBlock;
	for (const RoomRef& r : rooms) {
		if (r.used || model["unitZones"].Size () == 0) continue;
		const Json::Value& z = *r.z;
		if (z.GetInt ("story") < 0 && opt.sharedBasement) continue;
		const std::string b = hasBlocks ? BlockAt (blocks, z.GetNumber ("x"), z.GetNumber ("y")) : std::string ();
		commonByBlock[b] += z.GetNumber ("area");
	}
	for (const Json::Value& u : units) netByBlock[u.GetString ("block")] += u.GetNumber ("net");
	for (Json::Value& u : units) {
		const std::string b = u.GetString ("block");
		const double share = netByBlock[b] > 0.0 ? commonByBlock[b] * u.GetNumber ("net") / netByBlock[b] : 0.0;
		u.Set ("commonShare", std::round (share * 100.0) / 100.0);
		u.Set ("withCommon", std::round ((u.GetNumber ("net") + share) * 100.0) / 100.0);
	}

	std::stable_sort (units.begin (), units.end (), [] (const Json::Value& a, const Json::Value& b) {
		if (a.GetString ("block") != b.GetString ("block")) return a.GetString ("block") < b.GetString ("block");
		if (a.GetString ("entrance") != b.GetString ("entrance")) return a.GetString ("entrance") < b.GetString ("entrance");
		if (a.GetInt ("story") != b.GetInt ("story")) return a.GetInt ("story") < b.GetInt ("story");
		return a.GetString ("number") < b.GetString ("number");
	});

	Json::Value out = Json::Value::MakeObject ();
	Json::Value arr = Json::Value::MakeArray ();
	for (const Json::Value& u : units) arr.Push (u);
	out.Set ("units", arr);
	Json::Value common = Json::Value::MakeObject ();
	for (const auto& kv : commonByBlock) common.Set (kv.first.empty () ? std::string ("-") : kv.first, std::round (kv.second * 100.0) / 100.0);
	out.Set ("commonByBlock", common);
	out.Set ("source", "model");
	return out;
}

}	// namespace ARBS
