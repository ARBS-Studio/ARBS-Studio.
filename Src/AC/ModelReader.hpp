// =============================================================================
// ARBS Studio - shtresa Archicad : ModelReader.hpp
// Lexon modelin e Archicad-it ne formatin JSON qe perdorin raportet (Core/Reports).
// =============================================================================
#pragma once

#include "AC/ArbsUtils.hpp"

namespace ARBS {
namespace AC {

class ModelReader {
public:
	// selectionOnly: vetem elementet e selektuara (per dyer/dritare/objekte)
	static Json::Value Read (bool selectionOnly = false);

	static Json::Value Stories ();
	static double      NorthDeg ();		// rrotullimi i veriut te projektit (grade)
};

}	// namespace AC
}	// namespace ARBS
