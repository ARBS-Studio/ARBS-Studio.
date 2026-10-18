// =============================================================================
// ARBS Studio - Core : BillOfQuantities.hpp
// Paramasa dhe parallogaria: sasite nga modeli (kompozitat sipas shtresave, elementet
// bazike, zonat, hapjet, objektet) x cmimet njesi nga Data/prices.json (tregu i Kosoves).
// =============================================================================
#pragma once

#include "Catalog.hpp"
#include "Json.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ARBS {

struct BoqOptions {
	double vat = 18.0;				// TVSH % (norma standarde ne Kosove)
	double contingency = 5.0;		// te paparashikuarat %
	double factor = 1.0;			// koeficienti i cmimeve (rajoni, kategoria e kontraktorit)
	bool   includeZero = false;		// shfaq edhe pozicionet me sasi 0
	Json::Value overrides = Json::Value::MakeObject ();	// { "3.02": 125.0, ... } cmimet e zyres
};

struct BoqLine {
	std::string code, group, unit, desc, source;
	double qty = 0.0, price = 0.0, value = 0.0;
	double thicknessCm = 0.0;		// > 0 kur cmimi varet nga trashesia
	bool   estimated = false;		// sasia nga vleresimi (termat "alt")
	bool   overridden = false;		// cmim i ndryshuar nga zyra
};

struct BoqGroup {
	std::string id, name;
	double total = 0.0;
	std::vector<BoqLine> lines;
};

struct BoqResult {
	std::vector<BoqGroup> groups;
	double works = 0.0, contingency = 0.0, net = 0.0, vat = 0.0, total = 0.0;
	double bts = 0.0, perM2 = 0.0;
	BoqOptions options;
	std::string priceLevel, source;
	std::vector<std::string> notes;
	std::map<std::string, double> quantities;	// celesat baze te modelit
};

class BillOfQuantities {
public:
	// Celesat e sasive (shih komentin ne Data/prices.json). sources: celesi -> kodet/burimet.
	static std::map<std::string, double> ModelQuantities (const Catalog& cat, const Json::Value& model, const Json::Value& project,
														  std::map<std::string, std::set<std::string>>* sources = nullptr,
														  std::vector<std::string>* notes = nullptr);

	static BoqResult Compute (const Catalog& cat, const Json::Value& model, const Json::Value& project, const BoqOptions& opt);

	static BoqOptions  OptionsFromJson (const Json::Value& req, const Catalog& cat);
	static Json::Value ToJson (const BoqResult& r);
	static Json::Value PriceList (const Catalog& cat);	// per paleten
	static std::string Money (double v);				// 12.345,67
};

}	// namespace ARBS
