// =============================================================================
// ARBS Studio - Core : Xlsx.hpp
// Shkrues i thjeshte i skedareve Excel (.xlsx, Office Open XML) pa biblioteka te jashtme:
// flete, qeliza me tekst / numra / formula (me vlere te llogaritur), stile fikse, bashkim
// qelizash, gjeresi kolonash, lartesi rreshtash, nje imazh (logoja) dhe parametrat e printimit.
// =============================================================================
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ARBS {
namespace Xlsx {

// stilet e gatshme (indekset e cellXfs ne styles.xml)
enum Style : int {
	S_Default = 0,
	S_Title,		// 14 bold, ne qender
	S_Subtitle,		// 11 bold, ne qender
	S_Small,		// 9 kursiv gri, ne qender
	S_Header,		// bold i bardhe mbi blu te erret, kufij
	S_Group,		// bold mbi te gjelber te lehte, kufij
	S_Text,			// tekst i mbeshtjelle, kufij
	S_Center,		// ne qender, kufij
	S_Number,		// #,##0.00, kufij
	S_Money,		// #,##0.00 EUR, kufij
	S_TotalLabel,	// bold mbi blu te lehte, kufij
	S_TotalMoney,	// bold, EUR, blu e lehte
	S_RecapHeader,	// bold i bardhe mbi te gjelber
	S_GrandLabel,	// bold 11 mbi te gjelber te lehte
	S_GrandMoney,	// bold 11, EUR
	S_Note,			// kursiv gri, i mbeshtjelle
	S_Plain,		// tekst pa kufij
	S_Percent,		// 0.0%, kufij
	S_Bold,			// bold pa kufij
	S_InputMoney,	// EUR, kufij, sfond i verdhe i lehte (qelize per t'u plotesuar)
	S_Count
};

struct Cell {
	enum Kind { Text, Number, Formula, Empty } kind = Empty;
	std::string text;		// teksti ose formula (pa "=")
	double      number = 0.0;	// numri ose vlera e llogaritur e formules
	int         style = S_Default;
};

struct Image {
	std::string bytes;		// PNG ose JPEG
	std::string ext;		// "png" | "jpeg"
	int col = 0, row = 0;	// 0-bazuar
	int widthPx = 0, heightPx = 0;
};

class Sheet {
public:
	explicit Sheet (const std::string& sheetName) : name (sheetName) {}

	void Text (int row, int col, const std::string& s, int style = S_Text);
	void Number (int row, int col, double v, int style = S_Number);
	void Formula (int row, int col, const std::string& f, double cached, int style = S_Money);
	void Blank (int row, int col, int style);
	void Merge (int row1, int col1, int row2, int col2);
	void ColWidth (int col, double width) { colWidths[col] = width; }
	void RowHeight (int row, double points) { rowHeights[row] = points; }
	void SetImage (const Image& img) { image = img; hasImage = true; }

	std::string name;
	bool   landscape = false;
	int    repeatRow1 = -1, repeatRow2 = -1;	// rreshtat e kokes qe perseriten ne cdo faqe (0-bazuar)

	// perdoren nga Workbook
	std::map<int, std::map<int, Cell>> rows;
	std::map<int, double> colWidths;
	std::map<int, double> rowHeights;
	std::vector<std::string> merges;
	Image image;
	bool  hasImage = false;
};

class Workbook {
public:
	Sheet& AddSheet (const std::string& name);
	std::string Save () const;		// permbajtja binare e skedarit .xlsx

	std::vector<Sheet> sheets;
};

std::string ColName (int col);					// 0 -> "A"
std::string Ref (int row, int col);				// (0,0) -> "A1"
std::string SheetRef (const std::string& sheet, int row, int col);	// 'B1'!F12
bool ImageSize (const std::string& bytes, const std::string& ext, int* w, int* h);
bool DecodeDataUri (const std::string& uri, std::string* bytes, std::string* ext);
double EstimateRowHeight (const std::string& text, double colWidth, double minHeight = 15.0);

}	// namespace Xlsx
}	// namespace ARBS
