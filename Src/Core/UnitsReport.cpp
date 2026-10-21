// =============================================================================
// ARBS Studio - Core : UnitsReport.cpp
// Specifikimi i njesive (banesa, lokale, zyra, garazhe, depo) sipas bllokut, hyrjes dhe katit,
// dhe kapitulli i blloqeve per pershkrimin teknik (me rekapitulim).
// =============================================================================
#include "Reports.hpp"
#include "Structure.hpp"

#include <algorithm>
#include <functional>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace ARBS {

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

std::string StoryLabel (const Json::Value& model, int story)
{
	for (const Json::Value& s : model["stories"].AsArray ())
		if (s.GetInt ("index") == story) return s.GetString ("name");
	if (story == 0) return "P\xC3\xABrdhesa";
	if (story < 0) return "Bodrumi " + std::to_string (-story);
	return "Kati " + std::to_string (story);
}

std::string KindText (const Json::Value& u)
{
	std::string t = u.GetString ("kindLabel");
	if (!u.GetString ("structure").empty ()) t += " " + u.GetString ("structure");
	if (!u.GetString ("type").empty ()) t += " (tipi " + u.GetString ("type") + ")";
	return t;
}

struct Totals {
	int count = 0;
	double inner = 0, outerRaw = 0, outerRed = 0, net = 0, common = 0, withCommon = 0, gross = 0;
	std::map<std::string, int> byKind;
	void Add (const Json::Value& u)
	{
		++count;
		inner += u.GetNumber ("inner"); outerRaw += u.GetNumber ("outerRaw"); outerRed += u.GetNumber ("outerReduced");
		net += u.GetNumber ("net"); common += u.GetNumber ("commonShare"); withCommon += u.GetNumber ("withCommon"); gross += u.GetNumber ("gross");
		byKind[KindText (u).substr (0, KindText (u).find (" (tipi"))]++;
	}
};

std::string TotalsRow (const char* cls, const std::string& label, const Totals& t)
{
	std::ostringstream b;
	b << "<tr class=\"" << cls << "\"><td colspan=\"3\">" << label << " (" << t.count << " nj\xC3\xABsi)</td><td class=\"n\">" << Html::Num (t.inner)
	  << "</td><td class=\"n\">" << Html::Num (t.outerRaw) << " / " << Html::Num (t.outerRed) << "</td><td class=\"n\">" << Html::Num (t.net)
	  << "</td><td class=\"n\">" << Html::Num (t.common) << "</td><td class=\"n\">" << Html::Num (t.withCommon) << "</td><td class=\"n\">" << (t.gross > 0 ? Html::Num (t.gross) : std::string ("\xE2\x80\x93")) << "</td></tr>";
	return b.str ();
}

}	// namespace

std::vector<ReportOutput> Reports::UnitSchedule (const Json::Value& project, const Json::Value& office, const Json::Value& model)
{
	const StructureOptions so = Structure::OptionsFromProject (project);
	const Json::Value ures = Structure::Units (model, so, project["njesite_manual"]);
	const Json::Value& units = ures["units"];

	std::ostringstream b;
	std::vector<std::vector<std::string>> csv;
	csv.push_back ({ "Blloku", "Hyrja", "Kati", "Nj\xC3\xABsia", "Lloji", "Struktura", "Hap\xC3\xABsira", "Sip. (m2)", "Koef.", "Sip. e llogaritur (m2)" });

	b << "<p>Specifikimi i nj\xC3\xABsive \xC3\xABsht\xC3\xAB hartuar " << (ures.GetString ("source") == "manual"
		? "nga tipet e nj\xC3\xABsive dhe vendosja e tyre n\xC3\xAB kate t\xC3\xAB p\xC3\xABrcaktuara n\xC3\xAB ARBS Studio"
		: "nga zonat e modelit n\xC3\xAB Archicad (zonat e nj\xC3\xABsive dhe hap\xC3\xABsirat brenda tyre)")
	  << ", sipas bllokut, hyrjes dhe katit. Numri i nj\xC3\xABsis\xC3\xAB p\xC3\xABrdoret si num\xC3\xABr i pjes\xC3\xABs s\xC3\xAB nd\xC3\xABrtes\xC3\xABs (UA MMPHI Nr. 07/2024).</p>";
	b << "<p class=\"note\">Koeficient\xC3\xAB p\xC3\xABr sip\xC3\xABrfaqet e jashtme: ballkon " << Html::Num (so.kBalcony) << ", lozh\xC3\xAB " << Html::Num (so.kLoggia) << ", teras\xC3\xAB " << Html::Num (so.kTerrace)
	  << (so.coefficientsSet ? "" : " <span class=\"mungon\">(nuk jan\xC3\xAB vendosur \xE2\x80\x93 p\xC3\xABrdoret 1,00; vendosini te Projekti \xE2\x80\xBA Struktura)</span>")
	  << ". Hap\xC3\xABsirat e p\xC3\xABrbashk\xC3\xABta t\xC3\xAB bllokut nd\xC3\xABrhen n\xC3\xAB proporcion me sip\xC3\xABrfaqen neto t\xC3\xAB nj\xC3\xABsive.</p>";

	if (units.Size () == 0) {
		b << "<p class=\"note\"><span class=\"mungon\">Nuk u gjet\xC3\xABn nj\xC3\xABsi.</span> Vizatoni p\xC3\xABr \xC3\xA7" "do nj\xC3\xABsi nj\xC3\xAB zon\xC3\xAB me emrin \xC2\xAB" "Banesa\xC2\xBB, \xC2\xABLokal\xC2\xBB ose \xC2\xABZyr\xC3\xAB\xC2\xBB (ose me kategori \xC2\xAB" "ARBS Nj\xC3\xABsia\xC2\xBB), "
			 "ose p\xC3\xABrcaktoni tipet e nj\xC3\xABsive me dor\xC3\xAB te paleta \xE2\x80\xBA Projekti \xE2\x80\xBA Struktura.</p>";
	}

	// --- tabela permbledhese sipas bllokut dhe hyrjes
	Totals all;
	std::map<std::string, Totals> byBlock;
	std::string curBlock = "\x01", curEntr = "\x01";
	Totals entrTot;
	auto head = [&] () {
		b << "<table><tr><th style=\"width:10%\">Nj\xC3\xABsia</th><th style=\"width:10%\">Kati</th><th>Lloji</th><th class=\"n\">Neto e brendshme</th><th class=\"n\">Ballkon/lozh\xC3\xAB (fakt./llog.)</th>"
			 "<th class=\"n\">Neto e llogaritur</th><th class=\"n\">P\xC3\xABrbashk\xC3\xABt</th><th class=\"n\">Gjithsej</th><th class=\"n\">Bruto</th></tr>";
	};
	auto closeEntrance = [&] () {
		if (curEntr != "\x01") { b << TotalsRow ("sub", "Gjithsej hyrja " + Html::Esc (curEntr), entrTot) << "</table>"; }
		entrTot = Totals ();
	};
	for (const Json::Value& u : units.AsArray ()) {
		const std::string bl = u.GetString ("block");
		const std::string en = u.GetString ("entrance");
		if (bl != curBlock) {
			closeEntrance ();
			curEntr = "\x01";
			curBlock = bl;
			b << "<h2>" << Html::Esc (Structure::BlockName (model, bl)) << (bl.empty () ? "" : " (" + Html::Esc (bl) + ")") << "</h2>";
		}
		if (en != curEntr) {
			closeEntrance ();
			curEntr = en;
			b << "<h3>Hyrja " << Html::Esc (en) << "</h3>";
			head ();
		}
		b << "<tr><td><b>" << Html::Esc (u.GetString ("number")) << "</b></td><td>" << Html::Esc (StoryLabel (model, u.GetInt ("story"))) << "</td><td>" << Html::Esc (KindText (u))
		  << "</td><td class=\"n\">" << Html::Num (u.GetNumber ("inner")) << "</td><td class=\"n\">" << Html::Num (u.GetNumber ("outerRaw")) << " / " << Html::Num (u.GetNumber ("outerReduced"))
		  << "</td><td class=\"n\"><b>" << Html::Num (u.GetNumber ("net")) << "</b></td><td class=\"n\">" << Html::Num (u.GetNumber ("commonShare")) << "</td><td class=\"n\">" << Html::Num (u.GetNumber ("withCommon"))
		  << "</td><td class=\"n\">" << (u.GetNumber ("gross") > 0 ? Html::Num (u.GetNumber ("gross")) : std::string ("\xE2\x80\x93")) << "</td></tr>";
		entrTot.Add (u);
		byBlock[bl].Add (u);
		all.Add (u);
		for (const Json::Value& r : u["rooms"].AsArray ())
			csv.push_back ({ bl, en, StoryLabel (model, u.GetInt ("story")), u.GetString ("number"), u.GetString ("kindLabel"), u.GetString ("structure"),
							 r.GetString ("name"), Html::Num (r.GetNumber ("area")), Html::Num (r.GetNumber ("k")), Html::Num (r.GetNumber ("reduced")) });
		csv.push_back ({ bl, en, StoryLabel (model, u.GetInt ("story")), u.GetString ("number"), u.GetString ("kindLabel"), u.GetString ("structure"),
						 "GJITHSEJ NETO (me p\xC3\xABrbashk\xC3\xABt: " + Html::Num (u.GetNumber ("withCommon")) + ")", Html::Num (u.GetNumber ("inner") + u.GetNumber ("outerRaw")), "", Html::Num (u.GetNumber ("net")) });
	}
	closeEntrance ();

	// --- rekapitulimi
	if (units.Size () > 0) {
		b << "<h2>Rekapitulimi i nj\xC3\xABsive</h2><table><tr><th>Pjesa</th><th class=\"n\">Nj\xC3\xABsi</th><th>Sipas llojit</th><th class=\"n\">Neto e llogaritur (m\xC2\xB2)</th><th class=\"n\">Me t\xC3\xAB p\xC3\xABrbashk\xC3\xABtat (m\xC2\xB2)</th><th class=\"n\">Bruto (m\xC2\xB2)</th></tr>";
		auto kinds = [] (const Totals& t) {
			std::string s;
			for (const auto& kv : t.byKind) s += (s.empty () ? "" : ", ") + std::to_string (kv.second) + " \xC3\x97 " + kv.first;
			return s;
		};
		for (const auto& kv : byBlock)
			b << "<tr><td>" << Html::Esc (Structure::BlockName (model, kv.first)) << "</td><td class=\"n\">" << kv.second.count << "</td><td>" << Html::Esc (kinds (kv.second)) << "</td><td class=\"n\">"
			  << Html::Num (kv.second.net) << "</td><td class=\"n\">" << Html::Num (kv.second.withCommon) << "</td><td class=\"n\">" << (kv.second.gross > 0 ? Html::Num (kv.second.gross) : std::string ("\xE2\x80\x93")) << "</td></tr>";
		b << "<tr class=\"sum\"><td>GJITHSEJ PROJEKTI</td><td class=\"n\">" << all.count << "</td><td>" << Html::Esc (kinds (all)) << "</td><td class=\"n\">" << Html::Num (all.net)
		  << "</td><td class=\"n\">" << Html::Num (all.withCommon) << "</td><td class=\"n\">" << (all.gross > 0 ? Html::Num (all.gross) : std::string ("\xE2\x80\x93")) << "</td></tr></table>";

		// --- specifikimi i hapesirave per cdo njesi
		b << "<h2>Specifikimi i hap\xC3\xABsirave sipas nj\xC3\xABsive</h2>";
		for (const Json::Value& u : units.AsArray ()) {
			b << "<h3>" << Html::Esc (u.GetString ("number")) << " \xC2\xB7 " << Html::Esc (KindText (u)) << " \xC2\xB7 " << Html::Esc (StoryLabel (model, u.GetInt ("story"))) << "</h3>";
			b << "<table><tr><th style=\"width:8%\">Nr.</th><th>Hap\xC3\xABsira</th><th class=\"n\" style=\"width:14%\">Sip. (m\xC2\xB2)</th><th class=\"n\" style=\"width:10%\">Koef.</th><th class=\"n\" style=\"width:16%\">E llogaritur (m\xC2\xB2)</th></tr>";
			int i = 0;
			for (const Json::Value& r : u["rooms"].AsArray ())
				b << "<tr><td>" << ++i << "</td><td>" << Html::Esc (r.GetString ("name")) << "</td><td class=\"n\">" << Html::Num (r.GetNumber ("area")) << "</td><td class=\"n\">"
				  << Html::Num (r.GetNumber ("k")) << "</td><td class=\"n\">" << Html::Num (r.GetNumber ("reduced")) << "</td></tr>";
			b << "<tr class=\"sub\"><td></td><td>Neto e llogaritur</td><td></td><td></td><td class=\"n\">" << Html::Num (u.GetNumber ("net")) << "</td></tr>";
			if (u.GetNumber ("commonShare") > 0)
				b << "<tr><td></td><td>Pjesa e hap\xC3\xABsirave t\xC3\xAB p\xC3\xABrbashk\xC3\xABta</td><td></td><td></td><td class=\"n\">" << Html::Num (u.GetNumber ("commonShare")) << "</td></tr>";
			b << "<tr class=\"sum\"><td></td><td>GJITHSEJ</td><td></td><td></td><td class=\"n\">" << Html::Num (u.GetNumber ("withCommon")) << "</td></tr></table>";
		}
	}

	std::vector<ReportOutput> outs;
	const std::string base = Html::Slug (project.GetString ("nr", "ARBS")) + "_" + Html::Slug (project.GetString ("faza_kodi", "PI")) + "_A24_Specifikimi-i-njesive";
	const std::string title = "SPECIFIKIMI I NJ\xC3\x8BSIVE";
	outs.push_back ({ base + ".html", Document (title, b.str (), project, office, false), "text/html" });
	outs.push_back ({ base + ".doc", Document (title, b.str (), project, office, true), "application/msword" });
	outs.push_back ({ base + ".csv", Csv (csv), "text/csv" });
	return outs;
}

std::string Reports::BlocksChapter (const Json::Value& project, const Json::Value& model)
{
	const StructureOptions so = Structure::OptionsFromProject (project);
	const Json::Value units = Structure::Units (model, so, project["njesite_manual"])["units"];
	const auto parts = Structure::SplitModel (model, so);
	if (parts.size () <= 1 && units.Size () == 0) return "";

	struct Ind { std::string code, name; int above = 0, below = 0, entrances = 0, doors = 0, windows = 0; double net = 0, bts = 0; std::map<std::string, int> kinds; };
	std::vector<Ind> inds;
	for (const auto& p : parts) {
		Ind d;
		d.code = p.first;
		d.name = Structure::BlockName (model, p.first);
		std::set<int> st;
		for (const Json::Value& z : p.second["zones"].AsArray ()) { d.net += z.GetNumber ("area"); st.insert (z.GetInt ("story")); }
		for (int s : st) (s < 0 ? d.below : d.above)++;
		for (const Json::Value& o : p.second["openings"].AsArray ()) (o.GetString ("kind") == "door" ? d.doors : d.windows)++;
		std::set<std::string> ent;
		for (const Json::Value& e : p.second["entrances"].AsArray ()) ent.insert (e.GetString ("code"));
		for (const Json::Value& u : units.AsArray ())
			if (u.GetString ("block") == p.first || (p.first.empty ())) {
				ent.insert (u.GetString ("entrance"));
				std::string k = u.GetString ("kindLabel");
				if (!u.GetString ("structure").empty ()) k += " " + u.GetString ("structure");
				d.kinds[k]++;
			}
		d.entrances = static_cast<int> (ent.size ());
		const Json::Value& given = project["blloqet_bts"][p.first];
		d.bts = given.IsNumber () && given.AsNumber () > 0 ? given.AsNumber () : std::round (d.net * 1.20 * 10.0) / 10.0;
		inds.push_back (d);
	}

	std::ostringstream b;
	for (const Ind& d : inds) {
		b << "<h3>" << Html::Esc (d.name) << (d.code.empty () || d.code == Structure::CommonPart () ? "" : " (" + Html::Esc (d.code) + ")") << "</h3><table>";
		b << "<tr><td style=\"width:45%\">Katet</td><td>" << d.below << " n\xC3\xABntok\xC3\xAB, " << d.above << " mbi tok\xC3\xAB</td></tr>";
		if (d.code != Structure::CommonPart ()) b << "<tr><td>Hyrjet</td><td>" << d.entrances << "</td></tr>";
		b << "<tr><td>Sip\xC3\xABrfaqja neto (zonat)</td><td>" << Html::Num (d.net) << " m\xC2\xB2</td></tr><tr><td>BTS</td><td>" << Html::Num (d.bts, 1) << " m\xC2\xB2</td></tr>";
		if (!d.kinds.empty ()) {
			std::string k;
			for (const auto& kv : d.kinds) k += (k.empty () ? "" : "; ") + std::to_string (kv.second) + " \xC3\x97 " + kv.first;
			b << "<tr><td>Nj\xC3\xABsit\xC3\xAB</td><td>" << Html::Esc (k) << "</td></tr>";
		}
		b << "<tr><td>Dyer / dritare</td><td>" << d.doors << " / " << d.windows << "</td></tr></table>";
	}
	// rekapitulimi
	b << "<h3>Rekapitulimi i treguesve sipas blloqeve</h3><table><tr><th>Treguesi</th>";
	for (const Ind& d : inds) b << "<th class=\"n\">" << Html::Esc (d.code.empty () ? d.name : d.code) << "</th>";
	b << "<th class=\"n\">GJITHSEJ</th></tr>";
	auto row = [&] (const char* label, std::function<double (const Ind&)> f, int dec) {
		double s = 0;
		b << "<tr><td>" << label << "</td>";
		for (const Ind& d : inds) { const double v = f (d); s += v; b << "<td class=\"n\">" << Html::Num (v, dec) << "</td>"; }
		b << "<td class=\"n\"><b>" << Html::Num (s, dec) << "</b></td></tr>";
	};
	row ("BTS (m\xC2\xB2)", [] (const Ind& d) { return d.bts; }, 1);
	row ("Sip\xC3\xABrfaqja neto (m\xC2\xB2)", [] (const Ind& d) { return d.net; }, 2);
	row ("Nj\xC3\xABsi gjithsej", [] (const Ind& d) { int n = 0; for (const auto& kv : d.kinds) n += kv.second; return static_cast<double> (n); }, 0);
	std::set<std::string> allKinds;
	for (const Ind& d : inds) for (const auto& kv : d.kinds) allKinds.insert (kv.first);
	for (const std::string& k : allKinds) {
		const std::string label = "\xE2\x80\x93 " + Html::Esc (k);
		row (label.c_str (), [k] (const Ind& d) { auto it = d.kinds.find (k); return it == d.kinds.end () ? 0.0 : static_cast<double> (it->second); }, 0);
	}
	row ("Dyer", [] (const Ind& d) { return static_cast<double> (d.doors); }, 0);
	row ("Dritare", [] (const Ind& d) { return static_cast<double> (d.windows); }, 0);
	b << "</table>";
	return b.str ();
}

}	// namespace ARBS
