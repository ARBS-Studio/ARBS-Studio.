// =============================================================================
// ARBS Studio - shtresa Archicad : ProjectStore.cpp
// =============================================================================
#include "AC/ProjectStore.hpp"

#include "AC/TemplateSetup.hpp"

#include <cctype>
#include <cstring>
#include <vector>

namespace ARBS {
namespace AC {

static const Int32 kPrefsVersion = 1;
static const Int32 kModulVersion = 1;

Json::Value ProjectStore::LoadOffice ()
{
	Int32 version = 0;
	GSSize size = 0;
	if (ACAPI_GetPreferences (&version, &size, nullptr) != NoError || size <= 0 || version != kPrefsVersion)
		return Json::Value::MakeObject ();
	std::vector<char> buf (static_cast<size_t> (size));
	if (ACAPI_GetPreferences (&version, &size, buf.data ()) != NoError)
		return Json::Value::MakeObject ();
	try {
		return Json::Value::Parse (std::string (buf.data (), buf.size ()));
	} catch (const Json::ParseError&) {
		return Json::Value::MakeObject ();
	}
}

bool ProjectStore::SaveOffice (const Json::Value& office)
{
	const std::string s = office.Dump ();
	return ACAPI_SetPreferences (kPrefsVersion, static_cast<GSSize> (s.size ()), s.data ()) == NoError;
}

Json::Value ProjectStore::LoadModulData (const char* name)
{
	API_ModulData md {};
	if (ACAPI_ModulData_Get (&md, ToUni (name)) != NoError || md.dataHdl == nullptr)
		return Json::Value::MakeObject ();
	const GSSize size = BMGetHandleSize (md.dataHdl);
	std::string s (*md.dataHdl, static_cast<size_t> (size));
	BMKillHandle (&md.dataHdl);
	try {
		return Json::Value::Parse (s);
	} catch (const Json::ParseError&) {
		return Json::Value::MakeObject ();
	}
}

bool ProjectStore::SaveModulData (const char* name, const Json::Value& v)
{
	const std::string s = v.Dump ();
	API_ModulData md {};
	md.dataVersion = kModulVersion;
	md.platformSign = GS::Act_Platform_Sign;
	md.dataHdl = BMAllocateHandle (static_cast<GSSize> (s.size ()), ALLOCATE_CLEAR, 0);
	if (md.dataHdl == nullptr) return false;
	std::memcpy (*md.dataHdl, s.data (), s.size ());
	GSErrCode err = TemplateSetup::RunCommand ("ARBS: Ruaj te dhenat", [&] () -> GSErrCode {
		return ACAPI_ModulData_Store (&md, ToUni (name));
	});
	BMKillHandle (&md.dataHdl);
	return err == NoError;
}

Json::Value ProjectStore::LoadProject ()            { return LoadModulData ("ARBS_Project"); }
bool        ProjectStore::SaveProject (const Json::Value& p) { return SaveModulData ("ARBS_Project", p); }
Json::Value ProjectStore::LoadPlan ()               { return LoadModulData ("ARBS_Plan"); }
bool        ProjectStore::SavePlan (const Json::Value& p)    { return SaveModulData ("ARBS_Plan", p); }

Json::Value ProjectStore::OfficeForReports ()
{
	Json::Value office = LoadOffice ();
	if (office.GetString ("kompania").empty ()) office.Set ("kompania", "ARBS Studio");
	if (office.GetString ("logo").empty ()) office.Set ("logo", LogoDataUri ());
	return office;
}

// Archicad 30 e shenon ACAPI_AutoText_CreateAnAutoText si "deprecated"; me /WX paralajmerimi
// behet gabim, prandaj e thirrim me paralajmerimin e fikur (funksioni ekziston ne 28, 29 dhe 30).
#if defined (_MSC_VER)
#pragma warning (push)
#pragma warning (disable: 4996)
#elif defined (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
static GSErrCode CreateProjectInfoField (const API_Guid& guid, const std::string& uiLabel)
{
	return ACAPI_AutoText_CreateAnAutoText (&guid, uiLabel.c_str ());
}
#if defined (_MSC_VER)
#pragma warning (pop)
#elif defined (__clang__)
#pragma clang diagnostic pop
#endif

static std::string Upper (std::string s)
{
	for (char& c : s) c = static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
	return s;
}

Json::Value ProjectStore::PushToProjectInfo (const Json::Value& project, const Json::Value& office)
{
	Json::Value report = Json::Value::MakeObject ();
	int setCount = 0, createdCount = 0;

	GS::Array<GS::ArrayFB<GS::UniString, 3>> autotexts;
	ACAPI_AutoText_GetAutoTexts (&autotexts, APIAutoText_All);

	// [0] pershkrimi, [1] celesi i databazes, [2] vlera
	auto findKey = [&] (const std::vector<std::string>& candidates, const std::string& uiLabel) -> GS::UniString {
		for (const std::string& c : candidates)
			for (const auto& a : autotexts)
				if (Upper (ToUtf8 (a[1])) == Upper (c)) return a[1];
		for (const auto& a : autotexts)
			if (ToUtf8 (a[0]) == uiLabel) return a[1];
		return GS::UniString ();
	};

	TemplateSetup::RunCommand ("ARBS: Project Info", [&] () -> GSErrCode {
		for (const Json::Value& f : GetCatalog ().Documents ()["projectFields"].AsArray ()) {
			const std::string value = project.GetString (f.GetString ("key"));
			if (value.empty ()) continue;
			std::vector<std::string> cands;
			for (const Json::Value& k : f["autotext"].AsArray ()) cands.push_back (k.AsString ());
			const std::string uiLabel = "ARBS " + f.GetString ("label");
			GS::UniString key = findKey (cands, uiLabel);
			if (key.IsEmpty ()) {
				// krijo fushe te re ne Project Info
				API_Guid guid = GSGuid2APIGuid (GS::Guid (GS::Guid::GenerateGuid));
				if (CreateProjectInfoField (guid, uiLabel) == NoError) {
					++createdCount;
					autotexts.Clear ();
					ACAPI_AutoText_GetAutoTexts (&autotexts, APIAutoText_All);
					key = findKey ({}, uiLabel);
				}
			}
			if (key.IsEmpty ()) continue;
			GS::UniString v = ToUni (value);
			if (ACAPI_AutoText_SetAnAutoText (&key, &v) == NoError) ++setCount;
		}
		// zyra
		const std::pair<const char*, const char*> officeKeys[] = { { "COMPANY", "kompania" }, { "COMPANYFULLADDRESS", "adresa" } };
		for (const auto& ok : officeKeys) {
			const std::string value = office.GetString (ok.second);
			if (value.empty ()) continue;
			GS::UniString key = findKey ({ ok.first }, "");
			if (key.IsEmpty ()) continue;
			GS::UniString v = ToUni (value);
			if (ACAPI_AutoText_SetAnAutoText (&key, &v) == NoError) ++setCount;
		}
		return NoError;
	});
	report.Set ("set", setCount);
	report.Set ("created", createdCount);
	return report;
}

}	// namespace AC
}	// namespace ARBS
