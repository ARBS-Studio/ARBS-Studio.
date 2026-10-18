// =============================================================================
// ARBS Studio - Add-On per Archicad 28, 29 dhe me te reja
// Pika hyrese e add-on-it: regjistrimi i menyse "ARBS Studio" dhe i paletes.
// =============================================================================
#include "APIEnvir.h"
#include "ACAPinc.h"

#include "ResourceIds.hpp"
#include "RS.hpp"

#include "AC/ArbsPalette.hpp"

using ARBS::AC::ArbsPalette;

// Artikujt e menyse: cdo artikull eshte burim STR# me vete (ID_ARBS_MENU + k), me nje artikull.
// Artikujt e gjenerimit hapin seksionin e paletes dhe e nisin menjehere gjenerimin nga modeli.
static GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
	const Int32 k = static_cast<Int32> (menuParams->menuItemRef.menuResID) - ID_ARBS_MENU;
	if (k < 0 || k >= ID_ARBS_MENU_COUNT) return NoError;
	if (k == 0) {
		ArbsPalette::Toggle ();
		return NoError;
	}
	struct Action { const char* section; bool autoRun; };
	static const Action actions[ID_ARBS_MENU_COUNT] = {
		{ "", false }, { "template", false }, { "composites", false }, { "techdesc", true }, { "boq", true },
		{ "physics", true }, { "insolation", true }, { "schedules", true }, { "plan", true }, { "profile", false }
	};
	ArbsPalette::CreateInstance ();
	ArbsPalette::GetInstance ().ShowSection (actions[k].section, actions[k].autoRun);
	return NoError;
}

API_AddonType CheckEnvironment (API_EnvirParams* envir)
{
	RSGetIndString (&envir->addOnInfo.name, ID_ADDON_INFO, 1, ACAPI_GetOwnResModule ());
	RSGetIndString (&envir->addOnInfo.description, ID_ADDON_INFO, 2, ACAPI_GetOwnResModule ());
	return APIAddon_Preload;
}

GSErrCode RegisterInterface (void)
{
	// Menyja kryesore "ARBS Studio" (MenuCode_UserDef): nje burim per artikull, qe artikujt
	// te dalin direkt nen menyne (me shume artikuj ne nje burim Archicad krijon nenmeny).
	for (short k = 0; k < ID_ARBS_MENU_COUNT; ++k) {
		const GSFlags flags = (k == 0 || k == ID_ARBS_MENU_COUNT - 2) ? MenuFlag_SeparatorAfter : MenuFlag_Default;
		const GSErrCode err = ACAPI_MenuItem_RegisterMenu (static_cast<short> (ID_ARBS_MENU + k), static_cast<short> (ID_ARBS_MENU_PROMPT + k), MenuCode_UserDef, flags);
		if (err != NoError) return err;
	}
	return NoError;
}

GSErrCode Initialize (void)
{
	GSErrCode err = NoError;
	for (short k = 0; k < ID_ARBS_MENU_COUNT; ++k) {
		err = ACAPI_MenuItem_InstallMenuHandler (static_cast<short> (ID_ARBS_MENU + k), MenuCommandHandler);
		if (err != NoError) return err;
	}

	err = ACAPI_Notification_CatchSelectionChange (ArbsPalette::SelectionChangeHandler);
	if (err != NoError) return err;

	return ArbsPalette::RegisterPaletteControlCallBack ();
}

GSErrCode FreeData (void)
{
	return NoError;
}
