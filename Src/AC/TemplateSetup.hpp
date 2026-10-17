// =============================================================================
// ARBS Studio - shtresa Archicad : TemplateSetup.hpp
// Krijon template-in e punes ne shqip: shtresat, kombinimet e shtresave,
// strukturen e View Map sipas fazave dhe Layout Book (subset + fleta).
// =============================================================================
#pragma once

#include "AC/ArbsUtils.hpp"

namespace ARBS {
namespace AC {

struct SetupOptions {
	bool layers = true;
	bool combinations = true;
	bool viewMap = true;
	bool layoutBook = true;
	bool perStoreySheets = true;
	std::vector<std::string> phases = { "PK", "PI", "PKR", "PZ" };
};

class TemplateSetup {
public:
	// Kthen raport JSON: { created:{layers,combinations,folders,views,subsets,layouts}, skipped, errors[] }
	static Json::Value Run (const SetupOptions& opt);

	static GSErrCode RunCommand (const char* name, const std::function<GSErrCode ()>& fn);

	static bool FindAttributeByName (API_AttrTypeID type, const GS::UniString& name, API_AttributeIndex* index);

private:
	static int CreateLayers (Json::Value& report);
	static int CreateCombinations (Json::Value& report);
	static void CreateViewMap (const SetupOptions& opt, Json::Value& report);
	static void CreateLayoutBook (const SetupOptions& opt, Json::Value& report);
};

}	// namespace AC
}	// namespace ARBS
