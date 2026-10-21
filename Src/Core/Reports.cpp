// =============================================================================
// ARBS Studio - Core : Reports.cpp
// =============================================================================
#include "Reports.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace ARBS {

static const double kPi = 3.14159265358979323846;

// =============================================================================
// Html helpers
// =============================================================================

std::string Html::Esc (const std::string& s)
{
	std::string out;
	out.reserve (s.size () + 16);
	for (char c : s) {
		switch (c) {
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '"': out += "&quot;"; break;
			default: out += c;
		}
	}
	return out;
}

std::string Html::Num (double v, int decimals)
{
	char fmt[8];
	std::snprintf (fmt, sizeof (fmt), "%%.%df", decimals);
	char buf[64];
	std::snprintf (buf, sizeof (buf), fmt, v);
	std::string s = buf;
	for (char& c : s) if (c == '.') c = ',';
	if (s == "-0" || s == "-0,0" || s == "-0,00") s = s.substr (1);
	return s;
}

std::string Html::Slug (const std::string& s)
{
	// zevendeson shkronjat shqipe me diakritike dhe karakteret e palejuara ne emra skedaresh
	std::string out;
	for (size_t i = 0; i < s.size (); ++i) {
		unsigned char c = static_cast<unsigned char> (s[i]);
		if (c == 0xC3 && i + 1 < s.size ()) {
			unsigned char d = static_cast<unsigned char> (s[i + 1]);
			if (d == 0xAB) out += 'e';			// e
			else if (d == 0x8B) out += 'E';		// E
			else if (d == 0xA7) out += 'c';		// c
			else if (d == 0x87) out += 'C';		// C
			else out += '_';
			++i;
			continue;
		}
		if (c >= 0x80) continue;
		if (std::isalnum (c) || c == '-' || c == '_' || c == '.') out += static_cast<char> (c);
		else if (c == ' ' || c == '/' || c == '\\' || c == ':') out += '-';
	}
	std::string clean;
	for (char c : out) {
		if (c == '-' && !clean.empty () && clean.back () == '-') continue;
		clean += c;
	}
	return clean.empty () ? "dokument" : clean;
}

static std::string Lookup (const std::string& key, const Json::Value& project, const Json::Value& office)
{
	if (key.rfind ("zyra.", 0) == 0) return office.GetString (key.substr (5));
	if (key.rfind ("glaser.", 0) == 0) return project["_glaser"].GetString (key.substr (7));
	return project.GetString (key);
}

std::string Html::Fill (const std::string& tmpl, const Json::Value& project, const Json::Value& office)
{
	std::string out;
	size_t pos = 0;
	while (pos < tmpl.size ()) {
		size_t a = tmpl.find ("{{", pos);
		if (a == std::string::npos) { out += Esc (tmpl.substr (pos)); break; }
		size_t b = tmpl.find ("}}", a + 2);
		if (b == std::string::npos) { out += Esc (tmpl.substr (pos)); break; }
		out += Esc (tmpl.substr (pos, a - pos));
		const std::string key = tmpl.substr (a + 2, b - a - 2);
		const std::string value = Lookup (key, project, office);
		if (value.empty ())
			out += "<span class=\"mungon\">[" + Esc (key) + "]</span>";
		else
			out += Esc (value);
		pos = b + 2;
	}
	return out;
}

// =============================================================================
// Dokumenti baze
// =============================================================================

static const char* kCss = R"CSS(
  @page { size: A4; margin: 18mm 16mm 18mm 20mm; }
  @page landscape { size: A3 landscape; margin: 12mm; }
  body { font-family: "Segoe UI", Calibri, Arial, sans-serif; font-size: 10.5pt; color: #1d2733; line-height: 1.45; margin: 0; padding: 18px; background: #ffffff; }
  .page-land { page: landscape; }
  h1 { font-size: 17pt; color: #002142; margin: 6px 0 2px 0; letter-spacing: .5px; }
  h2 { font-size: 12.5pt; color: #135027; border-bottom: 1.5px solid #135027; padding-bottom: 2px; margin: 20px 0 8px 0; }
  h3 { font-size: 11pt; color: #002142; margin: 14px 0 6px 0; }
  p { margin: 4px 0 6px 0; text-align: justify; }
  table { border-collapse: collapse; width: 100%; margin: 6px 0 10px 0; }
  th, td { border: 0.6pt solid #9aa6b2; padding: 3px 6px; vertical-align: top; font-size: 9.5pt; }
  th { background: #e8efe9; color: #002142; text-align: left; font-weight: 600; }
  td.n, th.n { text-align: right; white-space: nowrap; }
  td.c, th.c { text-align: center; }
  tr.sum td { font-weight: 700; background: #f3f6f4; }
  tr.sub td { font-weight: 600; background: #f8faf9; }
  .hdr td { border: none; padding: 0; vertical-align: middle; }
  .hdr .office { text-align: right; font-size: 8.5pt; color: #4a5866; }
  .proj td { border: none; border-bottom: 0.5pt solid #d5dbe1; font-size: 9.5pt; }
  .proj td.k { color: #5c6b79; width: 26%; }
  .ok { color: #135027; font-weight: 700; }
  .ko { color: #b3261e; font-weight: 700; }
  .mungon { background: #fff3b0; color: #6b5800; padding: 0 2px; }
  .note { font-size: 8.5pt; color: #5c6b79; }
  .sign td { border: none; padding-top: 28px; font-size: 9.5pt; }
  .sign .line { border-top: 0.6pt solid #1d2733; padding-top: 3px; }
  .badge { display: inline-block; padding: 0 6px; border-radius: 3px; background: #e8efe9; color: #135027; font-size: 8.5pt; }
  .legend span { display: inline-block; margin-right: 14px; font-size: 8.5pt; }
  .legend i { display: inline-block; width: 18px; height: 9px; margin-right: 4px; vertical-align: middle; }
  svg text { font-family: "Segoe UI", Calibri, Arial, sans-serif; }
  .footer { margin-top: 26px; font-size: 7.5pt; color: #8a96a3; border-top: 0.5pt solid #d5dbe1; padding-top: 4px; }
)CSS";

std::string Reports::Document (const std::string& title, const std::string& body, const Json::Value& project,
							   const Json::Value& office, bool forWord, bool landscape)
{
	std::ostringstream h;
	if (forWord) {
		h << "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:w=\"urn:schemas-microsoft-com:office:word\" xmlns=\"http://www.w3.org/TR/REC-html40\">";
	} else {
		h << "<!DOCTYPE html><html lang=\"sq\">";
	}
	h << "<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
	h << "<title>" << Html::Esc (title) << "</title>";
	if (forWord)
		h << "<!--[if gte mso 9]><xml><w:WordDocument><w:View>Print</w:View><w:Zoom>100</w:Zoom></w:WordDocument></xml><![endif]-->";
	h << "<style>" << kCss;
	if (landscape) h << " @page { size: A3 landscape; margin: 10mm; } ";
	h << "</style></head><body" << (landscape ? " class=\"page-land\"" : "") << ">";

	// koka: logoja + te dhenat e zyres
	const std::string logo = office.GetString ("logo");
	h << "<table class=\"hdr\"><tr><td style=\"width:40%\">";
	if (!logo.empty ())
		h << "<img src=\"" << Html::Esc (logo) << "\" alt=\"logo\" style=\"height:58px\">";
	else
		h << "<b style=\"font-size:16pt;color:#135027\">" << Html::Esc (office.GetString ("kompania", "ARBS Studio")) << "</b>";
	h << "</td><td class=\"office\">";
	h << "<b>" << Html::Esc (office.GetString ("kompania", "ARBS Studio")) << "</b><br>";
	for (const char* key : { "adresa", "telefoni", "email", "web" }) {
		const std::string v = office.GetString (key);
		if (!v.empty ()) h << Html::Esc (v) << "<br>";
	}
	h << "</td></tr></table>";

	h << "<h1>" << Html::Esc (title) << "</h1>";
	h << "<table class=\"proj\">";
	auto row = [&] (const char* label, const std::string& v) {
		h << "<tr><td class=\"k\">" << label << "</td><td>" << (v.empty () ? "<span class=\"mungon\">&nbsp;&nbsp;&nbsp;&nbsp;</span>" : Html::Esc (v)) << "</td></tr>";
	};
	row ("Investitori", project.GetString ("investitori"));
	row ("Objekti", project.GetString ("emri") + (project.GetString ("destinimi").empty () ? "" : " \xE2\x80\x93 " + project.GetString ("destinimi")));
	std::string loc = project.GetString ("vendi");
	if (!project.GetString ("komuna").empty ()) loc += (loc.empty () ? "" : ", ") + std::string ("Komuna ") + project.GetString ("komuna");
	if (!project.GetString ("zona_kadastrale").empty ()) loc += ", ZK " + project.GetString ("zona_kadastrale");
	if (!project.GetString ("parcela").empty ()) loc += ", P. nr. " + project.GetString ("parcela");
	row ("Lokacioni", loc);
	row ("Faza", project.GetString ("faza"));
	row ("Nr. i projektit / Data", project.GetString ("nr") + (project.GetString ("data").empty () ? "" : "  \xC2\xB7  " + project.GetString ("data")));
	h << "</table>";

	h << body;

	// nenshkrimet
	h << "<table class=\"sign\"><tr><td style=\"width:50%\"></td><td>";
	h << "<div>Arkitekti p\xC3\xABrgjegj\xC3\xABs:</div><div style=\"height:34px\"></div><div class=\"line\">"
	  << Html::Esc (office.GetString ("titulli")) << " " << Html::Esc (office.GetString ("arkitekti"))
	  << (office.GetString ("licenca").empty () ? "" : "<br><span class=\"note\">Licenca nr. " + Html::Esc (office.GetString ("licenca")) + "</span>")
	  << "</div></td></tr></table>";
	h << "<div class=\"footer\">Gjeneruar me ARBS Studio p\xC3\xABr Archicad \xC2\xB7 " << Html::Esc (office.GetString ("kompania", "ARBS Studio")) << "</div>";
	h << "</body></html>";
	return h.str ();
}

// =============================================================================
// Blloqet e perbashketa
// =============================================================================

// shkurton tekstin UTF-8 ne max "maxChars" karaktere pa e prere nje karakter shumebajtesh
static std::string Utf8Truncate (const std::string& s, size_t maxChars)
{
	size_t chars = 0, i = 0;
	while (i < s.size ()) {
		const unsigned char c = static_cast<unsigned char> (s[i]);
		const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
		if (chars + 1 > maxChars - 1 && i + len < s.size ()) return s.substr (0, i) + "\xE2\x80\xA6";
		i += len;
		++chars;
	}
	return s;
}

static std::string StoryName (const Json::Value& model, int story)
{
	for (const Json::Value& s : model["stories"].AsArray ())
		if (s.GetInt ("index") == story) return s.GetString ("name");
	if (story == 0) return "P\xC3\xABrdhesa";
	if (story < 0) return "Bodrumi " + std::to_string (-story);
	return "Kati " + std::to_string (story);
}

static std::string ZonesTable (const Json::Value& model, double* totalOut = nullptr)
{
	std::map<int, std::vector<const Json::Value*>> byStory;
	for (const Json::Value& z : model["zones"].AsArray ())
		byStory[z.GetInt ("story")].push_back (&z);
	std::ostringstream h;
	if (byStory.empty ()) {
		h << "<p class=\"note\">Nuk u gjet\xC3\xABn zona (hap\xC3\xABsira) n\xC3\xAB model. Vendosni zonat me em\xC3\xABr dhe num\xC3\xABr q\xC3\xAB tabela t\xC3\xAB plot\xC3\xABsohet automatikisht.</p>";
		if (totalOut) *totalOut = 0.0;
		return h.str ();
	}
	h << "<table><tr><th style=\"width:12%\">Nr.</th><th>Hap\xC3\xABsira</th><th class=\"n\" style=\"width:18%\">Sip\xC3\xABrfaqja (m\xC2\xB2)</th></tr>";
	double total = 0.0;
	for (auto& kv : byStory) {
		h << "<tr class=\"sub\"><td colspan=\"3\">" << Html::Esc (StoryName (model, kv.first)) << "</td></tr>";
		double sub = 0.0;
		std::vector<const Json::Value*> zs = kv.second;
		std::sort (zs.begin (), zs.end (), [] (const Json::Value* a, const Json::Value* b) { return a->GetString ("number") < b->GetString ("number"); });
		for (const Json::Value* z : zs) {
			const double a = z->GetNumber ("area");
			sub += a;
			h << "<tr><td>" << Html::Esc (z->GetString ("number")) << "</td><td>" << Html::Esc (z->GetString ("name")) << "</td><td class=\"n\">" << Html::Num (a) << "</td></tr>";
		}
		h << "<tr class=\"sub\"><td></td><td>Gjithsej " << Html::Esc (StoryName (model, kv.first)) << "</td><td class=\"n\">" << Html::Num (sub) << "</td></tr>";
		total += sub;
	}
	h << "<tr class=\"sum\"><td></td><td>SIP\xC3\x8BRFAQJA NETO GJITHSEJ</td><td class=\"n\">" << Html::Num (total) << "</td></tr></table>";
	if (totalOut) *totalOut = total;
	return h.str ();
}

static std::vector<const Composite*> SelectComposites (const Catalog& cat, const Json::Value& model, const std::vector<std::string>& codes, bool envelopeOnlyFallback)
{
	std::vector<const Composite*> out;
	std::set<std::string> seen;
	auto add = [&] (const std::string& code) {
		const Composite* c = cat.FindComposite (code);
		if (c != nullptr && seen.insert (code).second) out.push_back (c);
	};
	for (const std::string& code : codes) add (code);
	if (out.empty ())
		for (const Json::Value& c : model["composites"].AsArray ()) add (c.GetString ("code"));
	if (out.empty () && envelopeOnlyFallback)
		for (const Composite& c : cat.Composites ())
			if (!c.umaxKey.empty ()) add (c.code);
	return out;
}

static std::string LayersInline (const Composite& c)
{
	std::ostringstream h;
	bool first = true;
	for (const Layer& l : c.layers) {
		if (!first) h << " + ";
		first = false;
		h << Html::Num (l.thickness * 100.0, l.thickness * 100.0 < 1.0 ? 1 : (std::fmod (l.thickness * 1000.0, 10.0) > 0.01 ? 1 : 0)) << " cm " << Html::Esc (l.material->name);
	}
	return h.str ();
}

static double ModelArea (const Json::Value& model, const std::string& code)
{
	double a = 0.0;
	for (const Json::Value& c : model["composites"].AsArray ())
		if (c.GetString ("code") == code) a += c.GetNumber ("area");
	return a;
}

static std::string UTable (const Catalog& cat, const std::vector<const Composite*>& comps, const Json::Value& model)
{
	std::ostringstream h;
	h << "<table><tr><th style=\"width:9%\">Kodi</th><th>Elementi</th><th class=\"n\">A (m\xC2\xB2)</th><th class=\"n\">U (W/m\xC2\xB2K)</th><th class=\"n\">U<sub>max</sub></th><th class=\"c\">Vler\xC3\xABsimi</th></tr>";
	for (const Composite* c : comps) {
		const UResult u = BuildingPhysics::ComputeU (*c, cat.UMax (c->umaxKey));
		const double area = ModelArea (model, c->code);
		h << "<tr><td>" << Html::Esc (c->code) << "</td><td>" << Html::Esc (c->name)
		  << (c->umaxKey.empty () ? "" : "<br><span class=\"note\">" + Html::Esc (cat.UMaxLabel (c->umaxKey)) + "</span>")
		  << "</td><td class=\"n\">" << (area > 0.0 ? Html::Num (area, 1) : "\xE2\x80\x93") << "</td><td class=\"n\">" << Html::Num (u.u, 3) << "</td><td class=\"n\">"
		  << (u.umax > 0.0 ? Html::Num (u.umax, 2) : "\xE2\x80\x93") << "</td><td class=\"c\">"
		  << (u.umax <= 0.0 ? "<span class=\"note\">pa k\xC3\xABrkes\xC3\xAB</span>" : (u.compliant ? "<span class=\"ok\">Plot\xC3\xABson</span>" : "<span class=\"ko\">Nuk plot\xC3\xABson</span>"))
		  << "</td></tr>";
	}
	h << "</table><p class=\"note\">Burimi i U<sub>max</sub>: " << Html::Esc (cat.UMaxSource ()) << ".</p>";
	return h.str ();
}

// =============================================================================
// Pershkrimi teknik
// =============================================================================

ReportOutput Reports::TechnicalDescription (const Catalog& cat, const Json::Value& project, const Json::Value& office, const Json::Value& model)
{
	const Json::Value& td = cat.Documents ()["technicalDescription"];
	std::ostringstream b;
	double netTotal = 0.0;
	const std::string zones = ZonesTable (model, &netTotal);
	const std::vector<const Composite*> comps = SelectComposites (cat, model, {}, false);

	for (const Json::Value& sec : td["sections"].AsArray ()) {
		b << "<h2>" << Html::Esc (sec.GetString ("id")) << ". " << Html::Esc (sec.GetString ("title")) << "</h2>";
		for (const Json::Value& line : sec["body"].AsArray ()) {
			const std::string t = line.AsString ();
			if (t == "[[LEGJISLACIONI]]") {
				b << "<ul>";
				for (const Json::Value& l : cat.Documents ()["legislation"].AsArray ())
					b << "<li>" << Html::Esc (l.AsString ()) << "</li>";
				b << "</ul>";
			} else if (t == "[[TREGUESIT]]") {
				const double parcel = project.GetNumber ("sip_parceles");
				const double foot = project.GetNumber ("gjurma");
				const double bts = project.GetNumber ("bts");
				b << "<table><tr><th>Treguesi</th><th class=\"n\" style=\"width:24%\">Vlera</th></tr>";
				b << "<tr><td>Sip\xC3\xABrfaqja e parcel\xC3\xABs</td><td class=\"n\">" << (parcel > 0 ? Html::Num (parcel) + " m\xC2\xB2" : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Gjurma e objektit</td><td class=\"n\">" << (foot > 0 ? Html::Num (foot) + " m\xC2\xB2" : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Sip\xC3\xABrfaqja bruto e nd\xC3\xABrtimit (BTS)</td><td class=\"n\">" << (bts > 0 ? Html::Num (bts) + " m\xC2\xB2" : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Sip\xC3\xABrfaqja neto (nga zonat e modelit)</td><td class=\"n\">" << (netTotal > 0 ? Html::Num (netTotal) + " m\xC2\xB2" : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Indeksi i shfryt\xC3\xABzimit t\xC3\xAB parcel\xC3\xABs (gjurma / parcela)</td><td class=\"n\">" << (parcel > 0 && foot > 0 ? Html::Num (foot / parcel, 2) : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Indeksi i nd\xC3\xABrtimit (BTS / parcela)</td><td class=\"n\">" << (parcel > 0 && bts > 0 ? Html::Num (bts / parcel, 2) : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Kategoria e nd\xC3\xABrtimit</td><td class=\"n\">" << Html::Esc (project.GetString ("kategoria", "\xE2\x80\x93")) << "</td></tr>";
				b << "<tr><td>Destinimi</td><td class=\"n\">" << Html::Esc (project.GetString ("destinimi_emri", project.GetString ("destinimi", "\xE2\x80\x93")))
				  << (project.GetString ("nen_destinimi").empty () ? std::string () : " \xE2\x80\x93 " + Html::Esc (project.GetString ("nen_destinimi"))) << "</td></tr>";
				b << "<tr><td>Etazhiteti</td><td class=\"n\">" << Html::Esc (project.GetString ("katet", "\xE2\x80\x93")) << "</td></tr>";
				b << "<tr><td>Lart\xC3\xABsia e objektit</td><td class=\"n\">" << (project.GetNumber ("lartesia") > 0 ? Html::Num (project.GetNumber ("lartesia")) + " m" : "\xE2\x80\x93") << "</td></tr>";
				b << "<tr><td>Vendparkingjet</td><td class=\"n\">" << Html::Esc (project.GetString ("parkingu", "\xE2\x80\x93")) << "</td></tr>";
				b << "</table>";
			} else if (t == "[[TABELA_ZONAVE]]") {
				b << zones;
				const std::string blocks = BlocksChapter (project, model);
				if (!blocks.empty ())
					b << "<h3>Struktura e projektit sipas blloqeve, hyrjeve dhe nj\xC3\xABsive</h3>" << blocks;
			} else if (t == "[[TABELA_KOMPOZITAVE]]") {
				if (comps.empty ()) {
					b << "<p class=\"note\">N\xC3\xAB model nuk u gjet\xC3\xABn kompozita ARBS (kodet MJ-, MT-, MB-, MN-, DY-, KR-, KP-, TH-). Krijoni kompozitat nga paleta ARBS dhe p\xC3\xABrdorini n\xC3\xAB mure, pllaka dhe kulme.</p>";
				} else {
					b << "<table><tr><th style=\"width:9%\">Kodi</th><th>Kompozita / shtresat</th><th class=\"n\">d (cm)</th></tr>";
					for (const Composite* c : comps)
						b << "<tr><td>" << Html::Esc (c->code) << "</td><td><b>" << Html::Esc (c->name) << "</b><br>" << LayersInline (*c)
						  << "</td><td class=\"n\">" << Html::Num (c->TotalThickness () * 100.0, 1) << "</td></tr>";
					b << "</table>";
				}
			} else if (t == "[[TABELA_HAPJEVE]]") {
				int doors = 0, windows = 0;
				for (const Json::Value& o : model["openings"].AsArray ())
					(o.GetString ("kind") == "door" ? doors : windows)++;
				b << "<table><tr><th>Lloji</th><th class=\"n\" style=\"width:20%\">Sasia (cop\xC3\xAB)</th></tr>"
				  << "<tr><td>Dyer</td><td class=\"n\">" << doors << "</td></tr><tr><td>Dritare</td><td class=\"n\">" << windows << "</td></tr></table>";
			} else if (t == "[[TABELA_U]]") {
				std::vector<const Composite*> env;
				for (const Composite* c : comps) if (!c->umaxKey.empty ()) env.push_back (c);
				if (env.empty ()) b << "<p class=\"note\">Tabela plot\xC3\xABsohet pasi kompozitat ARBS t\xC3\xAB p\xC3\xABrdoren n\xC3\xAB model.</p>";
				else b << UTable (cat, env, model);
			} else {
				b << "<p>" << Html::Fill (t, project, office) << "</p>";
			}
		}
	}

	ReportOutput out;
	const std::string title = td.GetString ("title", "P\xC3\x8BRSHKRIMI TEKNIK");
	out.content = Document (title, b.str (), project, office, true);
	out.fileName = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PI")) + "_T01_Pershkrimi-teknik.doc";
	out.mime = "application/msword";
	return out;
}

// =============================================================================
// Fizika e nderteses
// =============================================================================

std::string Reports::CompositeSectionSvg (const Composite& c, int width, int height)
{
	// prerje skematike e kompozites (shtresat proporcionale, min 4 px)
	const double total = c.TotalThickness ();
	std::vector<double> px;
	double sum = 0.0;
	const double usable = width - 20.0;
	for (const Layer& l : c.layers) { double w = std::max (4.0, usable * l.thickness / std::max (1e-6, total)); px.push_back (w); sum += w; }
	const double scale = usable / sum;
	std::ostringstream s;
	s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">";
	double x = 10.0;
	const int barH = height - 46;
	for (size_t i = 0; i < c.layers.size (); ++i) {
		const Layer& l = c.layers[i];
		const double w = px[i] * scale;
		const std::string& g = l.material->group;
		const char* fill = "#d9d4c7";
		if (g == "Konstruksion") fill = "#b9bfc6";
		else if (g == "Murature") fill = "#d99a73";
		else if (g == "Termoizolim") fill = "#f4e27a";
		else if (g == "Membrana") fill = "#3b4a5a";
		else if (g == "Suva" || g == "Fasad\xC3\xAB") fill = "#ece7dc";
		else if (g == "Dysheme") fill = "#c8b28c";
		else if (g == "Dru") fill = "#b8875a";
		else if (g == "Dheu") fill = "#a59a83";
		else if (g == "Aj\xC3\xABr" || g == "Mbules\xC3\xAB") fill = "#ffffff";
		s << "<rect x=\"" << x << "\" y=\"8\" width=\"" << w << "\" height=\"" << barH << "\" fill=\"" << fill << "\" stroke=\"#1d2733\" stroke-width=\"0.6\"" << (l.ventilated ? " stroke-dasharray=\"3 2\"" : "") << "/>";
		if (g == "Termoizolim") {
			for (double yy = 12; yy < barH; yy += 8)
				s << "<path d=\"M" << x + 1 << " " << yy << " q " << w / 4 << " -4 " << w / 2 << " 0 t " << w / 2 - 2 << " 0\" fill=\"none\" stroke=\"#9a8a2a\" stroke-width=\"0.5\"/>";
		}
		s << "<text x=\"" << x + w / 2 << "\" y=\"" << barH + 22 << "\" font-size=\"9\" text-anchor=\"middle\" fill=\"#1d2733\">" << (i + 1) << "</text>";
		x += w;
	}
	const bool wall = c.usage == "wall";
	s << "<text x=\"10\" y=\"" << height - 4 << "\" font-size=\"9\" fill=\"#5c6b79\">" << (wall ? "JASHT\xC3\x8B" : "LART") << "</text>";
	s << "<text x=\"" << width - 10 << "\" y=\"" << height - 4 << "\" font-size=\"9\" text-anchor=\"end\" fill=\"#5c6b79\">" << (wall ? "BRENDA" : "POSHT\xC3\x8B") << "</text>";
	s << "</svg>";
	return s.str ();
}

std::string Reports::GlaserSvg (const GlaserResult& g, int width, int height)
{
	if (g.points.size () < 2) return "";
	const double xMax = g.points.back ().x;
	double pMax = 0.0, tMin = 1e9, tMax = -1e9;
	for (const GlaserInterface& p : g.points) {
		pMax = std::max ({ pMax, p.psat, p.p });
		tMin = std::min (tMin, p.theta);
		tMax = std::max (tMax, p.theta);
	}
	pMax *= 1.08;
	tMin = std::floor (tMin / 5.0) * 5.0 - 5.0;
	tMax = std::ceil (tMax / 5.0) * 5.0 + 5.0;
	const double L = 54, R = 54, T = 16, B = 36;
	const double W = width - L - R, H = height - T - B;
	auto X = [&] (double x) { return L + W * x / std::max (1e-6, xMax); };
	auto Yp = [&] (double p) { return T + H * (1.0 - p / pMax); };
	auto Yt = [&] (double t) { return T + H * (1.0 - (t - tMin) / (tMax - tMin)); };

	std::ostringstream s;
	s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">";
	// shtresat
	for (size_t i = 1; i < g.points.size (); ++i) {
		const double x0 = X (g.points[i - 1].x), x1 = X (g.points[i].x);
		s << "<rect x=\"" << x0 << "\" y=\"" << T << "\" width=\"" << std::max (0.5, x1 - x0) << "\" height=\"" << H << "\" fill=\"" << (i % 2 ? "#f4f6f8" : "#e9edf1") << "\" stroke=\"#c3cbd3\" stroke-width=\"0.5\"/>";
		s << "<text x=\"" << (x0 + x1) / 2 << "\" y=\"" << height - B + 12 << "\" font-size=\"8\" text-anchor=\"middle\" fill=\"#5c6b79\">" << i << "</text>";
	}
	if (g.condensation && g.planeFrom >= 0) {
		const double x0 = X (g.points[g.planeFrom].x), x1 = X (g.points[g.planeTo].x);
		s << "<rect x=\"" << x0 - 1.5 << "\" y=\"" << T << "\" width=\"" << std::max (3.0, x1 - x0 + 3) << "\" height=\"" << H << "\" fill=\"#3b82c4\" fill-opacity=\"0.25\"/>";
	}
	// boshtet
	s << "<line x1=\"" << L << "\" y1=\"" << T + H << "\" x2=\"" << L + W << "\" y2=\"" << T + H << "\" stroke=\"#1d2733\" stroke-width=\"0.8\"/>";
	for (int k = 0; k <= 4; ++k) {
		const double p = pMax * k / 4.0;
		s << "<text x=\"" << L - 4 << "\" y=\"" << Yp (p) + 3 << "\" font-size=\"8\" text-anchor=\"end\" fill=\"#b3261e\">" << static_cast<int> (p) << "</text>";
		const double t = tMin + (tMax - tMin) * k / 4.0;
		s << "<text x=\"" << L + W + 4 << "\" y=\"" << Yt (t) + 3 << "\" font-size=\"8\" fill=\"#135027\">" << Html::Num (t, 0) << "\xC2\xB0</text>";
	}
	s << "<text x=\"" << 4 << "\" y=\"" << T + 8 << "\" font-size=\"8\" fill=\"#b3261e\">p [Pa]</text>";
	s << "<text x=\"" << width - 4 << "\" y=\"" << T + 8 << "\" font-size=\"8\" text-anchor=\"end\" fill=\"#135027\">\xCE\xB8 [\xC2\xB0" "C]</text>";
	auto poly = [&] (const char* color, double dash, int which) {
		s << "<polyline fill=\"none\" stroke=\"" << color << "\" stroke-width=\"1.6\"" << (dash > 0 ? " stroke-dasharray=\"5 3\"" : "") << " points=\"";
		for (const GlaserInterface& p : g.points) {
			const double y = which == 0 ? Yp (p.psat) : which == 1 ? Yp (p.p) : Yt (p.theta);
			s << X (p.x) << "," << y << " ";
		}
		s << "\"/>";
	};
	poly ("#b3261e", 0, 0);		// presioni i ngopjes
	poly ("#2d5fa8", 1, 1);		// presioni i pjesshem
	poly ("#135027", 0, 2);		// temperatura
	s << "<text x=\"" << L << "\" y=\"" << height - 6 << "\" font-size=\"8\" fill=\"#5c6b79\">BRENDA</text>";
	s << "<text x=\"" << L + W << "\" y=\"" << height - 6 << "\" font-size=\"8\" text-anchor=\"end\" fill=\"#5c6b79\">JASHT\xC3\x8B</text>";
	s << "</svg>";
	return s.str ();
}

std::vector<ReportOutput> Reports::BuildingPhysics (const Catalog& cat, const Json::Value& projectIn, const Json::Value& office,
													const Json::Value& model, const std::vector<std::string>& codes)
{
	Json::Value project = projectIn;
	const GlaserClimate cl = cat.GlaserClimateFor (project.GetString ("komuna_id"));
	Json::Value gl = Json::Value::MakeObject ();
	gl.Set ("condDays", Html::Num (cl.condDays, 0));
	gl.Set ("thetaI", Html::Num (cl.thetaI, 0));
	gl.Set ("phiI", Html::Num (cl.phiI * 100.0, 0));
	gl.Set ("thetaE", Html::Num (cl.thetaE, 0));
	gl.Set ("phiE", Html::Num (cl.phiE * 100.0, 0));
	gl.Set ("dryDays", Html::Num (cl.dryDays, 0));
	gl.Set ("dryTheta", Html::Num (cl.dryTheta, 0));
	gl.Set ("dryPhi", Html::Num (cl.dryPhi * 100.0, 0));
	project.Set ("_glaser", gl);

	const std::vector<const Composite*> comps = SelectComposites (cat, model, codes, true);

	auto build = [&] (bool forWord) {
		std::ostringstream b;
		b << "<h2>1. Hyrje dhe metodologjia</h2>";
		for (const Json::Value& line : cat.Documents ()["buildingPhysicsIntro"].AsArray ())
			b << "<p>" << Html::Fill (line.AsString (), project, office) << "</p>";
		b << "<h2>2. P\xC3\xABrmbledhja e kontrollit t\xC3\xAB koeficientit U</h2>" << UTable (cat, comps, model);

		int n = 2;
		for (const Composite* c : comps) {
			++n;
			const UResult u = BuildingPhysics::ComputeU (*c, cat.UMax (c->umaxKey));
			const GlaserResult g = BuildingPhysics::Glaser (*c, cl);
			b << "<h2>" << n << ". " << Html::Esc (c->code) << " \xE2\x80\x93 " << Html::Esc (c->name) << "</h2>";
			if (!c->description.empty ()) b << "<p class=\"note\">" << Html::Esc (c->description) << "</p>";
			if (!forWord) b << CompositeSectionSvg (*c);
			b << "<table><tr><th class=\"c\">Nr.</th><th>Materiali</th><th class=\"n\">d (cm)</th><th class=\"n\">\xCE\xBB (W/mK)</th><th class=\"n\">R (m\xC2\xB2K/W)</th><th class=\"n\">\xCF\x81 (kg/m\xC2\xB3)</th><th class=\"n\">\xCE\xBC</th><th class=\"n\">s<sub>d</sub> (m)</th></tr>";
			for (size_t i = 0; i < c->layers.size (); ++i) {
				const Layer& l = c->layers[i];
				b << "<tr><td class=\"c\">" << (i + 1) << "</td><td>" << Html::Esc (l.material->name) << (l.ventilated ? " <span class=\"note\">(e ventiluar \xE2\x80\x93 nuk merret)</span>" : "")
				  << "</td><td class=\"n\">" << Html::Num (l.thickness * 100.0, l.thickness < 0.01 ? 2 : 1) << "</td><td class=\"n\">" << Html::Num (l.material->lambda, 3)
				  << "</td><td class=\"n\">" << Html::Num (u.layerR[i], 3) << "</td><td class=\"n\">" << Html::Num (l.material->density, 0)
				  << "</td><td class=\"n\">" << Html::Num (l.material->mu, 0) << "</td><td class=\"n\">" << Html::Num (l.ventilated ? 0.0 : l.thickness * l.material->mu, 2) << "</td></tr>";
			}
			b << "<tr class=\"sub\"><td></td><td>R<sub>si</sub> + R<sub>se</sub> (fluksi " << BuildingPhysics::FlowName (c->flow) << ", kufiri: " << BuildingPhysics::BoundaryName (c->boundary)
			  << ")</td><td class=\"n\">" << Html::Num (c->TotalThickness () * 100.0, 1) << "</td><td></td><td class=\"n\">" << Html::Num (u.rsi + u.rse, 3)
			  << "</td><td class=\"n\">" << Html::Num (u.arealMass, 0) << " kg/m\xC2\xB2</td><td></td><td class=\"n\">" << Html::Num (u.sd, 1) << "</td></tr>";
			b << "<tr class=\"sum\"><td></td><td colspan=\"3\">R<sub>T</sub> = " << Html::Num (u.rTotal, 3) << " m\xC2\xB2K/W   \xC2\xB7   U = " << Html::Num (u.u, 3) << " W/m\xC2\xB2K"
			  << (u.umax > 0 ? "   \xC2\xB7   U<sub>max</sub> = " + Html::Num (u.umax, 2) : std::string ()) << "</td><td colspan=\"4\" class=\"c\">"
			  << (u.umax <= 0 ? "pa k\xC3\xABrkes\xC3\xAB" : (u.compliant ? "<span class=\"ok\">PLOT\xC3\x8BSON</span>" : "<span class=\"ko\">NUK PLOT\xC3\x8BSON</span>")) << "</td></tr></table>";

			b << "<h3>Difuzioni i avullit t\xC3\xAB ujit (metoda Glaser)</h3>";
			if (g.points.empty ()) {
				b << "<p>" << Html::Esc (g.verdict) << "</p>";
				continue;
			}
			if (!forWord) {
				b << GlaserSvg (g);
				b << "<div class=\"legend\"><span><i style=\"background:#135027\"></i>temperatura \xCE\xB8</span><span><i style=\"background:#b3261e\"></i>presioni i ngopjes p<sub>s</sub></span><span><i style=\"background:#2d5fa8\"></i>presioni i pjessh\xC3\xABm p</span>"
				  << (g.condensation ? "<span><i style=\"background:#3b82c4;opacity:.4\"></i>zona e kondensimit</span>" : "") << "</div>";
			}
			b << "<table><tr><th class=\"c\">Ndarja</th><th>Pas shtres\xC3\xABs</th><th class=\"n\">\xCE\xB8 (\xC2\xB0" "C)</th><th class=\"n\">p<sub>s</sub> (Pa)</th><th class=\"n\">p (Pa)</th></tr>";
			for (size_t i = 0; i < g.points.size (); ++i) {
				const GlaserInterface& p = g.points[i];
				const bool cond = p.p > p.psat;
				b << "<tr><td class=\"c\">" << i << "</td><td>" << (i == 0 ? "Sip\xC3\xABrfaqja e brendshme" : Html::Esc (p.label)) << "</td><td class=\"n\">" << Html::Num (p.theta, 2)
				  << "</td><td class=\"n\">" << Html::Num (p.psat, 0) << "</td><td class=\"n\">" << (cond ? "<span class=\"ko\">" : "") << Html::Num (p.p, 0) << (cond ? "</span>" : "") << "</td></tr>";
			}
			b << "</table>";
			b << "<table><tr><th>Treguesi</th><th class=\"n\" style=\"width:24%\">Vlera</th></tr>"
			  << "<tr><td>Temperatura e sip\xC3\xABrfaqes s\xC3\xAB brendshme \xCE\xB8<sub>si</sub></td><td class=\"n\">" << Html::Num (g.surfaceTempInside, 2) << " \xC2\xB0" "C</td></tr>"
			  << "<tr><td>Faktori i temperatur\xC3\xABs f<sub>Rsi</sub> (rrezik myku n\xC3\xABse &lt; 0,70)</td><td class=\"n\">" << Html::Num (g.fRsi, 3) << "</td></tr>"
			  << "<tr><td>Sasia e kondensuar gjat\xC3\xAB periudh\xC3\xABs s\xC3\xAB kondensimit</td><td class=\"n\">" << Html::Num (g.condensedKgM2, 3) << " kg/m\xC2\xB2</td></tr>"
			  << "<tr><td>Sasia q\xC3\xAB thahet gjat\xC3\xAB periudh\xC3\xABs s\xC3\xAB tharjes</td><td class=\"n\">" << Html::Num (g.dryingKgM2, 3) << " kg/m\xC2\xB2</td></tr></table>";
			b << "<p><b class=\"" << (g.ok ? "ok" : "ko") << "\">" << Html::Esc (g.verdict) << "</b></p>";
		}
		return b.str ();
	};

	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PKR")) + "_F01_Fizika-nderteses";
	ReportOutput html;
	html.fileName = base + ".html";
	html.mime = "text/html";
	html.content = Document ("FIZIKA E ND\xC3\x8BRTES\xC3\x8BS \xE2\x80\x93 ELABORATI", build (false), project, office, false);
	outs.push_back (html);
	ReportOutput doc;
	doc.fileName = base + ".doc";
	doc.mime = "application/msword";
	doc.content = Document ("FIZIKA E ND\xC3\x8BRTES\xC3\x8BS \xE2\x80\x93 ELABORATI", build (true), project, office, true);
	outs.push_back (doc);
	return outs;
}

// =============================================================================
// Insolimi
// =============================================================================

std::string Reports::SunPathSvg (const Site& site, int year, double northDeg, int size)
{
	const double c = size / 2.0;
	const double R = size / 2.0 - 40.0;
	auto P = [&] (double az, double alt, double& x, double& y) {
		const double r = R * (90.0 - std::max (0.0, alt)) / 90.0;
		const double a = az * kPi / 180.0;
		x = c + r * std::sin (a);
		y = c - r * std::cos (a);
	};
	std::ostringstream s;
	s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << size << "\" height=\"" << size << "\" viewBox=\"0 0 " << size << " " << size << "\">";
	s << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>";
	for (int alt = 0; alt < 90; alt += 15) {
		const double r = R * (90.0 - alt) / 90.0;
		s << "<circle cx=\"" << c << "\" cy=\"" << c << "\" r=\"" << r << "\" fill=\"none\" stroke=\"" << (alt == 0 ? "#1d2733" : "#c3cbd3") << "\" stroke-width=\"" << (alt == 0 ? 1.2 : 0.6) << "\"/>";
		if (alt > 0) s << "<text x=\"" << c + 3 << "\" y=\"" << c - r + 10 << "\" font-size=\"8\" fill=\"#8a96a3\">" << alt << "\xC2\xB0</text>";
	}
	for (int az = 0; az < 360; az += 15) {
		double x, y;
		P (az, 0, x, y);
		s << "<line x1=\"" << c << "\" y1=\"" << c << "\" x2=\"" << x << "\" y2=\"" << y << "\" stroke=\"#e2e7ec\" stroke-width=\"0.5\"/>";
		const double a = az * kPi / 180.0;
		const double tx = c + (R + 14) * std::sin (a), ty = c - (R + 14) * std::cos (a) + 3;
		const char* lbl = nullptr;
		if (az == 0) lbl = "V"; else if (az == 90) lbl = "L"; else if (az == 180) lbl = "J"; else if (az == 270) lbl = "P";
		if (lbl) s << "<text x=\"" << tx << "\" y=\"" << ty << "\" font-size=\"13\" font-weight=\"700\" text-anchor=\"middle\" fill=\"#002142\">" << lbl << "</text>";
		else if (az % 45 == 0) s << "<text x=\"" << tx << "\" y=\"" << ty << "\" font-size=\"8\" text-anchor=\"middle\" fill=\"#8a96a3\">" << az << "\xC2\xB0</text>";
	}
	struct Curve { int m; int d; const char* color; const char* label; };
	const Curve curves[] = { { 6, 21, "#d97706", "21 qershor" }, { 5, 21, "#e9a53b", "" }, { 4, 21, "#c9a24a", "" },
							 { 3, 21, "#135027", "21 mars / 23 shtator" }, { 2, 21, "#5b8bb5", "" }, { 1, 21, "#3e6e99", "" },
							 { 12, 21, "#002142", "21 dhjetor" } };
	for (const Curve& cv : curves) {
		const Date d (year, cv.m, cv.d);
		const bool major = cv.label[0] != '\0';
		s << "<polyline fill=\"none\" stroke=\"" << cv.color << "\" stroke-width=\"" << (major ? 2.0 : 0.8) << "\" points=\"";
		for (double h = 3.0; h <= 22.0; h += 0.1) {
			const SunPosition p = Solar::Position (site, d, h);
			if (p.altitude < 0) continue;
			double x, y; P (p.azimuth, p.altitude, x, y);
			s << x << "," << y << " ";
		}
		s << "\"/>";
	}
	// vijat e oreve (ora e diellit ~ ora lokale) per 21 qershor dhe 21 dhjetor
	for (int hour = 5; hour <= 20; ++hour) {
		std::ostringstream pts;
		int count = 0;
		double lx = 0, ly = 0;
		for (int m = 1; m <= 12; ++m) {
			const Date d (year, m, 21);
			const SunPosition p = Solar::Position (site, d, hour);
			if (p.altitude < 0) continue;
			double x, y; P (p.azimuth, p.altitude, x, y);
			pts << x << "," << y << " ";
			if (m == 6) { lx = x; ly = y; }
			++count;
		}
		if (count < 2) continue;
		s << "<polyline fill=\"none\" stroke=\"#8a96a3\" stroke-width=\"0.5\" stroke-dasharray=\"2 2\" points=\"" << pts.str () << "\"/>";
		if (lx > 0) s << "<text x=\"" << lx << "\" y=\"" << ly - 4 << "\" font-size=\"8\" text-anchor=\"middle\" fill=\"#5c6b79\">" << hour << "h</text>";
	}
	// veriu i projektit
	if (std::fabs (northDeg) > 0.01) {
		const double a = -northDeg * kPi / 180.0;
		const double x2 = c + (R - 20) * std::sin (a), y2 = c - (R - 20) * std::cos (a);
		s << "<line x1=\"" << c << "\" y1=\"" << c << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\" stroke=\"#b3261e\" stroke-width=\"1.5\"/>";
		s << "<text x=\"" << x2 << "\" y=\"" << y2 - 4 << "\" font-size=\"9\" text-anchor=\"middle\" fill=\"#b3261e\">VP</text>";
	}
	s << "<circle cx=\"" << c << "\" cy=\"" << c << "\" r=\"2.5\" fill=\"#002142\"/>";
	s << "</svg>";
	return s.str ();
}

std::vector<ReportOutput> Reports::Insolation (const Catalog& cat, const Json::Value& projectIn, const Json::Value& office, const InsolationOptions& opt)
{
	Site site = cat.SiteFor (opt.cityId);
	if (opt.latitude != 0.0 && opt.longitude != 0.0) {
		site.latitude = opt.latitude;
		site.longitude = opt.longitude;
		if (opt.altitude != 0.0) site.altitude = opt.altitude;
	}
	Json::Value project = projectIn;
	project.Set ("lokacioni", site.name.empty () ? std::string ("Kosov\xC3\xAB") : site.name);
	project.Set ("lat", Html::Num (site.latitude, 4));
	project.Set ("lon", Html::Num (site.longitude, 4));
	project.Set ("alt", Html::Num (site.altitude, 0));

	std::ostringstream b;
	b << "<h2>1. Lokacioni</h2>";
	for (const Json::Value& line : cat.Documents ()["insolationIntro"].AsArray ())
		b << "<p>" << Html::Fill (line.AsString (), project, office) << "</p>";

	b << "<h2>2. Diagrami i l\xC3\xABvizjes s\xC3\xAB diellit (projeksion polar)</h2>";
	b << "<table style=\"border:none\"><tr><td style=\"border:none;width:62%\">" << SunPathSvg (site, opt.year, opt.projectNorthDeg) << "</td><td style=\"border:none\">"
	  << "<div class=\"legend\"><span><i style=\"background:#d97706\"></i>21 qershor (solstici veror)</span><br><span><i style=\"background:#135027\"></i>21 mars / 23 shtator (ekuinokset)</span><br>"
	  << "<span><i style=\"background:#002142\"></i>21 dhjetor (solstici dim\xC3\xABror)</span><br><span><i style=\"background:#8a96a3\"></i>or\xC3\xABt lokale</span>"
	  << (std::fabs (opt.projectNorthDeg) > 0.01 ? "<br><span><i style=\"background:#b3261e\"></i>VP \xE2\x80\x93 veriu i projektit (" + Html::Num (opt.projectNorthDeg, 1) + "\xC2\xB0)</span>" : std::string ())
	  << "</div><p class=\"note\">Qendra e diagramit = zeniti; rrath\xC3\xABt = lart\xC3\xABsia e diellit mbi horizont (\xC3\xA7" "do 15\xC2\xB0); V\xE2\x80\x93L\xE2\x80\x93J\xE2\x80\x93P = drejtimet gjeografike.</p></td></tr></table>";

	b << "<h2>3. T\xC3\xAB dh\xC3\xABnat e diellit p\xC3\xABr datat karakteristike</h2>";
	b << "<table><tr><th>Data</th><th class=\"n\">Lindja</th><th class=\"n\">Mesdita diellore</th><th class=\"n\">Per\xC3\xABndimi</th><th class=\"n\">Koh\xC3\xABzgjatja e dit\xC3\xABs</th><th class=\"n\">Lart\xC3\xABsia n\xC3\xAB mesdit\xC3\xAB</th><th class=\"n\">Azimuti lindje / per\xC3\xABndim</th></tr>";
	const Date keyDates[] = { Date (opt.year, 3, 21), Date (opt.year, 6, 21), Date (opt.year, 9, 23), Date (opt.year, 12, 21) };
	for (const Date& d : keyDates) {
		const SunDay sd = Solar::Day (site, d);
		b << "<tr><td>" << d.ToAlbLong () << (sd.dst ? " <span class=\"note\">(CEST)</span>" : " <span class=\"note\">(CET)</span>") << "</td><td class=\"n\">" << Solar::FormatHour (sd.sunrise)
		  << "</td><td class=\"n\">" << Solar::FormatHour (sd.solarNoon) << "</td><td class=\"n\">" << Solar::FormatHour (sd.sunset)
		  << "</td><td class=\"n\">" << Solar::FormatHour (sd.dayLength) << " h</td><td class=\"n\">" << Html::Num (sd.noonAltitude, 1)
		  << "\xC2\xB0</td><td class=\"n\">" << Html::Num (sd.riseAzimuth, 0) << "\xC2\xB0 / " << Html::Num (sd.setAzimuth, 0) << "\xC2\xB0</td></tr>";
	}
	b << "</table>";

	b << "<h2>4. Or\xC3\xABt teorike t\xC3\xAB insolimit sipas orientimit t\xC3\xAB fasad\xC3\xABs</h2>";
	b << "<table><tr><th>Orientimi i fasad\xC3\xABs</th><th class=\"n\">Azimuti</th>";
	for (const Date& d : keyDates) b << "<th class=\"n\">" << d.d << "." << (d.m < 10 ? "0" : "") << d.m << "</th>";
	b << "<th class=\"c\">Kriteri \xE2\x89\xA5 " << Html::Num (opt.minSunHours, 1) << " h (21.03)</th></tr>";
	for (int k = 0; k < 8; ++k) {
		const double az = 45.0 * k;
		b << "<tr><td>" << Solar::OrientationName (az) << "</td><td class=\"n\">" << az << "\xC2\xB0</td>";
		double equinox = 0.0;
		for (size_t i = 0; i < 4; ++i) {
			const double hrs = Solar::FacadeSunHours (site, keyDates[i], az);
			if (i == 0) equinox = hrs;
			b << "<td class=\"n\">" << Html::Num (hrs, 1) << " h</td>";
		}
		b << "<td class=\"c\">" << (equinox >= opt.minSunHours ? "<span class=\"ok\">po</span>" : "<span class=\"ko\">jo</span>") << "</td></tr>";
	}
	b << "</table><p class=\"note\">Or\xC3\xAB me diell mbi 5\xC2\xB0 lart\xC3\xABsi dhe brenda \xC2\xB1" "90\xC2\xB0 nga normalja e fasad\xC3\xABs, pa pengesa. Kriteri minimal i insolimit p\xC3\xABr hap\xC3\xABsirat e banimit p\xC3\xABrcaktohet nga projektuesi sipas dokumentit t\xC3\xAB planifikimit dhe normave n\xC3\xAB fuqi.</p>";

	b << "<h2>5. Gjat\xC3\xABsia e hijes p\xC3\xABr objektin me lart\xC3\xABsi H = " << Html::Num (opt.buildingHeight, 1) << " m</h2>";
	b << "<table><tr><th>Ora</th>";
	const Date shadowDates[] = { Date (opt.year, 12, 21), Date (opt.year, 3, 21), Date (opt.year, 6, 21) };
	for (const Date& d : shadowDates) b << "<th class=\"n\">" << d.ToAlbLong () << "<br><span class=\"note\">lart\xC3\xABsia / azimuti / hija</span></th>";
	b << "</tr>";
	for (int hour = 7; hour <= 18; ++hour) {
		b << "<tr><td>" << (hour < 10 ? "0" : "") << hour << ":00</td>";
		for (const Date& d : shadowDates) {
			const SunPosition p = Solar::Position (site, d, hour);
			if (p.altitude <= 0.5) { b << "<td class=\"n\">\xE2\x80\x93</td>"; continue; }
			const double L = Solar::ShadowLength (opt.buildingHeight, p.altitude);
			b << "<td class=\"n\">" << Html::Num (p.altitude, 1) << "\xC2\xB0 / " << Html::Num (p.azimuth, 0) << "\xC2\xB0 / <b>" << (L > 200 ? std::string ("&gt;200") : Html::Num (L, 1)) << " m</b></td>";
		}
		b << "</tr>";
	}
	b << "</table>";
	b << "<p class=\"note\">Ora lokale; drejtimi i hijes = azimuti i diellit + 180\xC2\xB0. P\xC3\xABr analiz\xC3\xAB me objektet p\xC3\xABrreth p\xC3\xABrdorni Studimin e Diellit n\xC3\xAB Archicad (Dokumenti \xE2\x80\xBA Krijimi i Imazheve \xE2\x80\xBA Studimi i Diellit), lokacioni i projektit \xC3\xABsht\xC3\xAB vendosur nga ARBS Studio.</p>";

	std::vector<ReportOutput> outs;
	ReportOutput html;
	html.fileName = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PKR")) + "_F02_Insolimi.html";
	html.mime = "text/html";
	html.content = Document ("SKEMA E INSOLIMIT DHE P\xC3\x8BRCAKTIMI I DIELLIMIT", b.str (), project, office, false);
	outs.push_back (html);
	return outs;
}

// =============================================================================
// Skemat e hapjeve, inventarit dhe zonave
// =============================================================================

std::string Reports::OpeningElevationSvg (const std::string& kind, double w, double h, double sill)
{
	// pamje nga brenda, shkalla e pershtatur ne 120 px lartesi
	const double maxH = 2.6;
	const double scale = 110.0 / std::max (maxH, h + std::max (0.0, sill));
	const int W = 130, H = 140;
	const double pw = w * scale, ph = h * scale;
	const double x0 = (W - pw) / 2.0;
	const double floorY = H - 16.0;
	const double y0 = floorY - (std::max (0.0, sill) * scale) - ph;
	std::ostringstream s;
	s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "\" height=\"" << H << "\" viewBox=\"0 0 " << W << " " << H << "\">";
	s << "<line x1=\"4\" y1=\"" << floorY << "\" x2=\"" << W - 4 << "\" y2=\"" << floorY << "\" stroke=\"#8a96a3\" stroke-width=\"0.8\"/>";
	s << "<rect x=\"" << x0 << "\" y=\"" << y0 << "\" width=\"" << pw << "\" height=\"" << ph << "\" fill=\"#ffffff\" stroke=\"#1d2733\" stroke-width=\"1.2\"/>";
	const double fr = 3.0;
	s << "<rect x=\"" << x0 + fr << "\" y=\"" << y0 + fr << "\" width=\"" << std::max (1.0, pw - 2 * fr) << "\" height=\"" << std::max (1.0, ph - 2 * fr) << "\" fill=\"" << (kind == "door" ? "#efe7da" : "#dbe9f5") << "\" stroke=\"#1d2733\" stroke-width=\"0.6\"/>";
	if (kind == "door") {
		s << "<path d=\"M" << x0 + fr << " " << y0 + fr << " L" << x0 + pw - fr << " " << y0 + ph / 2 << " L" << x0 + fr << " " << y0 + ph - fr << "\" fill=\"none\" stroke=\"#5c6b79\" stroke-width=\"0.6\" stroke-dasharray=\"3 2\"/>";
		s << "<circle cx=\"" << x0 + pw - 9 << "\" cy=\"" << y0 + ph * 0.55 << "\" r=\"1.6\" fill=\"#1d2733\"/>";
	} else {
		if (w > 1.2) s << "<line x1=\"" << x0 + pw / 2 << "\" y1=\"" << y0 + fr << "\" x2=\"" << x0 + pw / 2 << "\" y2=\"" << y0 + ph - fr << "\" stroke=\"#1d2733\" stroke-width=\"0.8\"/>";
		s << "<path d=\"M" << x0 + fr << " " << y0 + fr << " L" << x0 + (w > 1.2 ? pw / 2 : pw - fr) << " " << y0 + ph / 2 << " L" << x0 + fr << " " << y0 + ph - fr << "\" fill=\"none\" stroke=\"#5c6b79\" stroke-width=\"0.6\" stroke-dasharray=\"3 2\"/>";
	}
	s << "<text x=\"" << W / 2 << "\" y=\"" << y0 - 3 << "\" font-size=\"8\" text-anchor=\"middle\" fill=\"#1d2733\">" << Html::Num (w * 100.0, 0) << "</text>";
	const double lx = std::min (x0 + pw + 3.0, W - 16.0);
	s << "<text x=\"" << lx << "\" y=\"" << y0 + ph / 2 << "\" font-size=\"8\" fill=\"#1d2733\">" << Html::Num (h * 100.0, 0) << "</text>";
	if (sill > 0.01) s << "<text x=\"" << lx << "\" y=\"" << floorY - 2 << "\" font-size=\"7\" fill=\"#5c6b79\">p=" << Html::Num (sill * 100.0, 0) << "</text>";
	s << "</svg>";
	return s.str ();
}

static std::string CsvCell (const std::string& v)
{
	bool quote = v.find_first_of (";\"\n") != std::string::npos;
	std::string out = v;
	size_t p = 0;
	while ((p = out.find ('"', p)) != std::string::npos) { out.insert (p, "\""); p += 2; }
	return quote ? "\"" + out + "\"" : out;
}

static std::string CsvDoc (const std::vector<std::vector<std::string>>& rows)
{
	std::string out = "\xEF\xBB\xBF";	// BOM per Excel
	for (const auto& r : rows) {
		for (size_t i = 0; i < r.size (); ++i) {
			if (i > 0) out += ';';
			out += CsvCell (r[i]);
		}
		out += "\r\n";
	}
	return out;
}

std::vector<ReportOutput> Reports::OpeningSchedule (const Json::Value& project, const Json::Value& office, const Json::Value& model, const std::string& kind)
{
	struct Group { std::string type; double w, h, sill; std::vector<std::string> ids; std::set<int> stories; int count = 0; };
	std::vector<Group> groups;
	for (const Json::Value& o : model["openings"].AsArray ()) {
		if (o.GetString ("kind") != kind) continue;
		const double w = std::round (o.GetNumber ("width") * 100.0) / 100.0;
		const double h = std::round (o.GetNumber ("height") * 100.0) / 100.0;
		const double sill = std::round (o.GetNumber ("sill") * 100.0) / 100.0;
		const std::string type = o.GetString ("type");
		Group* g = nullptr;
		for (Group& x : groups)
			if (x.type == type && std::fabs (x.w - w) < 0.005 && std::fabs (x.h - h) < 0.005 && std::fabs (x.sill - sill) < 0.005) { g = &x; break; }
		if (g == nullptr) { groups.push_back ({ type, w, h, sill, {}, {}, 0 }); g = &groups.back (); }
		g->count++;
		if (!o.GetString ("id").empty ()) g->ids.push_back (o.GetString ("id"));
		g->stories.insert (o.GetInt ("story"));
	}
	std::sort (groups.begin (), groups.end (), [] (const Group& a, const Group& b) {
		if (a.h != b.h) return a.h > b.h;
		return a.w > b.w;
	});

	const bool door = kind == "door";
	const std::string prefix = door ? "D" : "W";
	const std::string title = door ? "SKEMA E DYERVE" : "SKEMA E DRITAREVE";

	std::ostringstream b;
	std::vector<std::vector<std::string>> csv;
	csv.push_back ({ "Pozicioni", "Tipi (objekti)", "Gjer\xC3\xABsia (cm)", "Lart\xC3\xABsia (cm)", door ? "Pragu (cm)" : "Parapeti (cm)", "Sasia (cop\xC3\xAB)", "Katet", "ID n\xC3\xAB model" });
	int total = 0;
	if (groups.empty ()) {
		b << "<p class=\"note\">Nuk u gjet\xC3\xABn " << (door ? "dyer" : "dritare") << " n\xC3\xAB model.</p>";
	} else {
		b << "<table><tr><th class=\"c\" style=\"width:7%\">Poz.</th><th class=\"c\" style=\"width:20%\">Pamja nga brenda</th><th>Tipi dhe p\xC3\xABrshkrimi</th><th class=\"n\">B \xC3\x97 H (cm)</th><th class=\"n\">" << (door ? "Pragu" : "Parapeti") << " (cm)</th><th class=\"n\">Sasia</th><th>Katet</th></tr>";
		int n = 0;
		for (const Group& g : groups) {
			++n;
			total += g.count;
			char pos[16];
			std::snprintf (pos, sizeof (pos), "%s%02d", prefix.c_str (), n);
			std::string stories;
			for (int st : g.stories) stories += (stories.empty () ? "" : ", ") + StoryName (model, st);
			std::string ids;
			for (size_t i = 0; i < g.ids.size () && i < 12; ++i) ids += (i ? ", " : "") + g.ids[i];
			if (g.ids.size () > 12) ids += " \xE2\x80\xA6";
			b << "<tr><td class=\"c\"><b>" << pos << "</b></td><td class=\"c\">" << OpeningElevationSvg (kind, g.w, g.h, g.sill) << "</td><td>" << Html::Esc (g.type)
			  << (ids.empty () ? "" : "<br><span class=\"note\">ID: " + Html::Esc (ids) + "</span>")
			  << "<br><span class=\"note\">" << (door ? "Krahu, materiali, doreza dhe pragu sipas detajit." : "Profil me nd\xC3\xABrprerje termike, xham termoizolues U<sub>w</sub> \xE2\x89\xA4 1,60 W/m\xC2\xB2K.") << "</span></td>"
			  << "<td class=\"n\">" << Html::Num (g.w * 100.0, 0) << " \xC3\x97 " << Html::Num (g.h * 100.0, 0) << "</td><td class=\"n\">" << Html::Num (g.sill * 100.0, 0)
			  << "</td><td class=\"n\"><b>" << g.count << "</b></td><td>" << Html::Esc (stories) << "</td></tr>";
			csv.push_back ({ pos, g.type, Html::Num (g.w * 100.0, 0), Html::Num (g.h * 100.0, 0), Html::Num (g.sill * 100.0, 0), std::to_string (g.count), stories, ids });
		}
		b << "<tr class=\"sum\"><td></td><td></td><td>GJITHSEJ</td><td></td><td></td><td class=\"n\">" << total << "</td><td></td></tr></table>";
		b << "<p class=\"note\">Dimensionet jan\xC3\xAB dimensione t\xC3\xAB hapjes (drit\xC3\xABs s\xC3\xAB murit) sipas modelit. Para prodhimit, dimensionet maten n\xC3\xAB vend.</p>";
	}

	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PZ")) + (door ? "_A20_Skema-e-dyerve" : "_A21_Skema-e-dritareve");
	outs.push_back ({ base + ".html", Document (title, b.str (), project, office, false), "text/html" });
	outs.push_back ({ base + ".csv", CsvDoc (csv), "text/csv" });
	return outs;
}

std::vector<ReportOutput> Reports::InventorySchedule (const Json::Value& project, const Json::Value& office, const Json::Value& model)
{
	std::map<std::string, std::map<int, int>> counts;	// tipi -> kati -> sasia
	std::set<int> stories;
	for (const Json::Value& o : model["objects"].AsArray ()) {
		const std::string t = o.GetString ("type", "Objekt");
		counts[t][o.GetInt ("story")]++;
		stories.insert (o.GetInt ("story"));
	}
	std::ostringstream b;
	std::vector<std::vector<std::string>> csv;
	std::vector<std::string> head = { "Nr.", "Elementi i inventarit" };
	for (int s : stories) head.push_back (StoryName (model, s));
	head.push_back ("Gjithsej");
	csv.push_back (head);
	if (counts.empty ()) {
		b << "<p class=\"note\">Nuk u gjet\xC3\xABn objekte inventari (mobilie, sanitari, pajisje) n\xC3\xAB model.</p>";
	} else {
		b << "<table><tr><th class=\"c\" style=\"width:6%\">Nr.</th><th>Elementi i inventarit</th>";
		for (int s : stories) b << "<th class=\"n\">" << Html::Esc (StoryName (model, s)) << "</th>";
		b << "<th class=\"n\">Gjithsej</th></tr>";
		int n = 0, grand = 0;
		for (const auto& kv : counts) {
			++n;
			int tot = 0;
			std::vector<std::string> row = { std::to_string (n), kv.first };
			b << "<tr><td class=\"c\">" << n << "</td><td>" << Html::Esc (kv.first) << "</td>";
			for (int s : stories) {
				auto it = kv.second.find (s);
				const int c = it != kv.second.end () ? it->second : 0;
				tot += c;
				b << "<td class=\"n\">" << (c ? std::to_string (c) : "\xE2\x80\x93") << "</td>";
				row.push_back (std::to_string (c));
			}
			grand += tot;
			b << "<td class=\"n\"><b>" << tot << "</b></td></tr>";
			row.push_back (std::to_string (tot));
			csv.push_back (row);
		}
		b << "<tr class=\"sum\"><td></td><td>GJITHSEJ</td>";
		for (size_t i = 0; i < stories.size (); ++i) b << "<td></td>";
		b << "<td class=\"n\">" << grand << "</td></tr></table>";
	}
	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PZ")) + "_A22_Skema-e-inventarit";
	outs.push_back ({ base + ".html", Document ("SKEMA E INVENTARIT", b.str (), project, office, false), "text/html" });
	outs.push_back ({ base + ".csv", CsvDoc (csv), "text/csv" });
	return outs;
}

std::vector<ReportOutput> Reports::ZoneSchedule (const Json::Value& project, const Json::Value& office, const Json::Value& model)
{
	std::vector<std::vector<std::string>> csv;
	csv.push_back ({ "Kati", "Nr.", "Hap\xC3\xABsira", "Sip\xC3\xABrfaqja (m2)" });
	for (const Json::Value& z : model["zones"].AsArray ())
		csv.push_back ({ StoryName (model, z.GetInt ("story")), z.GetString ("number"), z.GetString ("name"), Html::Num (z.GetNumber ("area")) });
	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PI")) + "_A23_Tabela-e-siperfaqeve";
	outs.push_back ({ base + ".html", Document ("TABELA E SIP\xC3\x8BRFAQEVE", ZonesTable (model), project, office, false), "text/html" });
	outs.push_back ({ base + ".csv", CsvDoc (csv), "text/csv" });
	return outs;
}

// =============================================================================
// Plani dinamik (Gantt)
// =============================================================================

std::vector<ReportOutput> Reports::DynamicPlanReport (const Json::Value& project, const Json::Value& office, const std::vector<PlanTask>& tasks,
													  const KosovoCalendar& cal, const PlanParameters& params,
													  const std::string& fileSuffix, const std::string& titleSuffix)
{
	const std::string planTitle = "PLANI DINAMIK I PUNIMEVE" + (titleSuffix.empty () ? std::string () : " \xE2\x80\x93 " + titleSuffix);
	std::ostringstream b;
	if (tasks.empty ()) {
		b << "<p>Plani nuk ka detyra.</p>";
		return { { "Plani-dinamik" + fileSuffix + ".html", Document (planTitle, b.str (), project, office, false, true), "text/html" } };
	}
	Date first = tasks.front ().es, last = tasks.front ().ef;
	for (const PlanTask& t : tasks) { if (t.es < first) first = t.es; if (last < t.ef) last = t.ef; }
	// fillimi i javes (e hene) dhe fundi i muajit
	Date from = first.AddDays (-first.Weekday ());
	Date to = Date (last.y, last.m, DaysInMonth (last.y, last.m));
	const long days = to.ToDays () - from.ToDays () + 1;

	const int workDays = cal.CountWorkingDays (first, last);
	b << "<table><tr><th>Fillimi i punimeve</th><th>P\xC3\xABrfundimi</th><th class=\"n\">Dit\xC3\xAB pune</th><th class=\"n\">Dit\xC3\xAB kalendarike</th><th class=\"n\">Detyra</th><th>Java e pun\xC3\xABs</th></tr>"
	  << "<tr><td>" << first.ToAlb () << " (" << WeekdayNameAlb (first.Weekday ()) << ")</td><td>" << last.ToAlb () << " (" << WeekdayNameAlb (last.Weekday ()) << ")</td><td class=\"n\">" << workDays
	  << "</td><td class=\"n\">" << (last.ToDays () - first.ToDays () + 1) << "</td><td class=\"n\">" << tasks.size () << "</td><td>"
	  << (params.saturdayWorking ? "E h\xC3\xABn\xC3\xAB \xE2\x80\x93 e shtun\xC3\xAB" : "E h\xC3\xABn\xC3\xAB \xE2\x80\x93 e premte") << "</td></tr></table>";

	// rekapitulimi sipas blloqeve (detyrat "B1:...")
	{
		std::vector<std::string> order;
		std::map<std::string, std::pair<Date, Date>> span;
		std::map<std::string, int> count;
		for (const PlanTask& t : tasks) {
			const std::string bl = DynamicPlan::BlockOf (t);
			const std::string key = bl.empty () ? std::string ("Punimet e p\xC3\xABrbashk\xC3\xABta") : "Blloku " + bl;
			if (!span.count (key)) { order.push_back (key); span[key] = { t.es, t.ef }; }
			if (t.es < span[key].first) span[key].first = t.es;
			if (span[key].second < t.ef) span[key].second = t.ef;
			count[key]++;
		}
		if (order.size () > 1) {
			b << "<h2>Rekapitulimi sipas blloqeve</h2><table><tr><th>Pjesa</th><th>Fillimi</th><th>P\xC3\xABrfundimi</th><th class=\"n\">Dit\xC3\xAB pune</th><th class=\"n\">Dit\xC3\xAB kalendarike</th><th class=\"n\">Detyra</th></tr>";
			for (const std::string& k : order) {
				const Date& a = span[k].first;
				const Date& z = span[k].second;
				b << "<tr><td>" << Html::Esc (k) << "</td><td>" << a.ToAlb () << "</td><td>" << z.ToAlb () << "</td><td class=\"n\">" << cal.CountWorkingDays (a, z)
				  << "</td><td class=\"n\">" << (z.ToDays () - a.ToDays () + 1) << "</td><td class=\"n\">" << count[k] << "</td></tr>";
			}
			b << "<tr class=\"sum\"><td>GJITHSEJ PROJEKTI</td><td>" << first.ToAlb () << "</td><td>" << last.ToAlb () << "</td><td class=\"n\">" << workDays
			  << "</td><td class=\"n\">" << (last.ToDays () - first.ToDays () + 1) << "</td><td class=\"n\">" << tasks.size () << "</td></tr></table>";
		}
	}

	// --- Gantt SVG
	const double pxDay = days > 700 ? 1.2 : days > 400 ? 2.0 : days > 200 ? 3.0 : 5.0;
	const double left = 430.0;
	const double rowH = 17.0;
	const double headH = 40.0;
	const double width = left + static_cast<double> (days) * pxDay + 10.0;
	std::vector<std::string> groupOrder;
	for (const PlanTask& t : tasks)
		if (std::find (groupOrder.begin (), groupOrder.end (), t.group) == groupOrder.end ()) groupOrder.push_back (t.group);
	const double height = headH + static_cast<double> (tasks.size () + groupOrder.size ()) * rowH + 10.0;
	auto X = [&] (const Date& d) { return left + static_cast<double> (d.ToDays () - from.ToDays ()) * pxDay; };

	std::ostringstream s;
	s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">";
	s << "<rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height << "\" fill=\"#ffffff\"/>";
	// ditet jo-pune
	for (long d = 0; d < days; ++d) {
		const Date day = Date::FromDays (from.ToDays () + d);
		std::string hname;
		const bool hol = cal.IsHoliday (day, &hname);
		const bool weekend = day.Weekday () == 6 || (day.Weekday () == 5 && !params.saturdayWorking);
		if (hol)
			s << "<rect x=\"" << X (day) << "\" y=\"" << headH << "\" width=\"" << pxDay << "\" height=\"" << height - headH << "\" fill=\"#f6c9c4\"><title>" << Html::Esc (day.ToAlb () + " \xE2\x80\x93 " + hname) << "</title></rect>";
		else if (weekend && pxDay >= 2.0)
			s << "<rect x=\"" << X (day) << "\" y=\"" << headH << "\" width=\"" << pxDay << "\" height=\"" << height - headH << "\" fill=\"#f1f3f5\"/>";
	}
	for (const DateRange& r : cal.GetNonWorkingRanges ()) {
		if (r.to < from || to < r.from) continue;
		const Date a = r.from < from ? from : r.from;
		const Date z = to < r.to ? to : r.to;
		s << "<rect x=\"" << X (a) << "\" y=\"" << headH << "\" width=\"" << (z.ToDays () - a.ToDays () + 1) * pxDay << "\" height=\"" << height - headH << "\" fill=\""
		  << (r.weatherOnly ? "#dbe9f5" : "#ece3f7") << "\" fill-opacity=\"0.55\"><title>" << Html::Esc (r.name) << "</title></rect>";
	}
	// koka: muajt dhe javet
	s << "<rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << headH << "\" fill=\"#002142\"/>";
	s << "<text x=\"8\" y=\"25\" font-size=\"10\" fill=\"#ffffff\" font-weight=\"700\">WBS</text><text x=\"44\" y=\"25\" font-size=\"10\" fill=\"#ffffff\" font-weight=\"700\">Pozicioni i punimeve</text>"
	  << "<text x=\"300\" y=\"25\" font-size=\"10\" fill=\"#ffffff\" font-weight=\"700\">Dit\xC3\xAB</text><text x=\"332\" y=\"25\" font-size=\"10\" fill=\"#ffffff\" font-weight=\"700\">Fillimi</text>"
	  << "<text x=\"380\" y=\"25\" font-size=\"10\" fill=\"#ffffff\" font-weight=\"700\">Mbarimi</text>";
	for (Date m (from.y, from.m, 1); m <= to; m = (m.m == 12 ? Date (m.y + 1, 1, 1) : Date (m.y, m.m + 1, 1))) {
		const Date a = m < from ? from : m;
		const double x = X (a);
		const double w = (DaysInMonth (m.y, m.m) - (a.d - 1)) * pxDay;
		s << "<line x1=\"" << x << "\" y1=\"0\" x2=\"" << x << "\" y2=\"" << height << "\" stroke=\"#9aa6b2\" stroke-width=\"0.6\"/>";
		if (w > 26) s << "<text x=\"" << x + 3 << "\" y=\"15\" font-size=\"9\" fill=\"#ffffff\">" << MonthShortAlb (m.m) << " " << m.y << "</text>";
	}
	if (pxDay >= 2.0) {
		for (long d = 0; d < days; d += 7) {
			const Date day = Date::FromDays (from.ToDays () + d);
			s << "<text x=\"" << X (day) + 1 << "\" y=\"33\" font-size=\"7\" fill=\"#c9d3dd\">" << day.d << "</text>";
			s << "<line x1=\"" << X (day) << "\" y1=\"" << headH << "\" x2=\"" << X (day) << "\" y2=\"" << height << "\" stroke=\"#e2e7ec\" stroke-width=\"0.4\"/>";
		}
	}
	// detyrat
	double y = headH;
	std::map<std::string, std::pair<double, double>> barPos;
	for (const std::string& grp : groupOrder) {
		s << "<rect x=\"0\" y=\"" << y << "\" width=\"" << left << "\" height=\"" << rowH << "\" fill=\"#e8efe9\"/>";
		s << "<text x=\"8\" y=\"" << y + 12 << "\" font-size=\"9.5\" font-weight=\"700\" fill=\"#135027\">" << Html::Esc (grp) << "</text>";
		// shiriti permbledhes i grupit
		Date gs, ge; bool firstT = true;
		for (const PlanTask& t : tasks) if (t.group == grp) { if (firstT || t.es < gs) gs = t.es; if (firstT || ge < t.ef) ge = t.ef; firstT = false; }
		if (!firstT) s << "<rect x=\"" << X (gs) << "\" y=\"" << y + 6 << "\" width=\"" << (ge.ToDays () - gs.ToDays () + 1) * pxDay << "\" height=\"4\" fill=\"#135027\"/>";
		y += rowH;
		for (const PlanTask& t : tasks) {
			if (t.group != grp) continue;
			const std::string name = Utf8Truncate (t.name, 60);
			s << "<line x1=\"0\" y1=\"" << y + rowH << "\" x2=\"" << width << "\" y2=\"" << y + rowH << "\" stroke=\"#eef1f4\" stroke-width=\"0.5\"/>";
			s << "<text x=\"8\" y=\"" << y + 12 << "\" font-size=\"8.5\" fill=\"#5c6b79\">" << Html::Esc (t.wbs) << "</text>";
			s << "<text x=\"44\" y=\"" << y + 12 << "\" font-size=\"8.5\" fill=\"#1d2733\">" << Html::Esc (name) << "</text>";
			s << "<text x=\"318\" y=\"" << y + 12 << "\" font-size=\"8.5\" text-anchor=\"end\" fill=\"#1d2733\">" << t.duration << "</text>";
			char sd[16], ed[16];
			std::snprintf (sd, sizeof (sd), "%02d.%02d.%02d", t.es.d, t.es.m, t.es.y % 100);
			std::snprintf (ed, sizeof (ed), "%02d.%02d.%02d", t.ef.d, t.ef.m, t.ef.y % 100);
			s << "<text x=\"332\" y=\"" << y + 12 << "\" font-size=\"8\" fill=\"#1d2733\">" << sd << "</text><text x=\"380\" y=\"" << y + 12 << "\" font-size=\"8\" fill=\"#1d2733\">" << ed << "</text>";
			if (t.duration == 0) {
				const double cx = X (t.es) + pxDay / 2.0, cy = y + rowH / 2.0;
				s << "<path d=\"M" << cx << " " << cy - 6 << " L" << cx + 6 << " " << cy << " L" << cx << " " << cy + 6 << " L" << cx - 6 << " " << cy << " Z\" fill=\"#002142\"><title>" << Html::Esc (t.name + " \xE2\x80\x93 " + t.es.ToAlb ()) << "</title></path>";
				barPos[t.id] = { cx, cy };
			} else {
				const double bx = X (t.es), bw = (t.ef.ToDays () - t.es.ToDays () + 1) * pxDay;
				s << "<rect x=\"" << bx << "\" y=\"" << y + 4 << "\" width=\"" << std::max (1.5, bw) << "\" height=\"" << rowH - 8 << "\" rx=\"2\" fill=\""
				  << (t.critical ? "#b3261e" : (t.weatherSensitive ? "#2d6aa8" : "#135027")) << "\"><title>" << Html::Esc (t.name + ": " + t.es.ToAlb () + " \xE2\x80\x93 " + t.ef.ToAlb () + " (" + std::to_string (t.duration) + " dit\xC3\xAB pune)") << "</title></rect>";
				barPos[t.id] = { bx + bw, y + rowH / 2.0 };
			}
			y += rowH;
		}
	}
	s << "<line x1=\"" << left << "\" y1=\"0\" x2=\"" << left << "\" y2=\"" << height << "\" stroke=\"#1d2733\" stroke-width=\"0.8\"/>";
	s << "</svg>";

	b << "<div class=\"legend\"><span><i style=\"background:#135027\"></i>pun\xC3\xAB</span><span><i style=\"background:#2d6aa8\"></i>pun\xC3\xAB e ndjeshme ndaj motit</span>"
	  << "<span><i style=\"background:#b3261e\"></i>rruga kritike</span><span><i style=\"background:#f6c9c4\"></i>fest\xC3\xAB zyrtare</span>"
	  << "<span><i style=\"background:#f1f3f5;border:1px solid #ccc\"></i>fundjav\xC3\xAB</span><span><i style=\"background:#dbe9f5\"></i>periudha dim\xC3\xABrore (vet\xC3\xABm p\xC3\xABr pun\xC3\xABt e ndjeshme)</span></div>";
	b << "<div style=\"overflow-x:auto\">" << s.str () << "</div>";

	// festat ne periudhe
	b << "<h2>Festat zyrtare dhe dit\xC3\xABt jo-pune n\xC3\xAB periudh\xC3\xABn e punimeve</h2>";
	b << "<p class=\"note\">" << Html::Esc (cal.LawReference ()) << ". Kur festa bie n\xC3\xAB fundjav\xC3\xAB, dita e lir\xC3\xAB zhvendoset n\xC3\xAB dit\xC3\xABn e par\xC3\xAB t\xC3\xAB pun\xC3\xABs.</p>";
	b << "<table><tr><th>Festa</th><th>Data</th><th>Dita jo-pune</th><th>Sh\xC3\xABnim</th></tr>";
	for (const Holiday& h : cal.HolidaysBetween (first, last)) {
		b << "<tr><td>" << Html::Esc (h.name) << "</td><td>" << h.date.ToAlb () << " (" << WeekdayNameAlb (h.date.Weekday ()) << ")</td><td>"
		  << h.observed.ToAlb () << " (" << WeekdayNameAlb (h.observed.Weekday ()) << ")</td><td>"
		  << (h.shifted ? "zhvendosur nga fundjava" : "") << (h.estimated ? (h.shifted ? "; " : "") + std::string ("<span class=\"mungon\">data e parashikuar \xE2\x80\x93 konfirmoni me vendimin e Qeveris\xC3\xAB</span>") : "") << "</td></tr>";
	}
	b << "</table>";

	// tabela e detyrave
	b << "<h2>Lista e pozicioneve t\xC3\xAB punimeve</h2>";
	b << "<table><tr><th>WBS</th><th>Pozicioni</th><th class=\"n\">Dit\xC3\xAB pune</th><th>Fillimi</th><th>Mbarimi</th><th class=\"n\">Rezerva</th><th>Var\xC3\xABsit\xC3\xAB</th><th>Sh\xC3\xABnime</th></tr>";
	std::vector<std::vector<std::string>> csv;
	csv.push_back ({ "WBS", "Grupi", "Pozicioni", "Dit\xC3\xAB pune", "Fillimi", "Mbarimi", "Rezerva (dit\xC3\xAB)", "Kritike", "Var\xC3\xABsit\xC3\xAB", "Ekipi", "Sh\xC3\xABnime" });
	std::map<std::string, std::string> wbsById;
	for (const PlanTask& t : tasks) wbsById[t.id] = t.wbs;
	for (const PlanTask& t : tasks) {
		std::string deps;
		for (const PlanLink& l : t.links) {
			if (!deps.empty ()) deps += ", ";
			deps += wbsById[l.predecessor];
			if (l.type != "FS") deps += l.type;
			if (l.lag != 0) deps += (l.lag > 0 ? "+" : "") + std::to_string (l.lag);
		}
		b << "<tr><td>" << Html::Esc (t.wbs) << "</td><td>" << Html::Esc (t.name) << "</td><td class=\"n\">" << t.duration << "</td><td>" << t.es.ToAlb () << "</td><td>" << t.ef.ToAlb ()
		  << "</td><td class=\"n\">" << (t.critical ? "<span class=\"ko\">0</span>" : std::to_string (t.totalFloat)) << "</td><td>" << Html::Esc (deps) << "</td><td class=\"note\">" << Html::Esc (t.notes) << "</td></tr>";
		csv.push_back ({ t.wbs, t.group, t.name, std::to_string (t.duration), t.es.ToAlb (), t.ef.ToAlb (), std::to_string (t.totalFloat), t.critical ? "po" : "jo", deps, t.crew, t.notes });
	}
	b << "</table>";

	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PZ")) + "_T05_Plani-dinamik" + fileSuffix;
	outs.push_back ({ base + ".html", Document (planTitle, b.str (), project, office, false, true), "text/html" });
	outs.push_back ({ base + ".csv", CsvDoc (csv), "text/csv" });
	return outs;
}

}	// namespace ARBS
