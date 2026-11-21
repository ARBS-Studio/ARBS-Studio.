// =============================================================================
// ARBS Studio - Core : Xlsx.cpp
// =============================================================================
#include "Xlsx.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>

namespace ARBS {
namespace Xlsx {

// -----------------------------------------------------------------------------
// ZIP (metoda "store", pa kompresim) - mjafton per Excel
// -----------------------------------------------------------------------------
namespace {

uint32_t Crc32 (const std::string& data)
{
	static uint32_t table[256];
	static bool init = false;
	if (!init) {
		for (uint32_t i = 0; i < 256; ++i) {
			uint32_t c = i;
			for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			table[i] = c;
		}
		init = true;
	}
	uint32_t crc = 0xFFFFFFFFu;
	for (unsigned char ch : data) crc = table[(crc ^ ch) & 0xFF] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFu;
}

void Put16 (std::string& s, uint32_t v) { s += static_cast<char> (v & 0xFF); s += static_cast<char> ((v >> 8) & 0xFF); }
void Put32 (std::string& s, uint32_t v) { Put16 (s, v & 0xFFFF); Put16 (s, (v >> 16) & 0xFFFF); }

struct ZipEntry { std::string name, data; uint32_t crc = 0, offset = 0; };

std::string Zip (const std::vector<std::pair<std::string, std::string>>& files)
{
	std::string out;
	std::vector<ZipEntry> entries;
	const uint32_t dosTime = 0, dosDate = (46u << 9) | (1u << 5) | 1u;	// 2026-01-01
	for (const auto& f : files) {
		ZipEntry e;
		e.name = f.first;
		e.data = f.second;
		e.crc = Crc32 (f.second);
		e.offset = static_cast<uint32_t> (out.size ());
		Put32 (out, 0x04034b50u);
		Put16 (out, 20); Put16 (out, 0x0800); Put16 (out, 0);	// versioni, flamuri UTF-8, store
		Put16 (out, dosTime); Put16 (out, dosDate);
		Put32 (out, e.crc);
		Put32 (out, static_cast<uint32_t> (e.data.size ()));
		Put32 (out, static_cast<uint32_t> (e.data.size ()));
		Put16 (out, static_cast<uint32_t> (e.name.size ())); Put16 (out, 0);
		out += e.name;
		out += e.data;
		entries.push_back (e);
	}
	const uint32_t cdStart = static_cast<uint32_t> (out.size ());
	for (const ZipEntry& e : entries) {
		Put32 (out, 0x02014b50u);
		Put16 (out, 20); Put16 (out, 20); Put16 (out, 0x0800); Put16 (out, 0);
		Put16 (out, dosTime); Put16 (out, dosDate);
		Put32 (out, e.crc);
		Put32 (out, static_cast<uint32_t> (e.data.size ()));
		Put32 (out, static_cast<uint32_t> (e.data.size ()));
		Put16 (out, static_cast<uint32_t> (e.name.size ()));
		Put16 (out, 0); Put16 (out, 0); Put16 (out, 0); Put16 (out, 0);
		Put32 (out, 0);
		Put32 (out, e.offset);
		out += e.name;
	}
	const uint32_t cdSize = static_cast<uint32_t> (out.size ()) - cdStart;
	Put32 (out, 0x06054b50u);
	Put16 (out, 0); Put16 (out, 0);
	Put16 (out, static_cast<uint32_t> (entries.size ())); Put16 (out, static_cast<uint32_t> (entries.size ()));
	Put32 (out, cdSize); Put32 (out, cdStart);
	Put16 (out, 0);
	return out;
}

std::string Xml (const std::string& s)
{
	std::string o;
	o.reserve (s.size () + 8);
	for (unsigned char c : s) {
		switch (c) {
			case '&': o += "&amp;"; break;
			case '<': o += "&lt;"; break;
			case '>': o += "&gt;"; break;
			case '"': o += "&quot;"; break;
			default:
				if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') break;	// karaktere te pavlefshme ne XML
				o += static_cast<char> (c);
		}
	}
	return o;
}

std::string NumStr (double v)
{
	if (!std::isfinite (v)) v = 0.0;
	char buf[40];
	std::snprintf (buf, sizeof (buf), "%.10g", v);
	return buf;
}

std::string SafeSheetName (std::string n)
{
	for (char& c : n) if (c == '[' || c == ']' || c == ':' || c == '*' || c == '?' || c == '/' || c == '\\') c = '-';
	if (n.empty ()) n = "Flete";
	if (n.size () > 31) n = n.substr (0, 31);
	return n;
}

const char* kStyles =
	"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
	"<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
	"<numFmts count=\"3\">"
	"<numFmt numFmtId=\"164\" formatCode=\"#,##0.00\"/>"
	"<numFmt numFmtId=\"165\" formatCode=\"#,##0.00\\ &quot;\xE2\x82\xAC&quot;\"/>"
	"<numFmt numFmtId=\"166\" formatCode=\"0.0%\"/>"
	"</numFmts>"
	"<fonts count=\"6\">"
	"<font><sz val=\"10\"/><name val=\"Arial\"/><family val=\"2\"/></font>"
	"<font><b/><sz val=\"10\"/><name val=\"Arial\"/><family val=\"2\"/></font>"
	"<font><b/><sz val=\"14\"/><color rgb=\"FF002142\"/><name val=\"Arial\"/><family val=\"2\"/></font>"
	"<font><i/><sz val=\"9\"/><color rgb=\"FF5C6B79\"/><name val=\"Arial\"/><family val=\"2\"/></font>"
	"<font><b/><sz val=\"10\"/><color rgb=\"FFFFFFFF\"/><name val=\"Arial\"/><family val=\"2\"/></font>"
	"<font><b/><sz val=\"11\"/><name val=\"Arial\"/><family val=\"2\"/></font>"
	"</fonts>"
	"<fills count=\"7\">"
	"<fill><patternFill patternType=\"none\"/></fill>"
	"<fill><patternFill patternType=\"gray125\"/></fill>"
	"<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF002142\"/><bgColor indexed=\"64\"/></patternFill></fill>"
	"<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFE8EFE9\"/><bgColor indexed=\"64\"/></patternFill></fill>"
	"<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFDCE6F0\"/><bgColor indexed=\"64\"/></patternFill></fill>"
	"<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF135027\"/><bgColor indexed=\"64\"/></patternFill></fill>"
	"<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFFFF8DC\"/><bgColor indexed=\"64\"/></patternFill></fill>"
	"</fills>"
	"<borders count=\"2\">"
	"<border><left/><right/><top/><bottom/><diagonal/></border>"
	"<border><left style=\"thin\"><color rgb=\"FFBFC7CF\"/></left><right style=\"thin\"><color rgb=\"FFBFC7CF\"/></right>"
	"<top style=\"thin\"><color rgb=\"FFBFC7CF\"/></top><bottom style=\"thin\"><color rgb=\"FFBFC7CF\"/></bottom><diagonal/></border>"
	"</borders>"
	"<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
	"<cellXfs count=\"20\">"
	"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
	"<xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"5\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"3\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"4\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"1\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"center\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"top\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"top\"/></xf>"
	"<xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"top\"/></xf>"
	"<xf numFmtId=\"165\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"top\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"1\" fillId=\"4\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"center\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"165\" fontId=\"1\" fillId=\"4\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"center\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"4\" fillId=\"5\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"5\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"center\"/></xf>"
	"<xf numFmtId=\"165\" fontId=\"5\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"center\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"3\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyAlignment=\"1\"><alignment vertical=\"top\" wrapText=\"1\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyAlignment=\"1\"><alignment vertical=\"center\"/></xf>"
	"<xf numFmtId=\"166\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"center\"/></xf>"
	"<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
	"<xf numFmtId=\"165\" fontId=\"0\" fillId=\"6\" borderId=\"1\" xfId=\"0\" applyNumberFormat=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment vertical=\"top\"/></xf>"
	"</cellXfs>"
	"<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
	"</styleSheet>";

}	// namespace

// -----------------------------------------------------------------------------
// ndihmese
// -----------------------------------------------------------------------------

std::string ColName (int col)
{
	std::string s;
	int c = col + 1;
	while (c > 0) {
		const int r = (c - 1) % 26;
		s.insert (s.begin (), static_cast<char> ('A' + r));
		c = (c - 1) / 26;
	}
	return s;
}

std::string Ref (int row, int col) { return ColName (col) + std::to_string (row + 1); }

std::string SheetRef (const std::string& sheet, int row, int col)
{
	std::string n = SafeSheetName (sheet);
	std::string q;
	for (char c : n) { q += c; if (c == '\'') q += '\''; }
	return "'" + q + "'!" + Ref (row, col);
}

double EstimateRowHeight (const std::string& text, double colWidth, double minHeight)
{
	// numri i karaktereve (UTF-8) per rresht ~ gjeresia e kolones x 1,1 (Arial 10)
	size_t chars = 0;
	for (unsigned char c : text) if ((c & 0xC0) != 0x80) ++chars;
	const double perLine = std::max (10.0, colWidth * 1.1);
	const double lines = std::ceil (static_cast<double> (chars) / perLine);
	return std::max (minHeight, lines * 12.8 + 3.0);
}

bool ImageSize (const std::string& b, const std::string& ext, int* w, int* h)
{
	auto u8 = [&] (size_t i) { return static_cast<unsigned> (static_cast<unsigned char> (b[i])); };
	if (ext == "png") {
		if (b.size () < 24) return false;
		*w = static_cast<int> ((u8 (16) << 24) | (u8 (17) << 16) | (u8 (18) << 8) | u8 (19));
		*h = static_cast<int> ((u8 (20) << 24) | (u8 (21) << 16) | (u8 (22) << 8) | u8 (23));
		return *w > 0 && *h > 0;
	}
	// JPEG: segmenti SOF0..SOF15 (pa DHT/JPG/DAC)
	size_t i = 2;
	while (i + 9 < b.size ()) {
		if (u8 (i) != 0xFF) { ++i; continue; }
		const unsigned m = u8 (i + 1);
		const size_t len = (u8 (i + 2) << 8) | u8 (i + 3);
		if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
			*h = static_cast<int> ((u8 (i + 5) << 8) | u8 (i + 6));
			*w = static_cast<int> ((u8 (i + 7) << 8) | u8 (i + 8));
			return *w > 0 && *h > 0;
		}
		i += 2 + len;
	}
	return false;
}

bool DecodeDataUri (const std::string& uri, std::string* bytes, std::string* ext)
{
	const size_t comma = uri.find (',');
	if (uri.rfind ("data:", 0) != 0 || comma == std::string::npos || uri.find (";base64") == std::string::npos) return false;
	const std::string mime = uri.substr (5, uri.find (';') - 5);
	if (mime == "image/png") *ext = "png";
	else if (mime == "image/jpeg" || mime == "image/jpg") *ext = "jpeg";
	else return false;
	static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	bytes->clear ();
	int val = 0, bits = -8;
	for (size_t i = comma + 1; i < uri.size (); ++i) {
		const char c = uri[i];
		if (c == '=') break;
		const size_t p = chars.find (c);
		if (p == std::string::npos) continue;
		val = (val << 6) + static_cast<int> (p);
		bits += 6;
		if (bits >= 0) {
			bytes->push_back (static_cast<char> ((val >> bits) & 0xFF));
			bits -= 8;
		}
	}
	return !bytes->empty ();
}

// -----------------------------------------------------------------------------
// Sheet
// -----------------------------------------------------------------------------

void Sheet::Text (int row, int col, const std::string& s, int style)
{
	Cell& c = rows[row][col];
	c.kind = Cell::Text;
	c.text = s;
	c.style = style;
}

void Sheet::Number (int row, int col, double v, int style)
{
	Cell& c = rows[row][col];
	c.kind = Cell::Number;
	c.number = v;
	c.style = style;
}

void Sheet::Formula (int row, int col, const std::string& f, double cached, int style)
{
	Cell& c = rows[row][col];
	c.kind = Cell::Formula;
	c.text = f;
	c.number = cached;
	c.style = style;
}

void Sheet::Blank (int row, int col, int style)
{
	Cell& c = rows[row][col];
	c.kind = Cell::Empty;
	c.style = style;
}

void Sheet::Merge (int row1, int col1, int row2, int col2)
{
	merges.push_back (Ref (row1, col1) + ":" + Ref (row2, col2));
}

// -----------------------------------------------------------------------------
// Workbook
// -----------------------------------------------------------------------------

Sheet& Workbook::AddSheet (const std::string& name)
{
	sheets.emplace_back (SafeSheetName (name));
	return sheets.back ();
}

std::string Workbook::Save () const
{
	std::vector<std::pair<std::string, std::string>> files;
	const char* head = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

	// [Content_Types].xml
	std::ostringstream ct;
	ct << head << "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
	   << "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
	   << "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
	   << "<Default Extension=\"png\" ContentType=\"image/png\"/>"
	   << "<Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>"
	   << "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
	   << "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
	   << "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
	   << "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>";
	int drawingNo = 0;
	for (size_t i = 0; i < sheets.size (); ++i) {
		ct << "<Override PartName=\"/xl/worksheets/sheet" << (i + 1) << ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
		if (sheets[i].hasImage)
			ct << "<Override PartName=\"/xl/drawings/drawing" << (++drawingNo) << ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>";
	}
	ct << "</Types>";
	files.push_back ({ "[Content_Types].xml", ct.str () });

	files.push_back ({ "_rels/.rels", std::string (head) +
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
		"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
		"<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
		"</Relationships>" });
	files.push_back ({ "docProps/core.xml", std::string (head) +
		"<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
		"xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
		"<dc:creator>ARBS Studio</dc:creator><dc:title>Paramasa dhe parallogaria</dc:title></cp:coreProperties>" });
	files.push_back ({ "docProps/app.xml", std::string (head) +
		"<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\"><Application>ARBS Studio</Application></Properties>" });

	// workbook.xml
	std::ostringstream wb;
	wb << head << "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
	   << "<bookViews><workbookView xWindow=\"0\" yWindow=\"0\" windowWidth=\"24000\" windowHeight=\"14000\"/></bookViews><sheets>";
	for (size_t i = 0; i < sheets.size (); ++i)
		wb << "<sheet name=\"" << Xml (sheets[i].name) << "\" sheetId=\"" << (i + 1) << "\" r:id=\"rId" << (i + 1) << "\"/>";
	wb << "</sheets>";
	bool anyTitles = false;
	for (const Sheet& s : sheets) if (s.repeatRow1 >= 0) anyTitles = true;
	if (anyTitles) {
		wb << "<definedNames>";
		for (size_t i = 0; i < sheets.size (); ++i)
			if (sheets[i].repeatRow1 >= 0) {
				std::string q;
				for (char c : sheets[i].name) { q += c; if (c == '\'') q += '\''; }
				wb << "<definedName name=\"_xlnm.Print_Titles\" localSheetId=\"" << i << "\">'" << Xml (q) << "'!$" << (sheets[i].repeatRow1 + 1) << ":$" << (sheets[i].repeatRow2 + 1) << "</definedName>";
			}
		wb << "</definedNames>";
	}
	wb << "<calcPr calcId=\"191029\" fullCalcOnLoad=\"1\"/></workbook>";
	files.push_back ({ "xl/workbook.xml", wb.str () });

	std::ostringstream wbr;
	wbr << head << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
	for (size_t i = 0; i < sheets.size (); ++i)
		wbr << "<Relationship Id=\"rId" << (i + 1) << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet" << (i + 1) << ".xml\"/>";
	wbr << "<Relationship Id=\"rId" << (sheets.size () + 1) << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/></Relationships>";
	files.push_back ({ "xl/_rels/workbook.xml.rels", wbr.str () });
	files.push_back ({ "xl/styles.xml", kStyles });

	// fletet
	drawingNo = 0;
	for (size_t i = 0; i < sheets.size (); ++i) {
		const Sheet& s = sheets[i];
		std::ostringstream x;
		x << head << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
		  << "<sheetPr><pageSetUpPr fitToPage=\"1\"/></sheetPr>"
		  << "<sheetViews><sheetView workbookViewId=\"0\" showGridLines=\"0\"" << (i == 0 ? " tabSelected=\"1\"" : "") << "/></sheetViews>"
		  << "<sheetFormatPr defaultRowHeight=\"15\"/>";
		if (!s.colWidths.empty ()) {
			x << "<cols>";
			for (const auto& cw : s.colWidths)
				x << "<col min=\"" << (cw.first + 1) << "\" max=\"" << (cw.first + 1) << "\" width=\"" << NumStr (cw.second) << "\" customWidth=\"1\"/>";
			x << "</cols>";
		}
		x << "<sheetData>";
		std::map<int, bool> rowSet;
		for (const auto& r : s.rows) rowSet[r.first] = true;
		for (const auto& rh : s.rowHeights) rowSet[rh.first] = true;
		for (const auto& rr : rowSet) {
			const int row = rr.first;
			x << "<row r=\"" << (row + 1) << "\"";
			auto h = s.rowHeights.find (row);
			if (h != s.rowHeights.end ()) x << " ht=\"" << NumStr (h->second) << "\" customHeight=\"1\"";
			x << ">";
			auto cells = s.rows.find (row);
			if (cells != s.rows.end ())
				for (const auto& cc : cells->second) {
					const Cell& c = cc.second;
					const std::string ref = Ref (row, cc.first);
					switch (c.kind) {
						case Cell::Text:
							x << "<c r=\"" << ref << "\" s=\"" << c.style << "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" << Xml (c.text) << "</t></is></c>";
							break;
						case Cell::Number:
							x << "<c r=\"" << ref << "\" s=\"" << c.style << "\"><v>" << NumStr (c.number) << "</v></c>";
							break;
						case Cell::Formula:
							x << "<c r=\"" << ref << "\" s=\"" << c.style << "\"><f>" << Xml (c.text) << "</f><v>" << NumStr (c.number) << "</v></c>";
							break;
						default:
							x << "<c r=\"" << ref << "\" s=\"" << c.style << "\"/>";
					}
				}
			x << "</row>";
		}
		x << "</sheetData>";
		if (!s.merges.empty ()) {
			x << "<mergeCells count=\"" << s.merges.size () << "\">";
			for (const std::string& m : s.merges) x << "<mergeCell ref=\"" << m << "\"/>";
			x << "</mergeCells>";
		}
		x << "<pageMargins left=\"0.5\" right=\"0.4\" top=\"0.5\" bottom=\"0.6\" header=\"0.3\" footer=\"0.3\"/>"
		  << "<pageSetup paperSize=\"9\" orientation=\"" << (s.landscape ? "landscape" : "portrait") << "\" fitToWidth=\"1\" fitToHeight=\"0\"/>"
		  << "<headerFooter><oddFooter>&amp;L&amp;8" << Xml (s.name) << "&amp;R&amp;8Faqja &amp;P / &amp;N</oddFooter></headerFooter>";
		if (s.hasImage) x << "<drawing r:id=\"rId1\"/>";
		x << "</worksheet>";
		files.push_back ({ "xl/worksheets/sheet" + std::to_string (i + 1) + ".xml", x.str () });

		if (s.hasImage) {
			++drawingNo;
			const std::string media = "image" + std::to_string (drawingNo) + "." + s.image.ext;
			files.push_back ({ "xl/worksheets/_rels/sheet" + std::to_string (i + 1) + ".xml.rels", std::string (head) +
				"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
				"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing" + std::to_string (drawingNo) + ".xml\"/>"
				"</Relationships>" });
			const long long emuW = static_cast<long long> (s.image.widthPx) * 9525LL;
			const long long emuH = static_cast<long long> (s.image.heightPx) * 9525LL;
			std::ostringstream d;
			d << head << "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			  << "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			  << "<xdr:oneCellAnchor><xdr:from><xdr:col>" << s.image.col << "</xdr:col><xdr:colOff>38100</xdr:colOff><xdr:row>" << s.image.row
			  << "</xdr:row><xdr:rowOff>38100</xdr:rowOff></xdr:from><xdr:ext cx=\"" << emuW << "\" cy=\"" << emuH << "\"/>"
			  << "<xdr:pic><xdr:nvPicPr><xdr:cNvPr id=\"2\" name=\"Logo\"/><xdr:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></xdr:cNvPicPr></xdr:nvPicPr>"
			  << "<xdr:blipFill><a:blip r:embed=\"rId1\"/><a:stretch><a:fillRect/></a:stretch></xdr:blipFill>"
			  << "<xdr:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"" << emuW << "\" cy=\"" << emuH << "\"/></a:xfrm><a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></xdr:spPr>"
			  << "</xdr:pic><xdr:clientData/></xdr:oneCellAnchor></xdr:wsDr>";
			files.push_back ({ "xl/drawings/drawing" + std::to_string (drawingNo) + ".xml", d.str () });
			files.push_back ({ "xl/drawings/_rels/drawing" + std::to_string (drawingNo) + ".xml.rels", std::string (head) +
				"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
				"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"../media/" + media + "\"/>"
				"</Relationships>" });
			files.push_back ({ "xl/media/" + media, s.image.bytes });
		}
	}
	return Zip (files);
}

}	// namespace Xlsx
}	// namespace ARBS
