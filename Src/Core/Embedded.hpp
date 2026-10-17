// =============================================================================
// ARBS Studio - Core : Embedded.hpp
// Qasja ne skedaret e ngulitur ne binar (gjenerohet nga cmake/EmbedFiles.cmake)
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ARBS {
namespace Embedded {

bool Get (const std::string& name, std::string& out);
std::vector<std::string> List ();

}	// namespace Embedded
}	// namespace ARBS
