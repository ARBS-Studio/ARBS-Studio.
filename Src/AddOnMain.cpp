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

static GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
	if (menuParams->menuItemRef.menuResID != ID_ARBS_MENU) return NoError;

	switch (menuParams->menuItemRef.itemIndex) {
		case ID_ARBS_MENU_PALETTE:
			ArbsPalette::Toggle ();
			break;
		default: {
			static const char* sections[] = { "", "", "template", "composites", "techdesc", "physics", "insolation", "schedules", "plan", "profile" };
			const Int32 idx = menuParams->menuItemRef.itemIndex;
			ArbsPalette::CreateInstance ();
			if (idx > 1 && idx < static_cast<Int32> (sizeof (sections) / sizeof (sections[0])))
				ArbsPalette::GetInstance ().ShowSection (sections[idx]);
			else
				ArbsPalette::GetInstance ().Show ();
			break;
		}
	}
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
	// Menyja kryesore "ARBS Studio" (MenuCode_UserDef: stringu i pare eshte titulli i menyse)
	return ACAPI_MenuItem_RegisterMenu (ID_ARBS_MENU, ID_ARBS_MENU_PROMPT, MenuCode_UserDef, MenuFlag_Default);
}

GSErrCode Initialize (void)
{
	GSErrCode err = ACAPI_MenuItem_InstallMenuHandler (ID_ARBS_MENU, MenuCommandHandler);
	if (err != NoError) return err;

	err = ACAPI_Notification_CatchSelectionChange (ArbsPalette::SelectionChangeHandler);
	if (err != NoError) return err;

	return ArbsPalette::RegisterPaletteControlCallBack ();
}

GSErrCode FreeData (void)
{
	return NoError;
}
