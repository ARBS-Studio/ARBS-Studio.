// =============================================================================
// ARBS Studio - shtresa Archicad : ArbsUtils.hpp
// Konvertimet e teksteve, katalogu i ngulitur, ruajtja e skedareve, hapja e tyre.
// =============================================================================
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"

#include "Location.hpp"
#include "UniString.hpp"

#include "Core/Catalog.hpp"
#include "Core/Json.hpp"
#include "Core/Reports.hpp"

#include <string>
#include <vector>

namespace ARBS {
namespace AC {

GS::UniString ToUni (const std::string& utf8);
std::string   ToUtf8 (const GS::UniString& s);
std::string   ToUtf8 (const GS::uchar_t* s);
void          CopyToUChar (const GS::UniString& s, GS::uchar_t* dst, size_t capacity);

// Katalogu i te dhenave (ngarkohet nje here nga skedaret e ngulitur)
const Catalog& GetCatalog ();
std::string    CatalogError ();

std::string LogoDataUri ();						// logoja ARBS si data URI (PNG)
std::string Base64 (const std::string& bytes);

// Dosja e daljes: Dokumentet/ARBS Studio/<NrProjektit - Emri>
IO::Location OutputFolder (const Json::Value& project);
std::string  LocationToUtf8 (const IO::Location& loc);

// Ruan dokumentet ne dosjen e daljes, kthen shtigjet e plota (UTF-8)
std::vector<std::string> SaveOutputs (const Json::Value& project, const std::vector<ReportOutput>& outputs, GSErrCode* err = nullptr);
bool OpenWithSystem (const std::string& utf8Path);

void Report (const std::string& utf8Message);	// Session Report i Archicad-it

}	// namespace AC
}	// namespace ARBS
