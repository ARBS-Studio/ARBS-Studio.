// =============================================================================
// ARBS Studio - shtresa Archicad : ArbsPalette.cpp
// (bazuar ne shembullin Browser_Control te Archicad API DevKit)
// =============================================================================
#include "AC/ArbsPalette.hpp"

#include "AC/ArbsUtils.hpp"
#include "AC/Commands.hpp"
#include "Core/Embedded.hpp"

namespace ARBS {
namespace AC {

GS::Ref<ArbsPalette> ArbsPalette::instance;

static GS::Guid PaletteGuid ()
{
	// Guid fiks i paletes (ruan pozicionin dhe madhesine ndermjet sesioneve)
	static const GS::Guid guid ("6B3A2F1E-5C7D-4E8A-9B21-0A4B53000001");
	return guid;
}

static GSErrCode NotificationHandler (API_NotifyEventID notifID, Int32 /*param*/)
{
	if (notifID == APINotify_Quit)
		ArbsPalette::DestroyInstance ();
	return NoError;
}

static GS::UniString LoadPaletteHtml ()
{
	std::string html;
	if (!Embedded::Get ("arbs_palette.html", html))
		html = "<html><body><h3>ARBS Studio</h3><p>Nderfaqja nuk u gjet ne add-on.</p></body></html>";
	return ToUni (html);
}

ArbsPalette::ArbsPalette () :
	DG::Palette (ACAPI_GetOwnResModule (), ID_ARBS_PALETTE, ACAPI_GetOwnResModule (), PaletteGuid ()),
	browser (GetReference (), BrowserId)
{
	ACAPI_ProjectOperation_CatchProjectEvent (APINotify_Quit, NotificationHandler);
	Attach (*this);
	BeginEventProcessing ();
	InitBrowserControl ();
}

ArbsPalette::~ArbsPalette ()
{
	EndEventProcessing ();
}

bool ArbsPalette::HasInstance ()             { return instance != nullptr; }
ArbsPalette& ArbsPalette::GetInstance ()     { return *instance; }
void ArbsPalette::DestroyInstance ()         { instance = nullptr; }

void ArbsPalette::CreateInstance ()
{
	if (instance != nullptr) return;
	instance = new ArbsPalette ();
	ACAPI_KeepInMemory (true);
}

void ArbsPalette::Toggle ()
{
	if (HasInstance () && GetInstance ().IsVisible ()) {
		GetInstance ().Hide ();
	} else {
		CreateInstance ();
		GetInstance ().Show ();
	}
}

void ArbsPalette::Show ()
{
	DG::Palette::Show ();
	SetMenuItemCheckedState (true);
}

void ArbsPalette::Hide ()
{
	DG::Palette::Hide ();
	SetMenuItemCheckedState (false);
}

void ArbsPalette::ShowSection (const char* section)
{
	Show ();
	browser.ExecuteJS (ToUni (std::string ("window.ARBSUI && ARBSUI.open('") + section + "')"));
}

void ArbsPalette::InitBrowserControl ()
{
	browser.LoadHTML (LoadPaletteHtml ());

	JS::Object* jsArbs = new JS::Object ("ARBS");
	jsArbs->AddItem (new JS::Function ("Call", [] (GS::Ref<JS::Base> param) -> GS::Ref<JS::Base> {
		std::string request;
		GS::Ref<JS::Value> value = GS::DynamicCast<JS::Value> (param);
		if (value != nullptr && value->GetType () == JS::Value::STRING)
			request = ToUtf8 (value->GetString ());
		const std::string response = Commands::Handle (request);
		return new JS::Value (ToUni (response));
	}));
	browser.RegisterAsynchJSObject (jsArbs);
}

void ArbsPalette::SetMenuItemCheckedState (bool isChecked)
{
	API_MenuItemRef itemRef {};
	GSFlags itemFlags {};
	itemRef.menuResID = ID_ARBS_MENU;
	itemRef.itemIndex = ID_ARBS_MENU_PALETTE;
	ACAPI_MenuItem_GetMenuItemFlags (&itemRef, &itemFlags);
	if (isChecked) itemFlags |= API_MenuItemChecked;
	else           itemFlags &= ~API_MenuItemChecked;
	ACAPI_MenuItem_SetMenuItemFlags (&itemRef, &itemFlags);
}

void ArbsPalette::PanelResized (const DG::PanelResizeEvent& ev)
{
	BeginMoveResizeItems ();
	browser.Resize (ev.GetHorizontalChange (), ev.GetVerticalChange ());
	EndMoveResizeItems ();
}

void ArbsPalette::PanelCloseRequested (const DG::PanelCloseRequestEvent&, bool* accepted)
{
	Hide ();
	*accepted = true;
}

GSErrCode ArbsPalette::SelectionChangeHandler (const API_Neig*)
{
	if (HasInstance () && GetInstance ().IsVisible ())
		GetInstance ().browser.ExecuteJS ("window.ARBSUI && ARBSUI.onSelectionChanged && ARBSUI.onSelectionChanged()");
	return NoError;
}

GSErrCode ArbsPalette::PaletteControlCallBack (Int32, API_PaletteMessageID messageID, GS::IntPtr param)
{
	switch (messageID) {
		case APIPalMsg_OpenPalette:
			CreateInstance ();
			GetInstance ().Show ();
			break;
		case APIPalMsg_ClosePalette:
			if (HasInstance ()) GetInstance ().Hide ();
			break;
		case APIPalMsg_HidePalette_Begin:
			if (HasInstance () && GetInstance ().IsVisible ()) GetInstance ().Hide ();
			break;
		case APIPalMsg_HidePalette_End:
			if (HasInstance () && !GetInstance ().IsVisible ()) GetInstance ().Show ();
			break;
		case APIPalMsg_DisableItems_Begin:
			if (HasInstance () && GetInstance ().IsVisible ()) GetInstance ().DisableItems ();
			break;
		case APIPalMsg_DisableItems_End:
			if (HasInstance () && GetInstance ().IsVisible ()) GetInstance ().EnableItems ();
			break;
		case APIPalMsg_IsPaletteVisible:
			*(reinterpret_cast<bool*> (param)) = HasInstance () && GetInstance ().IsVisible ();
			break;
		default:
			break;
	}
	return NoError;
}

GSErrCode ArbsPalette::RegisterPaletteControlCallBack ()
{
	return ACAPI_RegisterModelessWindow (
		GS::CalculateHashValue (PaletteGuid ()),
		PaletteControlCallBack,
		API_PalEnabled_FloorPlan + API_PalEnabled_Section + API_PalEnabled_Elevation +
		API_PalEnabled_InteriorElevation + API_PalEnabled_3D + API_PalEnabled_Detail +
		API_PalEnabled_Worksheet + API_PalEnabled_Layout + API_PalEnabled_DocumentFrom3D,
		GSGuid2APIGuid (PaletteGuid ()));
}

}	// namespace AC
}	// namespace ARBS
