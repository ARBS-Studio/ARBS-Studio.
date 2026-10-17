// =============================================================================
// ARBS Studio - shtresa Archicad : ProjectStore.hpp
//  - Profili i zyres: ruhet ne preferencat e Archicad-it (vlen per te gjitha projektet)
//  - Te dhenat e projektit dhe plani dinamik: ruhen brenda skedarit .pln (ModulData)
//  - Sinkronizimi me Project Info (autotekstet) te Archicad-it
// =============================================================================
#pragma once

#include "AC/ArbsUtils.hpp"

namespace ARBS {
namespace AC {

class ProjectStore {
public:
	static Json::Value LoadOffice ();
	static bool        SaveOffice (const Json::Value& office);

	static Json::Value LoadProject ();
	static bool        SaveProject (const Json::Value& project);

	static Json::Value LoadPlan ();
	static bool        SavePlan (const Json::Value& plan);

	// Shkruan fushat ne Project Info (autotekste ekzistuese ose te reja "ARBS ...")
	static Json::Value PushToProjectInfo (const Json::Value& project, const Json::Value& office);

	// Office me logon e paracaktuar ARBS nese mungon
	static Json::Value OfficeForReports ();

private:
	static Json::Value LoadModulData (const char* name);
	static bool        SaveModulData (const char* name, const Json::Value& v);
};

}	// namespace AC
}	// namespace ARBS
