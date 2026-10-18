// =============================================================================
// ARBS Studio - Core : BillOfQuantities.cpp
// =============================================================================
#include "BillOfQuantities.hpp"

#include "Reports.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
			for (const std::string& g : { grp, std::string ("*") }) {
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

	// --- hapjet
	for (const Json::Value& o : model["openings"].AsArray ()) {
		const double w = o.GetNumber ("width"), h = o.GetNumber ("height");
		if (o.GetString ("kind") == "window") {
			add ("O:window:a", w * h, "");
			add ("O:window:n", 1.0, "");
			add ("O:window:w", w, "");
		} else {
			const std::string t = Lower (o.GetString ("type"));
			const char* cls = ContainsAny (t, { "garazh", "garage", "seksional", "sectional", "rolling", "overhead" }) ? "gar"
							: ContainsAny (t, { "hyr", "jasht", "entrance", "entry", "exterior", "external", "front", "sigurie", "security" }) ? "ext" : "int";
			add (std::string ("O:door:") + cls + ":n", 1.0, "");
			add ("O:door:n", 1.0, "");
		}
	}
	// dere hyrese: nese asnje dere nuk eshte klasifikuar si e jashtme, merret me e gjera ne perdhese
	if (q["O:door:ext:n"] == 0.0 && q["O:door:int:n"] > 0.0) {
		double best = 0.0;
		for (const Json::Value& o : model["openings"].AsArray ())
			if (o.GetString ("kind") == "door" && o.GetInt ("story") == 0) best = std::max (best, o.GetNumber ("width"));
		if (best >= 1.0) { q["O:door:ext:n"] = 1.0; q["O:door:int:n"] -= 1.0; }
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
	const double depth = basement ? 3.3 : 1.0;
	q["EXC:v"] = Round (gj * 1.15 * depth, 2);
	q["basement"] = basement ? 1.0 : 0.0;

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
	if (req["overrides"].IsObject ()) o.overrides = req["overrides"];
	return o;
}

BoqResult BillOfQuantities::Compute (const Catalog& cat, const Json::Value& model, const Json::Value& project, const BoqOptions& opt)
{
	BoqResult r;
	r.options = opt;
	const Json::Value& prices = cat.Prices ();
	r.priceLevel = prices.GetString ("priceLevel");
	r.source = prices.GetString ("source");

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
		r.groups.push_back (grp);
	}

	bool anyEstimated = false;
	for (const Json::Value& it : prices["items"].AsArray ()) {
		BoqLine line;
		line.code = it.GetString ("code");
		line.group = it.GetString ("g");
		line.unit = it.GetString ("u");
		line.desc = it.GetString ("d");

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
		double qty = 0.0, aSum = 0.0, vSum = 0.0;
		std::set<std::string> src;
		evaluate (it["q"], qty, aSum, vSum, src);
		if (qty <= 1e-9 && it["alt"].IsArray ()) {
			std::set<std::string> altSrc;
			evaluate (it["alt"], qty, aSum, vSum, altSrc);
			if (qty > 1e-9) { line.estimated = true; anyEstimated = true; src = altSrc; }
		}
		if (qty < 0.0) qty = 0.0;
		line.qty = Round (qty, UnitDecimals (line.unit));

		double price = it.GetNumber ("p");
		const double pcm = it.GetNumber ("pcm");
		if (pcm > 0.0 && aSum > 0.0) {
			line.thicknessCm = Round (vSum / aSum * 100.0, 1);
			price += pcm * line.thicknessCm;
		}
		if (opt.overrides.Has (line.code) && opt.overrides[line.code].IsNumber ()) {
			price = opt.overrides[line.code].AsNumber ();
			line.overridden = true;
		} else {
			price *= opt.factor;
		}
		line.price = Round (price, 2);
		line.value = Round (line.qty * line.price, 2);
		for (const std::string& s : src) line.source += (line.source.empty () ? "" : ", ") + s;

		if (line.qty <= 0.0 && !opt.includeZero) continue;
		for (BoqGroup& g : r.groups)
			if (g.id == line.group) { g.lines.push_back (line); g.total += line.value; break; }
	}
	r.groups.erase (std::remove_if (r.groups.begin (), r.groups.end (), [] (const BoqGroup& g) { return g.lines.empty (); }), r.groups.end ());

	for (const BoqGroup& g : r.groups) r.works += g.total;
	r.works = Round (r.works, 2);
	r.contingency = Round (r.works * opt.contingency / 100.0, 2);
	r.net = Round (r.works + r.contingency, 2);
	r.vat = Round (r.net * opt.vat / 100.0, 2);
	r.total = Round (r.net + r.vat, 2);
	r.bts = Q ("BTS");
	r.perM2 = r.bts > 0.0 ? Round (r.net / r.bts, 2) : 0.0;
	if (anyEstimated)
		r.notes.push_back ("Pozicionet e sh\xC3\xABnuara me * jan\xC3\xAB vler\xC3\xABsuar sepse modeli nuk p\xC3\xABrmban elementet p\xC3\xABrkat\xC3\xABse (p.sh. objektet sanitare, gjat\xC3\xABsit\xC3\xAB e streh\xC3\xABve).");
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
	return o;
}

// =============================================================================
// Dokumentet
// =============================================================================

namespace {

std::string Csv (const std::vector<std::vector<std::string>>& rows)
{
	std::string out = "\xEF\xBB\xBF";
	for (const auto& r : rows) {
		for (size_t i = 0; i < r.size (); ++i) {
			if (i > 0) out += ';';
			std::string v = r[i];
			const bool quote = v.find_first_of (";\"\n") != std::string::npos;
			size_t p = 0;
			while ((p = v.find ('"', p)) != std::string::npos) { v.insert (p, "\""); p += 2; }
			out += quote ? "\"" + v + "\"" : v;
		}
		out += "\r\n";
	}
	return out;
}

std::string LineDesc (const BoqLine& l)
{
	std::string d = Html::Esc (l.desc);
	if (l.thicknessCm > 0.0) d += " \xE2\x80\x93 d = " + Html::Num (l.thicknessCm, l.thicknessCm < 10.0 ? 1 : 0) + " cm";
	if (!l.source.empty ()) d += "<br><span class=\"note\">Sasia nga modeli: " + Html::Esc (l.source) + "</span>";
	return d;
}

std::string Body (const BoqResult& r, bool withPrices)
{
	std::ostringstream b;
	b << "<p>" << (withPrices
		? "Paramasa dhe parallogaria \xC3\xABsht\xC3\xAB hartuar n\xC3\xAB baz\xC3\xAB t\xC3\xAB sasive t\xC3\xAB nxjerra nga modeli BIM n\xC3\xAB Archicad (kompozitat sipas shtresave, elementet konstruktive, zonat, dyert dhe dritaret) dhe t\xC3\xAB \xC3\xA7mimeve nj\xC3\xABsi orientuese t\xC3\xAB tregut n\xC3\xAB Republik\xC3\xABn e Kosov\xC3\xABs"
		: "Paramasa p\xC3\xABr ofertim \xC3\xABsht\xC3\xAB hartuar n\xC3\xAB baz\xC3\xAB t\xC3\xAB sasive t\xC3\xAB nxjerra nga modeli BIM n\xC3\xAB Archicad. Ofertuesi plot\xC3\xABson \xC3\xA7mimin nj\xC3\xABsi p\xC3\xABr \xC3\xA7" "do pozicion; \xC3\xA7mimi p\xC3\xABrfshin materialin, pun\xC3\xABn, transportin dhe t\xC3\xAB gjitha punimet ndihm\xC3\xABse")
	  << (withPrices && !r.priceLevel.empty () ? " (niveli i \xC3\xA7mimeve " + Html::Esc (r.priceLevel) + ")" : "") << ".</p>";
	if (withPrices)
		b << "<p class=\"note\">" << Html::Esc (r.source) << " \xC3\x87mimet jan\xC3\xAB pa TVSH"
		  << (std::fabs (r.options.factor - 1.0) > 1e-9 ? ", me koeficient " + Html::Num (r.options.factor, 2) : std::string ())
		  << ". Pozicionet me \xC3\xA7mim t\xC3\xAB ndryshuar nga zyra sh\xC3\xABnohen me \xE2\x80\xA0.</p>";

	for (const BoqGroup& g : r.groups) {
		b << "<h2>" << Html::Esc (g.id) << ". " << Html::Esc (g.name) << "</h2><table><tr><th style=\"width:7%\">Nr.</th><th>P\xC3\xABrshkrimi i punimeve</th><th class=\"c\" style=\"width:7%\">Nj\xC3\xABsia</th><th class=\"n\" style=\"width:10%\">Sasia</th>";
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

	b << "<h2>Rekapitulimi</h2><table><tr><th style=\"width:7%\">Nr.</th><th>Grupi i punimeve</th><th class=\"n\" style=\"width:18%\">Vlera (\xE2\x82\xAC)</th>"
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

	b << "<h2>V\xC3\xABrejtje</h2><ul>";
	b << "<li class=\"note\">Sasit\xC3\xAB jan\xC3\xAB llogaritur nga modeli: sip\xC3\xABrfaqja e elementeve me kompozita ARBS \xC3\x97 trash\xC3\xABsia e shtresave; kolonat, trar\xC3\xABt dhe elementet bazike nga v\xC3\xABllimet e Archicad-it; dyert dhe dritaret nga dimensionet e hapjeve. Para tenderimit, sasit\xC3\xAB kontrollohen me vizatimet e projektit zbatues.</li>";
	b << "<li class=\"note\">Armatura \xC3\xABsht\xC3\xAB vler\xC3\xABsuar me konsum mesatar kg/m\xC2\xB3 sipas llojit t\xC3\xAB elementit; sasia p\xC3\xABrfundimtare merret nga projekti i konstruksionit (lista e armatur\xC3\xABs).</li>";
	b << "<li class=\"note\">Instalimet (elektrike, uj\xC3\xAB-kanalizim, ngrohje) jan\xC3\xAB vler\xC3\xABsim p\xC3\xABr m\xC2\xB2 BTS deri n\xC3\xAB hartimin e projekteve t\xC3\xAB instalimeve.</li>";
	for (const std::string& n : r.notes) b << "<li class=\"note\">" << Html::Esc (n) << "</li>";
	b << "</ul>";
	return b.str ();
}

}	// namespace

std::vector<ReportOutput> Reports::QuantitiesAndCost (const Catalog& cat, const Json::Value& project, const Json::Value& office,
													  const Json::Value& model, const BoqOptions& opt)
{
	const BoqResult r = BillOfQuantities::Compute (cat, model, project, opt);
	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PKR"));
	const std::string title = "PARAMASA DHE PARALLOGARIA";
	const std::string body = Body (r, true);
	outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.html", Document (title, body, project, office, false), "text/html" });
	outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.doc", Document (title, body, project, office, true), "application/msword" });
	outs.push_back ({ base + "_A31_Paramasa-per-ofertim.doc", Document ("PARAMASA P\xC3\x8BR OFERTIM", Body (r, false), project, office, true), "application/msword" });

	std::vector<std::vector<std::string>> csv;
	csv.push_back ({ "Grupi", "Nr.", "P\xC3\xABrshkrimi", "Nj\xC3\xABsia", "Sasia", "\xC3\x87mimi nj\xC3\xABsi (EUR)", "Vlera (EUR)", "Burimi n\xC3\xAB model", "Vler\xC3\xABsim" });
	for (const BoqGroup& g : r.groups) {
		for (const BoqLine& l : g.lines)
			csv.push_back ({ g.id + ". " + g.name, l.code, l.desc + (l.thicknessCm > 0.0 ? " - d = " + Html::Num (l.thicknessCm, 1) + " cm" : std::string ()), l.unit,
							 Html::Num (l.qty, UnitDecimals (l.unit)), Html::Num (l.price, 2), Html::Num (l.value, 2), l.source, l.estimated ? "po" : "" });
		csv.push_back ({ g.id + ". " + g.name, "", "GJITHSEJ " + g.id, "", "", "", Html::Num (g.total, 2), "", "" });
	}
	csv.push_back ({ "", "", "Gjithsej punimet", "", "", "", Html::Num (r.works, 2), "", "" });
	csv.push_back ({ "", "", "T\xC3\xAB paparashikuarat " + Html::Num (r.options.contingency, 1) + " %", "", "", "", Html::Num (r.contingency, 2), "", "" });
	csv.push_back ({ "", "", "Gjithsej pa TVSH", "", "", "", Html::Num (r.net, 2), "", "" });
	csv.push_back ({ "", "", "TVSH " + Html::Num (r.options.vat, 1) + " %", "", "", "", Html::Num (r.vat, 2), "", "" });
	csv.push_back ({ "", "", "GJITHSEJ ME TVSH", "", "", "", Html::Num (r.total, 2), "", "" });
	outs.push_back ({ base + "_A30_Paramasa-dhe-parallogaria.csv", Csv (csv), "text/csv" });
	return outs;
}

}	// namespace ARBS
