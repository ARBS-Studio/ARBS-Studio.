// =============================================================================
// ARBS Studio - shtresa Archicad : LicenseStore.hpp
// Ruajtja e licences se abonimit dhe e dates se nisjes se pare (prova 14 dite):
//  - skedari i gjendjes ne profilin e perdoruesit (i perbashket per Archicad 28/29/30)
//      Windows: %USERPROFILE%\AppData\Local\ARBS Studio\arbs_state.json
//      macOS:   ~/Library/Application Support/ARBS Studio/arbs_state.json
//  - kopje ne preferencat e Archicad-it (celesi "_arbs_lic" i profilit te zyres)
// Merret data me e hershme e nisjes dhe data me e vone e pare (mbrojtje nga kthimi i ores).
// =============================================================================
#pragma once

#include "AC/ArbsUtils.hpp"
#include "Core/License.hpp"

namespace ARBS {
namespace AC {

class LicenseStore {
public:
	static LicenseStatus Current ();					// gjendja per sot (dhe perditeson daten e pare/te fundit)
	static LicenseStatus Activate (const std::string& key, std::string* error);	// ruan celesin nese eshte i vlefshem
	static Json::Value   ToJson (const LicenseStatus& st);
	static const char*   OfficeKey () { return "_arbs_lic"; }
};

}	// namespace AC
}	// namespace ARBS
