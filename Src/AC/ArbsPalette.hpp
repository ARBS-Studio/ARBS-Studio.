// =============================================================================
// ARBS Studio - shtresa Archicad : ArbsPalette.hpp
// Paleta (toolbar) e ARBS Studio: nje DG::Palette me kontroll Browser (HTML/JS),
// e lidhur me C++ permes objektit JavaScript "ARBS" (ARBS.Call(json) -> Promise).
// =============================================================================
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "DGModule.hpp"
#include "DGBrowser.hpp"

#include "ResourceIds.hpp"

namespace ARBS {
namespace AC {

class ArbsPalette final : public DG::Palette,
						  public DG::PanelObserver
{
public:
	static bool         HasInstance ();
	static void         CreateInstance ();
	static ArbsPalette& GetInstance ();
	static void         DestroyInstance ();
	static void         Toggle ();
	static GSErrCode    RegisterPaletteControlCallBack ();
	static GSErrCode    SelectionChangeHandler (const API_Neig*);

	void Show ();
	void Hide ();
	void ShowSection (const char* section, bool autoRun = false);		// hap direkt nje seksion (nga menyja)

	virtual ~ArbsPalette ();

private:
	enum { BrowserId = 1 };

	ArbsPalette ();
	void InitBrowserControl ();
	void SetMenuItemCheckedState (bool isChecked);

	virtual void PanelResized (const DG::PanelResizeEvent& ev) override;
	virtual void PanelCloseRequested (const DG::PanelCloseRequestEvent& ev, bool* accepted) override;

	static GSErrCode PaletteControlCallBack (Int32 paletteId, API_PaletteMessageID messageID, GS::IntPtr param);

	DG::Browser browser;
	static GS::Ref<ArbsPalette> instance;
};

}	// namespace AC
}	// namespace ARBS
