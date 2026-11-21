// =============================================================================
// ARBS Studio - Core : BillOfQuantities.cpp
// =============================================================================
#include "BillOfQuantities.hpp"

#include "Reports.hpp"
#include "Xlsx.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>

namespace ARBS {

namespace {

double NumField (const Json::Value& o, const std::string& key)
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

std::string Lower (const std::string& s)
{
	std::string r = s;
	for (char& c : r) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
	return r;
}

bool ContainsAny (const std::string& lowerText, std::initializer_list<const char*> words)
{
	for (const char* w : words)
		if (lowerText.find (w) != std::string::npos) return true;
	return false;
}

double Round (double v, int decimals)
{
	const double f = std::pow (10.0, decimals);
	return std::round (v * f) / f;
}

int UnitDecimals (const std::string& unit)
{
	return (unit == "kg" || unit == "cop\xC3\xAB") ? 0 : 2;
}

std::string SourceLabel (const std::string& key, const std::map<std::string, std::set<std::string>>& sources)
{
	auto it = sources.find (key);
	if (it != sources.end () && !it->second.empty ()) {
		std::string s;
		for (const std::string& c : it->second) s += (s.empty () ? "" : ", ") + c;
		return s;
	}
	if (key.rfind ("Z:wet", 0) == 0) return "nyjet sanitare (zonat)";
	if (key.rfind ("Z:", 0) == 0) return "zonat";
	if (key.rfind ("O:window", 0) == 0) return "dritaret";
	if (key.rfind ("O:door", 0) == 0) return "dyert";
	if (key.rfind ("E:column", 0) == 0) return "kolonat";
	if (key.rfind ("E:beam", 0) == 0) return "trar\xC3\xABt";
	if (key.rfind ("E:stair", 0) == 0) return "shkall\xC3\xABt";
	if (key.rfind ("E:roof", 0) == 0) return "kulmet";
	if (key.rfind ("E:", 0) == 0) return "elementet bazike";
	if (key.rfind ("I:", 0) == 0) return "objektet sanitare";
	if (key == "BTS") return "BTS";
	if (key == "GJ") return "gjurma";
	if (key == "EXC:v") return "g\xC3\xABrmimi";
	return "";
}

}	// namespace

std::string BillOfQuantities::Money (double v)
{
	const bool neg = v < 0.0;
	const long long cents = static_cast<long long> (std::llround (std::fabs (v) * 100.0));
	std::string whole = std::to_string (cents / 100);
	std::string grouped;
	int count = 0;
	for (size_t i = whole.size (); i > 0; --i) {
		grouped.insert (grouped.begin (), whole[i - 1]);
		if (++count % 3 == 0 && i > 1) grouped.insert (grouped.begin (), '.');
	}
	char dec[8];
	std::snprintf (dec, sizeof (dec), ",%02lld", cents % 100);
	return (neg && cents > 0 ? "-" : "") + grouped + dec;
}

std::map<std::string, double> BillOfQuantities::ModelQuantities (const Catalog& cat, const Json::Value& model, const Json::Value& project,
																 std::map<std::string, std::set<std::string>>* sourcesOut,
																 std::vector<std::string>* notes)
{
	std::map<std::string, double> q;
	std::map<std::string, std::set<std::string>> sources;
	auto add = [&] (const std::string& key, double v, const std::string& src) {
		q[key] += v;
		if (!src.empty ()) sources[key].insert (src);
	};

	// --- kompozitat: siperfaqja e elementit x shtresat e katalogut
	for (const Json::Value& mc : model["composites"].AsArray ()) {
		const std::string code = mc.GetString ("code");
		const double area = mc.GetNumber ("area");
		if (area <= 0.0) continue;
		const Composite* c = cat.FindComposite (code);
		if (c == nullptr) {
			if (notes) notes->push_back ("Kompozita " + code + " (" + Html::Num (area, 1) + " m\xC2\xB2) nuk \xC3\xABsht\xC3\xAB n\xC3\xAB katalogun ARBS dhe nuk \xC3\xABsht\xC3\xAB p\xC3\xABrfshir\xC3\xAB.");
			continue;
		}
		const std::string grp = code.substr (0, code.find ('-'));
		add ("C:" + code + ":a", area, code);
		add ("C:" + grp + ":a", area, code);
		add ("U:" + c->usage + ":a", area, code);
		for (const Layer& l : c->layers) {
			if (l.material == nullptr) continue;
			const std::string id = l.material->id;
			for (const std::string& g : { grp, code, std::string ("*") }) {
				add ("L:" + id + ":" + g + ":a", area, code);
				add ("L:" + id + ":" + g + ":v", area * l.thickness, code);
			}
		}
	}
	for (const Json::Value& u : model["unknownComposites"].AsArray ())
		if (notes) notes->push_back ("Kompozita \xC2\xAB" + u.GetString ("name") + "\xC2\xBB (" + Html::Num (u.GetNumber ("area"), 1) +
									 " m\xC2\xB2) nuk ka kod ARBS (p.sh. \xC2\xABMJ-01 \xE2\x80\xA6\xC2\xBB) dhe nuk \xC3\xABsht\xC3\xAB p\xC3\xABrfshir\xC3\xAB.");

	// --- elementet bazike dhe gjatesite e kulmit (nga ModelReader)
	if (model["elements"].IsObject ())
		for (const auto& kv : model["elements"].AsObject ())
			if (kv.second.IsNumber ()) q[kv.first] += kv.second.AsNumber ();

	// --- zonat
	bool basement = false;
	double groundZones = 0.0;
	for (const Json::Value& z : model["zones"].AsArray ()) {
		const double a = z.GetNumber ("area");
		add ("Z:a", a, "");
		add ("Z:n", 1.0, "");
		if (z.GetInt ("story") < 0) basement = true;
		if (z.GetInt ("story") == 0) groundZones += a;
		const std::string n = Lower (z.GetString ("name"));
		if (ContainsAny (n, { "banj", "wc", "tualet", "dush", "lavand", "sanitar", "bath", "toilet", "shower", "laundry" })) {
			add ("Z:wet:a", a, "");
			add ("Z:wet:n", 1.0, "");
		}
	}
	for (const Json::Value& s : model["stories"].AsArray ())
		if (s.GetInt ("index") < 0) basement = true;

	// --- hapjet (gjithsej dhe sipas llojit nga modeli)
	auto doorClass = [] (const std::string& type) -> const char* {
		const std::string t = Lower (type);
		return ContainsAny (t, { "garazh", "garage", "seksional", "sectional", "rolling", "overhead" }) ? "gar"
			 : ContainsAny (t, { "hyr", "jasht", "entrance", "entry", "exterior", "external", "front", "sigurie", "security" }) ? "ext" : "int";
	};
	auto typeName = [] (const Json::Value& o, const char* fallback) {
		std::string t = o.GetString ("type");
		for (char& c : t) if (c == ':') c = '-';
		return t.empty () ? std::string (fallback) : t;
	};
	for (const Json::Value& o : model["openings"].AsArray ()) {
		const double w = o.GetNumber ("width"), h = o.GetNumber ("height");
		if (o.GetString ("kind") == "window") {
			add ("O:window:a", w * h, "");
			add ("O:window:n", 1.0, "");
			add ("O:window:w", w, "");
			const std::string t = typeName (o, "Dritare");
			add ("O:wt:" + t + ":a", w * h, "");
			add ("O:wt:" + t + ":n", 1.0, "");
			add ("O:wt:" + t + ":w", w, "");
		} else {
			const std::string t = typeName (o, "Der\xC3\xAB");
			const char* cls = doorClass (o.GetString ("type"));
			add (std::string ("O:door:") + cls + ":n", 1.0, "");
			add ("O:door:n", 1.0, "");
			add (std::string ("O:dt:") + cls + ":" + t + ":n", 1.0, "");
			add (std::string ("O:dt:") + cls + ":" + t + ":a", w * h, "");
		}
	}
	// dere hyrese: nese asnje dere nuk eshte klasifikuar si e jashtme, merret me e gjera ne perdhese
	if (q["O:door:ext:n"] == 0.0 && q["O:door:int:n"] > 0.0) {
		double best = 0.0;
		const Json::Value* bestDoor = nullptr;
		for (const Json::Value& o : model["openings"].AsArray ())
			if (o.GetString ("kind") == "door" && o.GetInt ("story") == 0 && o.GetNumber ("width") > best) { best = o.GetNumber ("width"); bestDoor = &o; }
		if (best >= 1.0 && bestDoor != nullptr) {
			q["O:door:ext:n"] = 1.0;
			q["O:door:int:n"] -= 1.0;
			const std::string t = typeName (*bestDoor, "Der\xC3\xAB");
			const double a = bestDoor->GetNumber ("width") * bestDoor->GetNumber ("height");
			q["O:dt:int:" + t + ":n"] -= 1.0;
			q["O:dt:int:" + t + ":a"] -= a;
			q["O:dt:ext:" + t + ":n"] += 1.0;
			q["O:dt:ext:" + t + ":a"] += a;
		}
	}

	// --- pajisjet sanitare
	for (const Json::Value& o : model["objects"].AsArray ()) {
		const std::string t = Lower (o.GetString ("type"));
		if (ContainsAny (t, { "wc", "lavaman", "vask", "dush", "bide", "pisoar", "toilet", "washbasin", "wash basin", "basin", "bathtub", "bath tub", "shower", "bidet", "urinal" }))
			add ("I:sanitary:n", 1.0, "");
	}

	// --- BTS, gjurma, germimi
	double bts = NumField (project, "bts");
	if (bts <= 0.0) {
		bts = Round (q["Z:a"] * 1.20, 1);
		if (bts > 0.0 && notes) notes->push_back ("BTS nuk \xC3\xABsht\xC3\xAB dh\xC3\xABn\xC3\xAB n\xC3\xAB t\xC3\xAB dh\xC3\xABnat e projektit: vler\xC3\xABsuar " +
												  Html::Num (bts, 1) + " m\xC2\xB2 = sip\xC3\xABrfaqja neto e zonave \xC3\x97 1,20.");
	}
	q["BTS"] = bts;
	double gj = NumField (project, "gjurma");
	if (gj <= 0.0) gj = std::max (q["C:TH:a"], q["C:DY-01:a"]);
	if (gj <= 0.0) gj = groundZones * 1.15;
	if (gj <= 0.0 && bts > 0.0) gj = bts / 2.0;
	q["GJ"] = Round (gj, 2);
	double depth = basement ? 3.3 : 1.0;
	if (project.Has ("_exc_depth")) depth = NumField (project, "_exc_depth");	// pjeset e kompleksit
	q["EXC:v"] = Round (gj * 1.15 * depth, 2);
	q["basement"] = basement ? 1.0 : 0.0;
	// parteri (vleresim): perimetri i objektit dhe pjesa e lire e parceles
	const double parc = NumField (project, "sip_parceles");
	q["PER"] = Round (4.0 * std::sqrt (std::max (0.0, gj)), 2);
	q["HAS_PARC"] = parc > 0.0 ? 1.0 : 0.0;
	q["PARC_FREE"] = Round (std::max (0.0, parc - gj), 2);
	q["PARC_PER"] = Round (parc > 0.0 ? 4.0 * std::sqrt (parc) : 0.0, 2);

	if (sourcesOut) *sourcesOut = sources;
	return q;
}

BoqOptions BillOfQuantities::OptionsFromJson (const Json::Value& req, const Catalog& cat)
{
	BoqOptions o;
	const Json::Value& prices = cat.Prices ();
	o.vat = req.Has ("vat") ? req.GetNumber ("vat") : prices.GetNumber ("vat", 18.0);
	o.contingency = req.Has ("contingency") ? req.GetNumber ("contingency") : prices.GetNumber ("contingency", 5.0);
	o.factor = req.GetNumber ("factor", 1.0);
	if (o.factor <= 0.0) o.factor = 1.0;
	o.includeZero = req.GetBool ("includeZero", false);
	o.editor = req.GetBool ("editor", false);
	if (req["overrides"].IsObject ()) o.overrides = req["overrides"];
	return o;
}

namespace {

std::string PropText (const std::vector<std::pair<std::string, std::string>>& props)
{
	std::string t;
	for (const auto& p : props) {
		if (p.second.empty () || p.second == "\xE2\x80\x94") continue;
		t += (t.empty () ? "" : "; ") + p.first + ": " + p.second;
	}
	return t;
}

}	// namespace

BoqResult BillOfQuantities::Compute (const Catalog& cat, const Json::Value& model, const Json::Value& project, const BoqOptions& opt)
{
	BoqResult r;
	r.options = opt;
	const Json::Value& prices = cat.Prices ();
	r.priceLevel = prices.GetString ("priceLevel");
	r.source = prices.GetString ("source");
	const Json::Value& settings = project["paramasa"];		// zgjedhjet e projektit (redaktori)
	const Json::Value& itemSet = settings["items"];
	std::set<std::string> offGroups;
	for (const Json::Value& g : settings["off_groups"].AsArray ()) offGroups.insert (g.AsString ());

	std::map<std::string, std::set<std::string>> sources;
	r.quantities = ModelQuantities (cat, model, project, &sources, &r.notes);
	auto Q = [&] (const std::string& key) -> double {
		auto it = r.quantities.find (key);
		return it == r.quantities.end () ? 0.0 : it->second;
	};

	for (const Json::Value& g : prices["groups"].AsArray ()) {
		BoqGroup grp;
		grp.id = g.GetString ("id");
		grp.name = g.GetString ("name");
		grp.on = offGroups.count (grp.id) == 0;
		r.groups.push_back (grp);
	}
	bool anyEstimated = false;

	auto evaluate = [&] (const Json::Value& terms, double& qty, double& areaSum, double& volSum, std::set<std::string>& src) {
		qty = areaSum = volSum = 0.0;
		for (const Json::Value& t : terms.AsArray ()) {
			const std::string key = t[0].AsString ();
			const double f = t[1].AsNumber (1.0);
			const double v = Q (key);
			qty += f * v;
			if (v != 0.0 && f > 0.0) {
				const std::string label = SourceLabel (key, sources);
				if (!label.empty ()) src.insert (label);
			}
			if (key.size () > 2 && key.compare (key.size () - 2, 2, ":a") == 0 && f > 0.0) {
				areaSum += v;
				volSum += Q (key.substr (0, key.size () - 1) + "v");
			}
		}
	};

	// zgjedhjet e projektit per nje pozicion: perfshirja, njesia, sasia, cmimi, vecorite
	auto applySettings = [&] (BoqLine& line, const std::map<std::string, double>& qtyByUnit, double basePrimary,
							  const std::string& primaryUnit, const std::string& baseCode, const Json::Value& item) {
		const Json::Value& st = itemSet[line.key];
		if (st.Has ("on")) line.on = st.GetBool ("on", true);
		// njesia
		std::string unit = primaryUnit;
		const std::string wanted = st.GetString ("u");
		if (!wanted.empty () && qtyByUnit.count (wanted)) unit = wanted;
		line.unit = unit;
		const double qPrimary = qtyByUnit.count (primaryUnit) ? qtyByUnit.at (primaryUnit) : 0.0;
		const double qUnit = qtyByUnit.count (unit) ? qtyByUnit.at (unit) : 0.0;
		const double conv = (unit != primaryUnit && qUnit > 1e-9) ? qPrimary / qUnit : 1.0;	// njesi primare per njesi te zgjedhur
		line.modelQty = Round (qUnit, UnitDecimals (unit));
		line.qty = line.modelQty;
		if (st["q"].IsNumber ()) { line.qty = Round (st["q"].AsNumber (), UnitDecimals (unit)); line.qtyOverridden = true; }
		// vecorite
		double delta = 0.0;
		const std::string ps = item.GetString ("ps");
		line.propSet = ps;
		if (!ps.empty ()) {
			for (const Json::Value& pg : prices["propSets"][ps].AsArray ()) {
				const std::string id = pg.GetString ("id");
				std::string val = st["props"].GetString (id, pg.GetString ("def"));
				for (const Json::Value& o : pg["opts"].AsArray ())
					if (o[0].AsString () == val) delta += o[1].AsNumber ();
				line.props.push_back ({ pg.GetString ("name"), val });
			}
		}
		// cmimi: projekti > zyra > (baza + vecorite) x koeficienti
		double price;
		if (st["p"].IsNumber ()) {
			price = st["p"].AsNumber ();
			line.overridden = true;
		} else if (opt.overrides[line.key].IsNumber () || opt.overrides[baseCode].IsNumber ()) {
			price = (opt.overrides[line.key].IsNumber () ? opt.overrides[line.key].AsNumber () : opt.overrides[baseCode].AsNumber () * conv) + delta * conv;
			line.overridden = true;
		} else {
			price = (basePrimary + delta) * conv * opt.factor;
		}
		line.price = Round (price, 2);
		line.value = Round (line.qty * line.price, 2);
		for (const auto& u : qtyByUnit) line.units.push_back (u.first);
		if (line.units.size () > 1) {
			// njesia primare e para
			std::stable_sort (line.units.begin (), line.units.end (), [&] (const std::string& x, const std::string& y) { return x == primaryUnit && y != primaryUnit; });
		}
		const std::string pt = PropText (line.props);
		if (!pt.empty ()) line.longDesc += " Ve\xC3\xA7orit\xC3\xAB: " + pt + ".";
	};

	auto pushLine = [&] (BoqLine& line) {
		BoqGroup* grp = nullptr;
		for (BoqGroup& g : r.groups) if (g.id == line.group) grp = &g;
		if (grp == nullptr) return;
		if (!grp->on) line.on = false;
		if (line.qty <= 0.0 && !opt.includeZero && !line.custom && !line.qtyOverridden) return;
		if (!line.on && !opt.editor) return;
		if (line.estimated) anyEstimated = true;
		grp->lines.push_back (line);
		if (line.on) grp->total += line.value;
	};

	for (const Json::Value& it : prices["items"].AsArray ()) {
		const std::string code = it.GetString ("code");
		const std::string group = it.GetString ("g");
		const std::string unit0 = it.GetString ("u");
		const std::string expand = it.GetString ("expand");
		const std::string longBase = it.GetString ("dl", it.GetString ("d"));

		if (expand.empty ()) {
			BoqLine line;
			line.code = code;
			line.key = code;
			line.group = group;
			line.desc = it.GetString ("d");
			line.longDesc = longBase;
			double qty = 0.0, aSum = 0.0, vSum = 0.0;
			std::set<std::string> src;
			evaluate (it["q"], qty, aSum, vSum, src);
			if (qty <= 1e-9 && it["alt"].IsArray ()) {
				std::set<std::string> altSrc;
				evaluate (it["alt"], qty, aSum, vSum, altSrc);
				if (qty > 1e-9) { line.estimated = true; src = altSrc; }
			}
			if (it.GetBool ("est") && qty > 1e-9) line.estimated = true;
			if (qty < 0.0) qty = 0.0;
			double base = it.GetNumber ("p");
			const double pcm = it.GetNumber ("pcm");
			if (pcm > 0.0 && aSum > 0.0) {
				line.thicknessCm = Round (vSum / aSum * 100.0, 1);
				base += pcm * line.thicknessCm;
				const std::string d = " \xE2\x80\x93 d = " + Html::Num (line.thicknessCm, line.thicknessCm < 10.0 ? 1 : 0) + " cm";
				line.longDesc += d;
			}
			for (const std::string& sname : src) line.source += (line.source.empty () ? "" : ", ") + sname;
			applySettings (line, { { unit0, qty } }, base, unit0, code, it);
			pushLine (line);
			continue;
		}

		// pozicioni ndahet sipas modelit
		struct Variant { std::string name; std::map<std::string, double> q; double thick = 0.0; };
		std::vector<Variant> variants;
		const Json::Value& unitsDef = it["units"];
		auto unitQty = [&] (const std::string& prefix) {
			std::map<std::string, double> m;
			if (unitsDef.IsObject () && !unitsDef.AsObject ().empty ()) {
				for (const auto& u : unitsDef.AsObject ()) m[u.first] = Q (prefix + u.second.GetString ("k"));
			} else {
				m[unit0] = Q (prefix + "a");
			}
			return m;
		};
		if (expand == "wtype" || expand.rfind ("dtype:", 0) == 0) {
			const std::string pre = expand == "wtype" ? std::string ("O:wt:") : "O:dt:" + expand.substr (6) + ":";
			std::set<std::string> names;
			for (const auto& kv : r.quantities)
				if (kv.first.rfind (pre, 0) == 0 && kv.first.size () > pre.size () + 2 && kv.second > 0.0)
					names.insert (kv.first.substr (pre.size (), kv.first.size () - pre.size () - 2));
			for (const std::string& n : names) {
				Variant v;
				v.name = n;
				v.q = unitQty (pre + n + ":");
				variants.push_back (v);
			}
		} else if (expand == "masd") {
			const std::string pre = "E:wall:mas:d";
			for (const auto& kv : r.quantities)
				if (kv.first.rfind (pre, 0) == 0 && kv.second > 0.0) {
					Variant v;
					v.thick = std::atof (kv.first.substr (pre.size ()).c_str ());
					v.name = "d = " + Html::Num (v.thick, 0) + " cm";
					v.q[unit0] = kv.second;
					variants.push_back (v);
				}
			std::sort (variants.begin (), variants.end (), [] (const Variant& x, const Variant& y) { return x.thick < y.thick; });
		}
		std::string primary = unit0;
		if (unitsDef.IsObject () && !unitsDef.AsObject ().empty ()) primary = unitsDef.AsObject ().front ().first;
		int k = 0;
		for (const Variant& v : variants) {
			BoqLine line;
			line.code = code + "." + std::to_string (++k);
			line.key = code + "|" + v.name;
			line.group = group;
			line.desc = it.GetString ("d") + " \xE2\x80\x93 " + v.name;
			line.longDesc = longBase + " Lloji: " + v.name + ".";
			line.source = expand == "masd" ? std::string ("muret bazike") : (expand == "wtype" ? std::string ("dritaret: ") : std::string ("dyert: ")) + (expand == "masd" ? std::string () : v.name);
			double base = it.GetNumber ("p") + it.GetNumber ("pcm") * v.thick;
			applySettings (line, v.q, base, primary, code, it);
			pushLine (line);
		}
	}

	// --- Pune te ndryshme: elementet e modelit pa pozicion (cmimi per t'u percaktuar)
	{
		std::map<std::string, std::pair<double, double>> misc;	// "lloji|emri" -> (siperfaqja, vellimi)
		for (const Json::Value& m : model["misc"].AsArray ()) {
			auto& v = misc[m.GetString ("kind") + "|" + m.GetString ("name")];
			v.first += m.GetNumber ("area");
			v.second += m.GetNumber ("volume");
		}
		for (const Json::Value& u : model["unknownComposites"].AsArray ())
			misc["Kompozit\xC3\xAB pa kod ARBS|" + u.GetString ("name")].first += u.GetNumber ("area");
		int k = 0;
		for (const auto& kv : misc) {
			const size_t bar = kv.first.find ('|');
			BoqLine line;
			line.code = "20." + std::to_string (++k);
			line.key = "misc|" + kv.first;
			line.group = "XX";
			line.desc = kv.first.substr (0, bar) + ": " + kv.first.substr (bar + 1) + " (element nga modeli)";
			line.longDesc = line.desc + " \xE2\x80\x93 \xC3\xA7mimi p\xC3\xABr t'u p\xC3\xABrcaktuar.";
			line.source = "modeli";
			std::map<std::string, double> qu;
			qu["m\xC2\xB2"] = kv.second.first;
			if (kv.second.second > 0.0) qu["m\xC2\xB3"] = kv.second.second;
			Json::Value dummy = Json::Value::MakeObject ();
			applySettings (line, qu, 0.0, "m\xC2\xB2", line.key, dummy);
			pushLine (line);
		}
		// pozicionet e shtuara me dore
		int c = 0;
		for (const Json::Value& cu : settings["custom"].AsArray ()) {
			BoqLine line;
			line.custom = true;
			line.group = cu.GetString ("g", "XX");
			line.key = "custom|" + cu.GetString ("id", std::to_string (c));
			line.code = "+" + std::to_string (++c);
			line.desc = cu.GetString ("d", "Pozicion i shtuar");
			line.longDesc = line.desc;
			line.source = "me dor\xC3\xAB";
			line.unit = cu.GetString ("u", "cop\xC3\xAB");
			line.qty = line.modelQty = Round (cu.GetNumber ("q"), UnitDecimals (line.unit));
			line.price = Round (cu.GetNumber ("p"), 2);
			line.value = Round (line.qty * line.price, 2);
			line.on = cu.GetBool ("on", true);
			line.units.push_back (line.unit);
			pushLine (line);
		}
	}

	// grupet pa pozicione hiqen (ne redaktor mbahen grupet e fikura qe kane pozicione)
	r.groups.erase (std::remove_if (r.groups.begin (), r.groups.end (), [] (const BoqGroup& g) { return g.lines.empty (); }), r.groups.end ());
	if (!opt.editor)
		r.groups.erase (std::remove_if (r.groups.begin (), r.groups.end (), [] (const BoqGroup& g) { return !g.on; }), r.groups.end ());

	for (BoqGroup& g : r.groups) { g.total = Round (g.total, 2); if (g.on) r.works += g.total; }
	r.works = Round (r.works, 2);
	r.contingency = Round (r.works * opt.contingency / 100.0, 2);
	r.net = Round (r.works + r.contingency, 2);
	r.vat = Round (r.net * opt.vat / 100.0, 2);
	r.total = Round (r.net + r.vat, 2);
	r.bts = Q ("BTS");
	r.perM2 = r.bts > 0.0 ? Round (r.net / r.bts, 2) : 0.0;
	if (anyEstimated)
		r.notes.push_back ("Pozicionet e sh\xC3\xABnuara me * jan\xC3\xAB vler\xC3\xABsuar sepse modeli nuk p\xC3\xABrmban elementet p\xC3\xABrkat\xC3\xABse (p.sh. objektet sanitare, gjat\xC3\xABsit\xC3\xAB e streh\xC3\xABve, parteri). Kontrolloni sasit\xC3\xAB n\xC3\xAB redaktor.");
	return r;
}

Json::Value BillOfQuantities::ToJson (const BoqResult& r)
{
	Json::Value o = Json::Value::MakeObject ();
	Json::Value groups = Json::Value::MakeArray ();
	for (const BoqGroup& g : r.groups) {
		Json::Value gj = Json::Value::MakeObject ();
		gj.Set ("id", g.id);
		gj.Set ("name", g.name);
		gj.Set ("total", g.total);
		gj.Set ("on", g.on);
		Json::Value lines = Json::Value::MakeArray ();
		for (const BoqLine& l : g.lines) {
			Json::Value lj = Json::Value::MakeObject ();
			lj.Set ("code", l.code);
			lj.Set ("desc", l.desc);
			lj.Set ("unit", l.unit);
			lj.Set ("qty", l.qty);
			lj.Set ("price", l.price);
			lj.Set ("value", l.value);
			lj.Set ("source", l.source);
			lj.Set ("estimated", l.estimated);
			lj.Set ("overridden", l.overridden);
			lj.Set ("key", l.key);
			lj.Set ("on", l.on);
			lj.Set ("modelQty", l.modelQty);
			lj.Set ("qtyOverridden", l.qtyOverridden);
			lj.Set ("custom", l.custom);
			if (!l.propSet.empty ()) {
				lj.Set ("ps", l.propSet);
				Json::Value pr = Json::Value::MakeArray ();
				for (const auto& p : l.props) { Json::Value x = Json::Value::MakeArray (); x.Push (Json::Value (p.first)); x.Push (Json::Value (p.second)); pr.Push (x); }
				lj.Set ("props", pr);
			}
			Json::Value us = Json::Value::MakeArray ();
			for (const std::string& u : l.units) us.Push (Json::Value (u));
			lj.Set ("units", us);
			if (l.thicknessCm > 0.0) lj.Set ("thicknessCm", l.thicknessCm);
			lines.Push (lj);
		}
		gj.Set ("lines", lines);
		groups.Push (gj);
	}
	o.Set ("groups", groups);
	o.Set ("works", r.works);
	o.Set ("contingency", r.contingency);
	o.Set ("net", r.net);
	o.Set ("vat", r.vat);
	o.Set ("total", r.total);
	o.Set ("bts", r.bts);
	o.Set ("perM2", r.perM2);
	o.Set ("vatRate", r.options.vat);
	o.Set ("contingencyRate", r.options.contingency);
	o.Set ("factor", r.options.factor);
	o.Set ("priceLevel", r.priceLevel);
	Json::Value notes = Json::Value::MakeArray ();
	for (const std::string& n : r.notes) notes.Push (Json::Value (n));
	o.Set ("notes", notes);
	return o;
}

std::vector<std::pair<std::string, BoqResult>> BillOfQuantities::ComputeParts (const Catalog& cat, const Json::Value& model, const Json::Value& project, const BoqOptions& opt)
{
	std::vector<std::pair<std::string, BoqResult>> out;
	const StructureOptions so = Structure::OptionsFromProject (project);
	const auto parts = Structure::SplitModel (model, so);
	if (parts.size () == 1 && parts.front ().first.empty ()) {
		out.push_back ({ "", Compute (cat, model, project, opt) });
		return out;
	}
	double blocksFoot = 0.0;
	for (const Json::Value& b : model["blocks"].AsArray ()) blocksFoot += b.GetNumber ("area");
	// parteri dhe gjelberimi (sipas parceles) i takojne pjeses se perbashket, ose bllokut te pare
	std::string sitePart = parts.front ().first;
	for (const auto& part : parts) if (part.first == Structure::CommonPart ()) sitePart = part.first;
	for (const auto& part : parts) {
		Json::Value pr = project;
		if (part.first != sitePart) pr.Set ("sip_parceles", 0.0);
		pr.Set ("bts", NumField (project["blloqet_bts"], part.first));	// BTS e dhene per bllok, ose vleresim nga zonat
		if (part.first == Structure::CommonPart ()) {
			double below = 0.0;
			for (const Json::Value& z : part.second["zones"].AsArray ()) if (z.GetInt ("story") < 0) below += z.GetNumber ("area");
			pr.Set ("gjurma", std::max (blocksFoot, below * 1.10));
			if (!so.sharedBasement || below <= 0.0) pr.Set ("_exc_depth", 0.0);
		} else {
			double foot = 0.0;
			for (const Json::Value& b : part.second["blocks"].AsArray ()) foot += b.GetNumber ("area");
			pr.Set ("gjurma", foot);
			if (so.sharedBasement) pr.Set ("_exc_depth", 0.0);	// germimi dhe bodrumi jane te pjesa e perbashket
		}
		BoqResult r = Compute (cat, part.second, pr, opt);
		r.notes.insert (r.notes.begin (), Structure::BlockName (model, part.first) + ": sasit\xC3\xAB nga elementet q\xC3\xAB bien brenda zon\xC3\xABs s\xC3\xAB bllokut" +
						std::string (part.first == Structure::CommonPart () ? " (katet n\xC3\xABntok\xC3\xAB dhe elementet jasht\xC3\xAB blloqeve)" : "") + ".");
		out.push_back ({ part.first, r });
	}
	return out;
}

Json::Value BillOfQuantities::PartsToJson (const std::vector<std::pair<std::string, BoqResult>>& parts, const Json::Value& model)
{
	Json::Value o = Json::Value::MakeObject ();
	const bool multi = !(parts.size () == 1 && parts.front ().first.empty ());
	o.Set ("multi", multi);
	Json::Value arr = Json::Value::MakeArray ();
	double works = 0, cont = 0, net = 0, vat = 0, total = 0, bts = 0;
	for (const auto& p : parts) {
		Json::Value j = ToJson (p.second);
		j.Set ("code", p.first);
		j.Set ("name", Structure::BlockName (model, p.first));
		arr.Push (j);
		works += p.second.works; cont += p.second.contingency; net += p.second.net; vat += p.second.vat; total += p.second.total;
		if (p.first != Structure::CommonPart ()) bts += p.second.bts;
	}
	o.Set ("parts", arr);
	Json::Value t = Json::Value::MakeObject ();
	t.Set ("works", Round (works, 2)); t.Set ("contingency", Round (cont, 2)); t.Set ("net", Round (net, 2));
	t.Set ("vat", Round (vat, 2)); t.Set ("total", Round (total, 2)); t.Set ("bts", bts); t.Set ("perM2", bts > 0 ? Round (net / bts, 2) : 0.0);
	if (!parts.empty ()) { t.Set ("vatRate", parts.front ().second.options.vat); t.Set ("contingencyRate", parts.front ().second.options.contingency); }
	o.Set ("total", t);
	return o;
}

Json::Value BillOfQuantities::PriceList (const Catalog& cat)
{
	const Json::Value& prices = cat.Prices ();
	Json::Value o = Json::Value::MakeObject ();
	o.Set ("priceLevel", prices.GetString ("priceLevel"));
	o.Set ("source", prices.GetString ("source"));
	o.Set ("vat", prices.GetNumber ("vat", 18.0));
	o.Set ("contingency", prices.GetNumber ("contingency", 5.0));
	o.Set ("groups", prices["groups"]);
	Json::Value items = Json::Value::MakeArray ();
	for (const Json::Value& it : prices["items"].AsArray ()) {
		Json::Value x = Json::Value::MakeObject ();
		x.Set ("code", it.GetString ("code"));
		x.Set ("g", it.GetString ("g"));
		x.Set ("u", it.GetString ("u"));
		x.Set ("p", it.GetNumber ("p"));
		if (it.GetNumber ("pcm") > 0.0) x.Set ("pcm", it.GetNumber ("pcm"));
		x.Set ("d", it.GetString ("d"));
		items.Push (x);
	}
	o.Set ("items", items);
	o.Set ("propSets", prices["propSets"]);
	return o;
}

// =============================================================================
// Dokumentet
// =============================================================================

namespace {

std::string LineDesc (const BoqLine& l)
{
	std::string d = Html::Esc (l.longDesc.empty () ? l.desc : l.longDesc);
	if (!l.source.empty ()) d += "<br><span class=\"note\">Sasia nga modeli: " + Html::Esc (l.source) + "</span>";
	return d;
}

std::string Body (const BoqResult& r, bool withPrices, const char* gh = "h2", bool intro = true, bool notes = true)
{
	std::ostringstream b;
	if (intro) {
	b << "<p>" << (withPrices
		? "Paramasa dhe parallogaria \xC3\xABsht\xC3\xAB hartuar n\xC3\xAB baz\xC3\xAB t\xC3\xAB sasive t\xC3\xAB nxjerra nga modeli BIM n\xC3\xAB Archicad (kompozitat sipas shtresave, elementet konstruktive, zonat, dyert dhe dritaret) dhe t\xC3\xAB \xC3\xA7mimeve nj\xC3\xABsi orientuese t\xC3\xAB tregut n\xC3\xAB Republik\xC3\xABn e Kosov\xC3\xABs"
		: "Paramasa p\xC3\xABr ofertim \xC3\xABsht\xC3\xAB hartuar n\xC3\xAB baz\xC3\xAB t\xC3\xAB sasive t\xC3\xAB nxjerra nga modeli BIM n\xC3\xAB Archicad. Ofertuesi plot\xC3\xABson \xC3\xA7mimin nj\xC3\xABsi p\xC3\xABr \xC3\xA7" "do pozicion; \xC3\xA7mimi p\xC3\xABrfshin materialin, pun\xC3\xABn, transportin dhe t\xC3\xAB gjitha punimet ndihm\xC3\xABse")
	  << (withPrices && !r.priceLevel.empty () ? " (niveli i \xC3\xA7mimeve " + Html::Esc (r.priceLevel) + ")" : "") << ".</p>";
	if (withPrices)
		b << "<p class=\"note\">" << Html::Esc (r.source) << " \xC3\x87mimet jan\xC3\xAB pa TVSH"
		  << (std::fabs (r.options.factor - 1.0) > 1e-9 ? ", me koeficient " + Html::Num (r.options.factor, 2) : std::string ())
		  << ". Pozicionet me \xC3\xA7mim t\xC3\xAB ndryshuar nga zyra sh\xC3\xABnohen me \xE2\x80\xA0.</p>";
	}

	for (const BoqGroup& g : r.groups) {
		b << "<" << gh << ">" << Html::Esc (g.id) << ". " << Html::Esc (g.name) << "</" << gh << "><table><tr><th style=\"width:7%\">Nr.</th><th>P\xC3\xABrshkrimi i punimeve</th><th class=\"c\" style=\"width:7%\">Nj\xC3\xABsia</th><th class=\"n\" style=\"width:10%\">Sasia</th>";
		b << "<th class=\"n\" style=\"width:11%\">\xC3\x87mimi nj\xC3\xABsi (\xE2\x82\xAC)</th><th class=\"n\" style=\"width:13%\">Vlera (\xE2\x82\xAC)</th>";
		b << "</tr>";
		for (const BoqLine& l : g.lines) {
			b << "<tr><td>" << Html::Esc (l.code) << (l.estimated ? "*" : "") << "</td><td>" << LineDesc (l) << "</td><td class=\"c\">" << Html::Esc (l.unit)
			  << "</td><td class=\"n\">" << Html::Num (l.qty, UnitDecimals (l.unit)) << "</td>";
			if (withPrices) b << "<td class=\"n\">" << BillOfQuantities::Money (l.price) << (l.overridden ? " \xE2\x80\xA0" : "") << "</td><td class=\"n\">" << BillOfQuantities::Money (l.value) << "</td>";
			else b << "<td></td><td></td>";
			b << "</tr>";
		}
		b << "<tr class=\"sum\"><td></td><td>GJITHSEJ " << Html::Esc (g.id) << ". " << Html::Esc (g.name) << "</td><td></td><td></td><td></td><td class=\"n\">"
		  << (withPrices ? BillOfQuantities::Money (g.total) : std::string ()) << "</td></tr></table>";
	}

	b << "<" << gh << ">Rekapitulimi</" << gh << "><table><tr><th style=\"width:7%\">Nr.</th><th>Grupi i punimeve</th><th class=\"n\" style=\"width:18%\">Vlera (\xE2\x82\xAC)</th>"
	  << (withPrices ? "<th class=\"n\" style=\"width:10%\">%</th>" : "") << "</tr>";
	for (const BoqGroup& g : r.groups) {
		b << "<tr><td>" << Html::Esc (g.id) << "</td><td>" << Html::Esc (g.name) << "</td><td class=\"n\">" << (withPrices ? BillOfQuantities::Money (g.total) : std::string ()) << "</td>";
		if (withPrices) b << "<td class=\"n\">" << Html::Num (r.works > 0.0 ? g.total / r.works * 100.0 : 0.0, 1) << "</td>";
		b << "</tr>";
	}
	auto recap = [&] (const std::string& label, double v, const char* cls) {
		b << "<tr class=\"" << cls << "\"><td></td><td>" << label << "</td><td class=\"n\">" << (withPrices ? BillOfQuantities::Money (v) : std::string ()) << "</td>" << (withPrices ? "<td></td>" : "") << "</tr>";
	};
	recap ("Gjithsej punimet nd\xC3\xABrtimore", r.works, "sub");
	recap ("T\xC3\xAB paparashikuarat " + Html::Num (r.options.contingency, r.options.contingency == std::floor (r.options.contingency) ? 0 : 1) + " %", r.contingency, "");
	recap ("Gjithsej pa TVSH", r.net, "sub");
	recap ("TVSH " + Html::Num (r.options.vat, r.options.vat == std::floor (r.options.vat) ? 0 : 1) + " %", r.vat, "");
	recap ("GJITHSEJ ME TVSH", r.total, "sum");
	b << "</table>";
	if (withPrices && r.bts > 0.0)
		b << "<p><b>Kostoja p\xC3\xABr m\xC2\xB2 BTS:</b> " << BillOfQuantities::Money (r.perM2) << " \xE2\x82\xAC/m\xC2\xB2 pa TVSH \xC2\xB7 "
		  << BillOfQuantities::Money (r.bts > 0.0 ? r.total / r.bts : 0.0) << " \xE2\x82\xAC/m\xC2\xB2 me TVSH (BTS = " << Html::Num (r.bts, 1) << " m\xC2\xB2).</p>";

	if (!notes) return b.str ();
	b << "<h2>V\xC3\xABrejtje</h2><ul>";
	b << "<li class=\"note\">Sasit\xC3\xAB jan\xC3\xAB llogaritur nga modeli: sip\xC3\xABrfaqja e elementeve me kompozita ARBS \xC3\x97 trash\xC3\xABsia e shtresave; kolonat, trar\xC3\xABt dhe elementet bazike nga v\xC3\xABllimet e Archicad-it; dyert dhe dritaret nga dimensionet e hapjeve. Para tenderimit, sasit\xC3\xAB kontrollohen me vizatimet e projektit zbatues.</li>";
	b << "<li class=\"note\">Armatura \xC3\xABsht\xC3\xAB vler\xC3\xABsuar me konsum mesatar kg/m\xC2\xB3 sipas llojit t\xC3\xAB elementit; sasia p\xC3\xABrfundimtare merret nga projekti i konstruksionit (lista e armatur\xC3\xABs).</li>";
	b << "<li class=\"note\">Instalimet (elektrike, uj\xC3\xAB-kanalizim, ngrohje) jan\xC3\xAB vler\xC3\xABsim p\xC3\xABr m\xC2\xB2 BTS deri n\xC3\xAB hartimin e projekteve t\xC3\xAB instalimeve.</li>";
	for (const std::string& n : r.notes) b << "<li class=\"note\">" << Html::Esc (n) << "</li>";
	b << "</ul>";
	return b.str ();
}

}	// namespace

namespace {

// -----------------------------------------------------------------------------
// Excel (.xlsx) me formula te gjalla
// Kolonat: A Nr. | B Pershkrimi | C Njesia | D Sasia | E Cmimi | F Shuma (=D*E)
// -----------------------------------------------------------------------------

struct XlsPartRefs {
	std::string sheet;
	std::map<std::string, std::string> groupTotal;	// id i grupit -> qeliza e totalit (ne kete flete)
	std::string works, contingency, net, vat, total, bts;
};

const double kDescWidth = 64.0;

std::string QS (const std::string& sheet)
{
	std::string q = "'";
	for (char c : sheet) { q += c; if (c == '\'') q += '\''; }
	return q + "'!";
}

std::string Joined (std::initializer_list<std::string> parts, const char* sep = "  \xC2\xB7  ")
{
	std::string o;
	for (const std::string& p : parts) if (!p.empty ()) o += (o.empty () ? "" : sep) + p;
	return o;
}

// koka e zyres me logo + titulli + te dhenat e projektit; kthen rreshtin e radhes
int XlsHeader (Xlsx::Sheet& sh, const Json::Value& project, const Json::Value& office, const std::string& title, const std::string& subtitle)
{
	sh.ColWidth (0, 8.0);
	sh.ColWidth (1, kDescWidth);
	sh.ColWidth (2, 8.0);
	sh.ColWidth (3, 12.0);
	sh.ColWidth (4, 13.0);
	sh.ColWidth (5, 16.0);

	bool logo = false;
	std::string bytes, ext;
	if (Xlsx::DecodeDataUri (office.GetString ("logo"), &bytes, &ext)) {
		int w = 0, h = 0;
		if (Xlsx::ImageSize (bytes, ext, &w, &h)) {
			Xlsx::Image img;
			img.bytes = bytes;
			img.ext = ext;
			img.heightPx = 56;
			img.widthPx = std::max (1, std::min (300, static_cast<int> (std::lround (56.0 * w / h))));
			sh.SetImage (img);
			logo = true;
		}
	}
	const std::string company = office.GetString ("kompania", "ARBS Studio");
	if (!logo) sh.Text (0, 0, company, Xlsx::S_Bold);
	sh.Text (0, 3, company, Xlsx::S_Bold);
	sh.Text (1, 3, Joined ({ office.GetString ("adresa"), office.GetString ("telefoni") }), Xlsx::S_Plain);
	sh.Text (2, 3, Joined ({ office.GetString ("email"), office.GetString ("web") }), Xlsx::S_Plain);
	for (int r = 0; r < 3; ++r) { sh.Merge (r, 3, r, 5); sh.RowHeight (r, 15.0); }
	if (!logo) sh.Merge (0, 0, 0, 1);

	sh.Text (4, 0, title, Xlsx::S_Title);
	sh.Merge (4, 0, 4, 5);
	sh.RowHeight (4, 22.0);
	std::string obj = project.GetString ("emri");
	if (!project.GetString ("destinimi").empty ()) obj += (obj.empty () ? "" : " \xE2\x80\x93 ") + project.GetString ("destinimi");
	sh.Text (5, 0, subtitle.empty () ? obj : obj + (obj.empty () ? "" : " \xE2\x80\x93 ") + subtitle, Xlsx::S_Subtitle);
	sh.Merge (5, 0, 5, 5);
	sh.RowHeight (5, 18.0);
	std::string loc = project.GetString ("vendi");
	if (!project.GetString ("komuna").empty ()) loc += (loc.empty () ? "" : ", ") + std::string ("Komuna ") + project.GetString ("komuna");
	if (!project.GetString ("parcela").empty ()) loc += ", P. nr. " + project.GetString ("parcela");
	const std::string inv = project.GetString ("investitori").empty () ? std::string () : "Investitori: " + project.GetString ("investitori");
	sh.Text (6, 0, Joined ({ inv, loc, project.GetString ("faza"), project.GetString ("nr"), project.GetString ("data") }), Xlsx::S_Small);
	sh.Merge (6, 0, 6, 5);
	return 8;
}

// tabela e punimeve + rekapitulimi i nje pjese (ose i projektit me nje pjese)
XlsPartRefs XlsPart (Xlsx::Workbook& wb, const std::string& sheetName, const BoqResult& r, const Json::Value& project, const Json::Value& office,
					 const std::string& title, const std::string& subtitle, bool withPrices)
{
	using namespace Xlsx;
	Sheet& sh = wb.AddSheet (sheetName);
	XlsPartRefs refs;
	refs.sheet = sh.name;
	int row = XlsHeader (sh, project, office, title, subtitle);

	const char* heads[] = { "Nr.", "P\xC3\xABrshkrimi i punimeve", "Nj\xC3\xABs.", "Sasia", "\xC3\x87mimi nj\xC3\xABsi (\xE2\x82\xAC)", "Shuma (\xE2\x82\xAC)" };
	for (int c = 0; c < 6; ++c) sh.Text (row, c, heads[c], S_Header);
	sh.RowHeight (row, 28.0);
	sh.repeatRow1 = sh.repeatRow2 = row;
	++row;

	for (const BoqGroup& g : r.groups) {
		sh.Text (row, 0, g.id, S_Group);
		sh.Text (row, 1, g.name, S_Group);
		for (int c = 2; c < 6; ++c) sh.Blank (row, c, S_Group);
		sh.Merge (row, 1, row, 5);
		sh.RowHeight (row, 18.0);
		++row;
		const int first = row;
		double sum = 0.0;
		for (const BoqLine& l : g.lines) {
			if (!l.on) continue;
			const std::string desc = l.longDesc.empty () ? l.desc : l.longDesc;
			sh.Text (row, 0, l.code + (l.estimated ? "*" : ""), S_Center);
			sh.Text (row, 1, desc, S_Text);
			sh.Text (row, 2, l.unit, S_Center);
			sh.Number (row, 3, l.qty, S_Number);
			if (withPrices) sh.Number (row, 4, l.price, S_Money);
			else sh.Blank (row, 4, S_InputMoney);
			const double v = withPrices ? l.value : 0.0;
			sh.Formula (row, 5, "ROUND(" + Ref (row, 3) + "*" + Ref (row, 4) + ",2)", v, S_Money);
			sum += v;
			sh.RowHeight (row, EstimateRowHeight (desc, kDescWidth));
			++row;
		}
		sh.Blank (row, 0, S_TotalLabel);
		sh.Text (row, 1, "GJITHSEJ " + g.id + ". " + g.name, S_TotalLabel);
		for (int c = 2; c < 5; ++c) sh.Blank (row, c, S_TotalLabel);
		sh.Merge (row, 1, row, 4);
		sh.Formula (row, 5, row > first ? "SUM(" + Ref (first, 5) + ":" + Ref (row - 1, 5) + ")" : std::string ("0"), sum, S_TotalMoney);
		refs.groupTotal[g.id] = Ref (row, 5);
		sh.RowHeight (row, 18.0);
		row += 2;
	}

	// rekapitulimi
	sh.Text (row, 0, "REKAPITULIMI", S_RecapHeader);
	for (int c = 1; c < 6; ++c) sh.Blank (row, c, S_RecapHeader);
	sh.Merge (row, 0, row, 5);
	sh.RowHeight (row, 20.0);
	++row;
	const int recapFirst = row;
	int worksRow = 0;
	{
		// rreshtat e grupeve; % llogaritet pasi te dihet rreshti i totalit
		std::vector<int> groupRows;
		for (const BoqGroup& g : r.groups) {
			sh.Text (row, 0, g.id, S_Center);
			sh.Text (row, 1, g.name, S_Text);
			sh.Blank (row, 2, S_Text);
			sh.Blank (row, 3, S_Text);
			sh.Merge (row, 1, row, 3);
			sh.Formula (row, 5, refs.groupTotal[g.id], withPrices ? g.total : 0.0, S_Money);
			groupRows.push_back (row);
			++row;
		}
		worksRow = row;
		for (size_t i = 0; i < groupRows.size (); ++i) {
			const double share = withPrices && r.works > 0.0 ? r.groups[i].total / r.works : 0.0;
			sh.Formula (groupRows[i], 4, "IF(" + Ref (worksRow, 5) + "=0,0," + Ref (groupRows[i], 5) + "/" + Ref (worksRow, 5) + ")", share, S_Percent);
		}
	}
	auto label = [&] (int rr, const std::string& text, int style) {
		sh.Blank (rr, 0, style);
		sh.Text (rr, 1, text, style);
		sh.Blank (rr, 2, style);
		sh.Blank (rr, 3, style);
		sh.Merge (rr, 1, rr, 3);
	};
	const double k = withPrices ? 1.0 : 0.0;
	label (row, "Gjithsej punimet nd\xC3\xABrtimore", S_TotalLabel);
	sh.Blank (row, 4, S_TotalLabel);
	sh.Formula (row, 5, row > recapFirst ? "SUM(" + Ref (recapFirst, 5) + ":" + Ref (row - 1, 5) + ")" : std::string ("0"), k * r.works, S_TotalMoney);
	refs.works = Ref (row, 5);
	++row;
	label (row, "T\xC3\xAB paparashikuarat (norma n\xC3\xAB kolon\xC3\xABn E)", S_Text);
	sh.Number (row, 4, r.options.contingency / 100.0, S_Percent);
	sh.Formula (row, 5, "ROUND(" + refs.works + "*" + Ref (row, 4) + ",2)", k * r.contingency, S_Money);
	refs.contingency = Ref (row, 5);
	++row;
	label (row, "Gjithsej pa TVSH", S_TotalLabel);
	sh.Blank (row, 4, S_TotalLabel);
	sh.Formula (row, 5, refs.works + "+" + refs.contingency, k * r.net, S_TotalMoney);
	refs.net = Ref (row, 5);
	++row;
	label (row, "TVSH (norma n\xC3\xAB kolon\xC3\xABn E)", S_Text);
	sh.Number (row, 4, r.options.vat / 100.0, S_Percent);
	sh.Formula (row, 5, "ROUND(" + refs.net + "*" + Ref (row, 4) + ",2)", k * r.vat, S_Money);
	refs.vat = Ref (row, 5);
	++row;
	label (row, "GJITHSEJ ME TVSH", S_GrandLabel);
	sh.Blank (row, 4, S_GrandLabel);
	sh.Formula (row, 5, refs.net + "+" + refs.vat, k * r.total, S_GrandMoney);
	refs.total = Ref (row, 5);
	sh.RowHeight (row, 20.0);
	++row;
	label (row, "BTS (m\xC2\xB2)", S_Text);
	sh.Blank (row, 4, S_Text);
	sh.Number (row, 5, r.bts, S_Number);
	refs.bts = Ref (row, 5);
	++row;
	label (row, "Kostoja p\xC3\xABr m\xC2\xB2 BTS pa TVSH", S_Text);
	sh.Blank (row, 4, S_Text);
	sh.Formula (row, 5, "IF(" + refs.bts + "=0,0," + refs.net + "/" + refs.bts + ")", withPrices && r.bts > 0.0 ? r.net / r.bts : 0.0, S_Money);
	row += 2;

	// shenime
	std::vector<std::string> notes;
	if (withPrices) {
		std::string n = r.source + " \xC3\x87mimet jan\xC3\xAB pa TVSH";
		if (!r.priceLevel.empty ()) n += ", niveli i \xC3\xA7mimeve " + r.priceLevel;
		notes.push_back (n + ".");
	} else {
		notes.push_back ("Ofertuesi plot\xC3\xABson \xC3\xA7mimin nj\xC3\xABsi n\xC3\xAB qelizat e verdha (kolona E); shumat dhe rekapitulimi llogariten vet\xC3\xABm. \xC3\x87mimi p\xC3\xABrfshin materialin, pun\xC3\xABn, transportin dhe punimet ndihm\xC3\xABse.");
	}
	notes.push_back ("Sasit\xC3\xAB jan\xC3\xAB nxjerr\xC3\xAB nga modeli BIM n\xC3\xAB Archicad; pozicionet me * jan\xC3\xAB vler\xC3\xABsime. Normat e t\xC3\xAB paparashikuarave dhe TVSH-s\xC3\xAB mund t\xC3\xAB ndryshohen n\xC3\xAB kolon\xC3\xABn E.");
	for (const std::string& n : r.notes) notes.push_back (n);
	for (const std::string& n : notes) {
		sh.Text (row, 0, n, S_Note);
		sh.Merge (row, 0, row, 5);
		sh.RowHeight (row, EstimateRowHeight (n, 100.0, 13.0));
		++row;
	}
	return refs;
}

// fleta e rekapitulimit te projektit me blloqe: grupet x pjeset, me referenca ne fletet e pjeseve
void XlsMultiRecap (Xlsx::Sheet& sh, const std::vector<std::pair<std::string, BoqResult>>& parts, const std::vector<XlsPartRefs>& refs,
					const std::vector<std::string>& partNames, const Json::Value& project, const Json::Value& office, bool withPrices, int firstRow)
{
	using namespace Xlsx;
	(void) project; (void) office;
	const int n = static_cast<int> (parts.size ());
	const int totalCol = 2 + n;
	sh.landscape = n > 3;
	sh.ColWidth (0, 7.0);
	sh.ColWidth (1, 40.0);
	for (int c = 0; c <= n; ++c) sh.ColWidth (2 + c, 16.0);

	int row = firstRow;
	sh.Text (row, 0, "Nr.", S_Header);
	sh.Text (row, 1, "Grupi i punimeve", S_Header);
	for (int i = 0; i < n; ++i) sh.Text (row, 2 + i, partNames[i], S_Header);
	sh.Text (row, totalCol, "GJITHSEJ", S_Header);
	sh.RowHeight (row, 30.0);
	sh.repeatRow1 = sh.repeatRow2 = row;
	++row;

	std::vector<std::pair<std::string, std::string>> groups;
	for (const auto& p : parts)
		for (const BoqGroup& g : p.second.groups)
			if (std::find_if (groups.begin (), groups.end (), [&] (const std::pair<std::string, std::string>& x) { return x.first == g.id; }) == groups.end ())
				groups.push_back ({ g.id, g.name });
	auto roman = [] (const std::string& id) {
		static const char* R[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX", "XX" };
		for (int i = 0; i < 20; ++i) if (id == R[i]) return i;
		return 99;
	};
	std::stable_sort (groups.begin (), groups.end (), [&] (const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) { return roman (a.first) < roman (b.first); });

	const int first = row;
	for (const auto& g : groups) {
		sh.Text (row, 0, g.first, S_Center);
		sh.Text (row, 1, g.second, S_Text);
		double sum = 0.0;
		for (int i = 0; i < n; ++i) {
			double v = 0.0;
			for (const BoqGroup& pg : parts[i].second.groups) if (pg.id == g.first) v = pg.total;
			auto it = refs[i].groupTotal.find (g.first);
			if (it != refs[i].groupTotal.end ()) {
				const double cached = withPrices ? v : 0.0;
				sh.Formula (row, 2 + i, QS (refs[i].sheet) + it->second, cached, S_Money);
				sum += cached;
			} else {
				sh.Number (row, 2 + i, 0.0, S_Money);
			}
		}
		sh.Formula (row, totalCol, "SUM(" + Ref (row, 2) + ":" + Ref (row, 1 + n) + ")", sum, S_TotalMoney);
		++row;
	}

	struct RowDef { const char* label; std::string XlsPartRefs::* ref; double BoqResult::* field; int style, money; };
	const RowDef defs[] = {
		{ "Gjithsej punimet nd\xC3\xABrtimore", &XlsPartRefs::works, &BoqResult::works, S_TotalLabel, S_TotalMoney },
		{ "T\xC3\xAB paparashikuarat", &XlsPartRefs::contingency, &BoqResult::contingency, S_Text, S_Money },
		{ "Gjithsej pa TVSH", &XlsPartRefs::net, &BoqResult::net, S_TotalLabel, S_TotalMoney },
		{ "TVSH", &XlsPartRefs::vat, &BoqResult::vat, S_Text, S_Money },
		{ "GJITHSEJ ME TVSH", &XlsPartRefs::total, &BoqResult::total, S_GrandLabel, S_GrandMoney } };
	int netRow = 0;
	(void) first;
	for (const RowDef& d : defs) {
		sh.Blank (row, 0, d.style);
		sh.Text (row, 1, d.label, d.style);
		double sum = 0.0;
		for (int i = 0; i < n; ++i) {
			const double cached = withPrices ? parts[i].second.*(d.field) : 0.0;
			sh.Formula (row, 2 + i, QS (refs[i].sheet) + refs[i].*(d.ref), cached, d.money);
			sum += cached;
		}
		sh.Formula (row, totalCol, "SUM(" + Ref (row, 2) + ":" + Ref (row, 1 + n) + ")", sum, d.money);
		if (d.ref == &XlsPartRefs::net) netRow = row;
		++row;
	}
	// BTS: pjesa e perbashket nuk ka BTS te vetin (kostoja e saj ndahet mbi blloqet)
	sh.Blank (row, 0, S_Text);
	sh.Text (row, 1, "BTS (m\xC2\xB2)", S_Text);
	double btsSum = 0.0;
	std::string btsSumF;
	for (int i = 0; i < n; ++i) {
		const bool common = parts[i].first == Structure::CommonPart ();
		sh.Formula (row, 2 + i, QS (refs[i].sheet) + refs[i].bts, parts[i].second.bts, S_Number);
		if (!common) { btsSum += parts[i].second.bts; btsSumF += (btsSumF.empty () ? "" : "+") + Ref (row, 2 + i); }
	}
	sh.Formula (row, totalCol, btsSumF.empty () ? std::string ("0") : btsSumF, btsSum, S_Number);
	const int btsRow = row;
	++row;
	sh.Blank (row, 0, S_Text);
	sh.Text (row, 1, "Kostoja p\xC3\xABr m\xC2\xB2 BTS pa TVSH", S_Text);
	double netSum = 0.0;
	for (int i = 0; i < n; ++i) {
		const BoqResult& pr = parts[i].second;
		sh.Formula (row, 2 + i, "IF(" + Ref (btsRow, 2 + i) + "=0,0," + Ref (netRow, 2 + i) + "/" + Ref (btsRow, 2 + i) + ")", withPrices && pr.bts > 0.0 ? pr.net / pr.bts : 0.0, S_Money);
		netSum += withPrices ? pr.net : 0.0;
	}
	sh.Formula (row, totalCol, "IF(" + Ref (btsRow, totalCol) + "=0,0," + Ref (netRow, totalCol) + "/" + Ref (btsRow, totalCol) + ")", btsSum > 0.0 ? netSum / btsSum : 0.0, S_TotalMoney);
	row += 2;
	const std::string note = "Vlerat merren me formula nga fletet e secil\xC3\xABs pjes\xC3\xAB; ndryshimet e \xC3\xA7mimeve ose sasive n\xC3\xAB ato flet\xC3\xAB pasqyrohen k\xC3\xABtu. "
							 "Kostoja totale p\xC3\xABr m\xC2\xB2 llogaritet me koston e plot\xC3\xAB (p\xC3\xABrfshir\xC3\xAB pjes\xC3\xABn e p\xC3\xABrbashk\xC3\xABt) mbi BTS e blloqeve.";
	sh.Text (row, 0, note, S_Note);
	sh.Merge (row, 0, row, totalCol);
	sh.RowHeight (row, 28.0);
}

std::string BoqWorkbook (const std::vector<std::pair<std::string, BoqResult>>& parts, const Json::Value& model, const Json::Value& project,
						 const Json::Value& office, bool withPrices)
{
	const std::string title = withPrices ? "PARAMASA DHE PARALLOGARIA" : "PARAMASA P\xC3\x8BR OFERTIM";
	Xlsx::Workbook wb;
	if (parts.size () == 1 && parts.front ().first.empty ()) {
		XlsPart (wb, "Paramasa", parts.front ().second, project, office, title, std::string (), withPrices);
		return wb.Save ();
	}
	// fleta e pare: rekapitulimi (mbushet pasi te dihen referencat e fleteve te pjeseve)
	wb.sheets.reserve (parts.size () + 1);
	wb.AddSheet ("Rekapitulimi");
	std::vector<XlsPartRefs> refs;
	std::vector<std::string> names;
	for (const auto& p : parts) {
		const std::string name = p.first == Structure::CommonPart () ? std::string ("Pjesa e p\xC3\xABrbashk\xC3\xABt") : Structure::BlockName (model, p.first);
		const std::string code = p.first == Structure::CommonPart () || p.first.empty () ? std::string ("PB") : p.first;
		names.push_back (name);
		const std::string sheetName = name.find (code) != std::string::npos ? name : code + " " + name;
		refs.push_back (XlsPart (wb, sheetName, p.second, project, office, title, name, withPrices));
	}
	Xlsx::Sheet& recap = wb.sheets.front ();
	const int row = XlsHeader (recap, project, office, title, "Rekapitulimi sipas blloqeve");
	XlsMultiRecap (recap, parts, refs, names, project, office, withPrices, row);
	return wb.Save ();
}

const char* kXlsxMime = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";

// Rekapitulimi i projektit: grupet (rreshta) x pjeset (kolona)
std::string MultiRecap (const std::vector<std::pair<std::string, BoqResult>>& parts, const Json::Value& model, const Json::Value& project, bool withPrices)
{
	std::ostringstream b;
	std::vector<std::pair<std::string, std::string>> groups;	// id, emri ne rendin e listes
	for (const auto& p : parts)
		for (const BoqGroup& g : p.second.groups)
			if (std::find_if (groups.begin (), groups.end (), [&] (const std::pair<std::string, std::string>& x) { return x.first == g.id; }) == groups.end ())
				groups.push_back ({ g.id, g.name });
	std::sort (groups.begin (), groups.end (), [&] (const std::pair<std::string, std::string>& x, const std::pair<std::string, std::string>& y) {
		auto roman = [] (const std::string& r) {
			static const char* R[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX", "XX" };
			for (int i = 0; i < 20; ++i) {
				if (r == R[i]) return i;
			}
			return 99;
		};
		return roman (x.first) < roman (y.first);
	});
	auto groupTotal = [] (const BoqResult& r, const std::string& id) { for (const BoqGroup& g : r.groups) if (g.id == id) return g.total; return 0.0; };

	b << "<h2>Rekapitulimi i projektit sipas blloqeve</h2><table><tr><th style=\"width:5%\">Nr.</th><th>Grupi i punimeve</th>";
	for (const auto& p : parts) b << "<th class=\"n\">" << Html::Esc (p.first == Structure::CommonPart () ? std::string ("Pjesa e p\xC3\xABrbashk\xC3\xABt") : Structure::BlockName (model, p.first)) << "</th>";
	b << "<th class=\"n\">GJITHSEJ</th></tr>";
	auto money = [&] (double v) { return withPrices ? BillOfQuantities::Money (v) : std::string (); };
	for (const auto& g : groups) {
		double sum = 0.0;
		b << "<tr><td>" << Html::Esc (g.first) << "</td><td>" << Html::Esc (g.second) << "</td>";
		for (const auto& p : parts) { const double v = groupTotal (p.second, g.first); sum += v; b << "<td class=\"n\">" << (v > 0.0 ? money (v) : std::string ("\xE2\x80\x93")) << "</td>"; }
		b << "<td class=\"n\"><b>" << money (sum) << "</b></td></tr>";
	}
	struct RowDef { const char* label; double BoqResult::* field; const char* cls; };
	const RowDef rows[] = {
		{ "Gjithsej punimet nd\xC3\xABrtimore", &BoqResult::works, "sub" }, { "T\xC3\xAB paparashikuarat", &BoqResult::contingency, "" },
		{ "Gjithsej pa TVSH", &BoqResult::net, "sub" }, { "TVSH", &BoqResult::vat, "" }, { "GJITHSEJ ME TVSH", &BoqResult::total, "sum" } };
	for (const RowDef& rd : rows) {
		double sum = 0.0;
		b << "<tr class=\"" << rd.cls << "\"><td></td><td>" << rd.label << "</td>";
		for (const auto& p : parts) { const double v = p.second.*(rd.field); sum += v; b << "<td class=\"n\">" << money (v) << "</td>"; }
		b << "<td class=\"n\">" << money (sum) << "</td></tr>";
	}
	if (withPrices) {
		double btsSum = 0.0, netSum = 0.0;
		b << "<tr><td></td><td>BTS (m\xC2\xB2)</td>";
		for (const auto& p : parts) { b << "<td class=\"n\">" << Html::Num (p.second.bts, 1) << "</td>"; if (p.first != Structure::CommonPart ()) btsSum += p.second.bts; netSum += p.second.net; }
		b << "<td class=\"n\">" << Html::Num (btsSum, 1) << "</td></tr>";
		b << "<tr><td></td><td>\xE2\x82\xAC/m\xC2\xB2 BTS pa TVSH</td>";
		for (const auto& p : parts) b << "<td class=\"n\">" << BillOfQuantities::Money (p.second.perM2) << "</td>";
		b << "<td class=\"n\"><b>" << BillOfQuantities::Money (btsSum > 0 ? netSum / btsSum : 0.0) << "</b></td></tr>";
	}
	b << "</table>";
	if (withPrices) b << "<p class=\"note\">Kolona GJITHSEJ \xE2\x82\xAC/m\xC2\xB2 llogaritet me koston e plot\xC3\xAB (p\xC3\xABrfshir\xC3\xAB pjes\xC3\xABn e p\xC3\xABrbashk\xC3\xABt) mbi BTS e blloqeve.</p>";

	// kostoja per njesi
	if (withPrices) {
		const StructureOptions so = Structure::OptionsFromProject (project);
		const Json::Value units = Structure::Units (model, so, project["njesite_manual"])["units"];
		if (units.Size () > 0) {
			double common = 0.0, btsBlocks = 0.0;
			std::map<std::string, double> netBlock, btsBlock, weightBlock;
			for (const auto& p : parts) {
				if (p.first == Structure::CommonPart ()) common += p.second.net;
				else { netBlock[p.first] += p.second.net; btsBlock[p.first] += p.second.bts; btsBlocks += p.second.bts; }
			}
			auto weight = [] (const Json::Value& u) { return u.GetNumber ("gross") > 0.0 ? u.GetNumber ("gross") : u.GetNumber ("withCommon"); };
			for (const Json::Value& u : units.AsArray ()) weightBlock[u.GetString ("block")] += weight (u);
			const double vatK = 1.0 + (parts.empty () ? 18.0 : parts.front ().second.options.vat) / 100.0;
			b << "<h2>Kostoja sipas nj\xC3\xABsive</h2><p class=\"note\">Kostoja e bllokut (me pjes\xC3\xABn e p\xC3\xABrbashk\xC3\xABt t\xC3\xAB nd\xC3\xABr\xC3\xAB sipas BTS) shp\xC3\xABrndahet n\xC3\xAB nj\xC3\xABsi sipas sip\xC3\xABrfaqes bruto (ose neto me hap\xC3\xABsirat e p\xC3\xABrbashk\xC3\xABta kur bruto mungon).</p>";
			b << "<table><tr><th>Nj\xC3\xABsia</th><th>Blloku / hyrja</th><th class=\"c\">Kati</th><th>Lloji</th><th class=\"n\">Sip. (m\xC2\xB2)</th><th class=\"n\">Kosto pa TVSH (\xE2\x82\xAC)</th><th class=\"n\">Me TVSH (\xE2\x82\xAC)</th><th class=\"n\">\xE2\x82\xAC/m\xC2\xB2</th></tr>";
			for (const Json::Value& u : units.AsArray ()) {
				const std::string bl = u.GetString ("block");
				const double blockCost = (netBlock.count (bl) ? netBlock[bl] : 0.0) + (btsBlocks > 0.0 ? common * btsBlock[bl] / btsBlocks : 0.0);
				const double w = weight (u);
				const double cost = weightBlock[bl] > 0.0 ? blockCost * w / weightBlock[bl] : 0.0;
				b << "<tr><td>" << Html::Esc (u.GetString ("number")) << "</td><td>" << Html::Esc (bl + (bl.empty () ? "" : " / ") + u.GetString ("entrance")) << "</td><td class=\"c\">" << u.GetInt ("story")
				  << "</td><td>" << Html::Esc (u.GetString ("kindLabel") + (u.GetString ("structure").empty () ? "" : " " + u.GetString ("structure"))) << "</td><td class=\"n\">" << Html::Num (w)
				  << "</td><td class=\"n\">" << BillOfQuantities::Money (cost) << "</td><td class=\"n\">" << BillOfQuantities::Money (cost * vatK) << "</td><td class=\"n\">" << BillOfQuantities::Money (w > 0 ? cost / w : 0.0) << "</td></tr>";
			}
			b << "</table>";
		}
	}
	return b.str ();
}

}	// namespace

std::vector<ReportOutput> Reports::QuantitiesAndCost (const Catalog& cat, const Json::Value& project, const Json::Value& office,
													  const Json::Value& model, const BoqOptions& opt)
{
	const auto parts = BillOfQuantities::ComputeParts (cat, model, project, opt);
	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PKR"));
	const std::string title = "PARAMASA DHE PARALLOGARIA";

	if (parts.size () == 1 && parts.front ().first.empty ()) {
		const BoqResult& r = parts.front ().second;
		const std::string body = Body (r, true);
		outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.html", Document (title, body, project, office, false), "text/html" });
		outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.doc", Document (title, body, project, office, true), "application/msword" });
		outs.push_back ({ base + "_A31_Paramasa-per-ofertim.doc", Document ("PARAMASA P\xC3\x8BR OFERTIM", Body (r, false), project, office, true), "application/msword" });
		outs.insert (outs.begin (), { base + "_A30_Paramasa-dhe-parallogaria.xlsx", BoqWorkbook (parts, model, project, office, true), kXlsxMime });
		outs.push_back ({ base + "_A31_Paramasa-per-ofertim.xlsx", BoqWorkbook (parts, model, project, office, false), kXlsxMime });
		return outs;
	}

	// projekt me blloqe: dokumenti i plote (rekapitulimi + te gjitha pjeset) dhe nje dokument per cdo pjese
	auto fullBody = [&] (bool withPrices) {
		std::ostringstream b;
		// hyrja
		const BoqResult& r0 = parts.front ().second;
		std::string intro = Body (r0, withPrices, "h3", true, false);
		intro = intro.substr (0, intro.find ("<h3>"));
		b << intro;
		b << MultiRecap (parts, model, project, withPrices);
		for (const auto& p : parts) {
			b << "<h2 style=\"margin-top:28px\">" << Html::Esc (Structure::BlockName (model, p.first)) << (p.first == Structure::CommonPart () || p.first.empty () ? "" : " (" + Html::Esc (p.first) + ")") << "</h2>";
			std::string part = Body (p.second, withPrices, "h3", true, false);
			const size_t h = part.find ("<h3>");
			b << (h == std::string::npos ? part : part.substr (h));
		}
		std::string notes = Body (parts.front ().second, withPrices, "h3", false, true);
		const size_t n = notes.find ("<h2>V");
		if (n != std::string::npos) b << notes.substr (n);
		return b.str ();
	};
	outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.html", Document (title, fullBody (true), project, office, false), "text/html" });
	outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.doc", Document (title, fullBody (true), project, office, true), "application/msword" });
	outs.push_back ({ base + "_A31_Paramasa-per-ofertim.doc", Document ("PARAMASA P\xC3\x8BR OFERTIM", fullBody (false), project, office, true), "application/msword" });
	for (const auto& p : parts) {
		const std::string name = Structure::BlockName (model, p.first);
		const std::string suffix = p.first == Structure::CommonPart () ? std::string ("PB") : p.first;
		outs.push_back ({ base + "_A30-" + Html::Slug (suffix) + "_Paramasa-dhe-parallogaria.doc",
						  Document (title + " \xE2\x80\x93 " + name, Body (p.second, true), project, office, true), "application/msword" });
	}
	outs.insert (outs.begin (), { base + "_A30_Paramasa-dhe-parallogaria.xlsx", BoqWorkbook (parts, model, project, office, true), kXlsxMime });
	outs.push_back ({ base + "_A31_Paramasa-per-ofertim.xlsx", BoqWorkbook (parts, model, project, office, false), kXlsxMime });
	return outs;
}

}	// namespace ARBS
