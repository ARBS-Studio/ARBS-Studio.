// =============================================================================
// ARBS Studio - shtresa Archicad : CompositeBuilder.hpp
// Krijon Building Materials (me lambda, rho, c) dhe Composites nga katalogu i Kosoves.
// Emrat: materialet "ARBS . <Emri>", kompozitat "<KODI> <Emri>".
// =============================================================================
#pragma once

#include "AC/ArbsUtils.hpp"

namespace ARBS {
namespace AC {

class CompositeBuilder {
public:
	// codes bosh = te gjitha. Kthen { materials, composites, updated, errors[] }
	static Json::Value Create (const std::vector<std::string>& codes, bool updateExisting);

	static std::string MaterialAttributeName (const Material& m);
	static std::string CompositeAttributeName (const Composite& c);
	// "MJ-01 Mur i ..." -> "MJ-01" (ose "" nese nuk eshte kompozite ARBS)
	static std::string CodeFromName (const std::string& name);

private:
	static bool EnsureMaterial (const Material& m, bool update, API_AttributeIndex* index, Json::Value& report);
	static API_AttributeIndex FindFill (const std::vector<std::string>& candidates);
	static API_AttributeIndex FindSurface (const std::string& group);
};

}	// namespace AC
}	// namespace ARBS
