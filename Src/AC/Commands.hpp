// =============================================================================
// ARBS Studio - shtresa Archicad : Commands.hpp
// Ruteri i komandave qe vijne nga paleta (JavaScript -> C++), ne formatin JSON:
//   kerkesa:  { "cmd": "<emri>", ...parametrat }
//   pergjigja: { "ok": true|false, "error": "...", ...te dhenat }
// =============================================================================
#pragma once

#include <string>

namespace ARBS {
namespace AC {

class Commands {
public:
	static std::string Handle (const std::string& requestJsonUtf8);
};

}	// namespace AC
}	// namespace ARBS
