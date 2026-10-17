#ifndef ARBS_RESOURCEIDS_HPP
#define ARBS_RESOURCEIDS_HPP

// -----------------------------------------------------------------------------
// ARBS Studio - identifikuesit e burimeve (resources)
// -----------------------------------------------------------------------------

#define ID_ADDON_INFO			32000
#define ID_ARBS_MENU			32500
#define ID_ARBS_MENU_PROMPT		32510
#define ID_ARBS_PALETTE			32600
#define ID_ARBS_ICON			10001

// Artikujt e menyse "ARBS Studio" (indekset fillojne nga 1)
#define ID_ARBS_MENU_PALETTE		1
#define ID_ARBS_MENU_TEMPLATE		2
#define ID_ARBS_MENU_COMPOSITES		3
#define ID_ARBS_MENU_TECHDESC		4
#define ID_ARBS_MENU_PHYSICS		5
#define ID_ARBS_MENU_INSOLATION		6
#define ID_ARBS_MENU_SCHEDULES		7
#define ID_ARBS_MENU_PLAN			8
#define ID_ARBS_MENU_PROFILE		9

// -----------------------------------------------------------------------------
// MDID - identifikuesi i add-on-it te Graphisoft.
// Per shperndarje (jo vetem ne modalitetin DEMO te Archicad-it) regjistrohuni falas
// ne https://archicadapi.graphisoft.com (Profile > Add-Ons) dhe vendosni ketu
// Developer ID dhe Local ID qe merrni. Vlerat 1/1 punojne vetem ne DEMO.
// -----------------------------------------------------------------------------
#ifndef ARBS_MDID_DEVELOPER
#define ARBS_MDID_DEVELOPER		1
#endif
#ifndef ARBS_MDID_LOCAL
#define ARBS_MDID_LOCAL			1
#endif

#endif
