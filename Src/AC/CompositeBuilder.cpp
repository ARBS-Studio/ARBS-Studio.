// =============================================================================
// ARBS Studio - shtresa Archicad : CompositeBuilder.cpp
// =============================================================================
#include "AC/CompositeBuilder.hpp"

#include "AC/TemplateSetup.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace ARBS {
namespace AC {

std::string CompositeBuilder::MaterialAttributeName (const Material& m)
{
	return "ARBS \xC2\xB7 " + m.name;		// "ARBS . ..."
}

std::string CompositeBuilder::CompositeAttributeName (const Composite& c)
{
	return c.code + " " + c.name;
}

std::string CompositeBuilder::CodeFromName (const std::string& name)
{
	// formati: dy shkronja te medha, '-', dy shifra (p.sh. "MJ-01")
	if (name.size () < 5) return "";
	if (!std::isupper (static_cast<unsigned char> (name[0])) || !std::isupper (static_cast<unsigned char> (name[1])) || name[2] != '-' ||
		!std::isdigit (static_cast<unsigned char> (name[3])) || !std::isdigit (static_cast<unsigned char> (name[4])))
		return "";
	if (name.size () > 5 && name[5] != ' ') return "";
	return name.substr (0, 5);
}

static std::string Lower (std::string s)
{
	for (char& c : s) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
	return s;
}

API_AttributeIndex CompositeBuilder::FindFill (const std::vector<std::string>& candidates)
{
	// 1) emri i sakte; 2) emri qe permban fjalen; 3) shrafura e pare "solid" e disponueshme
	GS::Array<API_Attribute> fills;
	ACAPI_Attribute_GetAttributesByType (API_FilltypeID, fills);
	std::vector<std::pair<API_AttributeIndex, std::string>> names;
	for (const API_Attribute& a : fills) {
		API_Attribute full {};
		GS::UniString n;
		full.header.typeID = API_FilltypeID;
		full.header.index = a.header.index;
		full.header.uniStringNamePtr = &n;
		if (ACAPI_Attribute_Get (&full) == NoError)
			names.emplace_back (a.header.index, Lower (ToUtf8 (n)));
	}
	for (const std::string& c : candidates)
		for (const auto& n : names)
			if (n.second == Lower (c)) return n.first;
	for (const std::string& c : candidates)
		for (const auto& n : names)
			if (n.second.find (Lower (c)) != std::string::npos) return n.first;
	for (const auto& n : names)
		if (n.second.find ("background") != std::string::npos || n.second.find ("empty") != std::string::npos) return n.first;
	return names.empty () ? ACAPI_CreateAttributeIndex (1) : names.front ().first;
}

API_AttributeIndex CompositeBuilder::FindSurface (const std::string& group)
{
	// siperfaqja (material 3D) sipas grupit, me emra te template-it INT
	static const std::map<std::string, std::vector<std::string>> map = {
		{ "Konstruksion", { "Concrete - Cast-In-Place", "Concrete", "Beton" } },
		{ "Murature",     { "Brick", "Masonry", "Ziegel" } },
		{ "Termoizolim",  { "Insulation", "D\xC3\xA4mmung" } },
		{ "Suva",         { "Paint - White", "Plaster", "Paint" } },
		{ "Fasad\xC3\xAB",       { "Plaster", "Paint - White", "Paint" } },
		{ "Dysheme",      { "Tile", "Wood", "Parquet" } },
		{ "Dru",          { "Wood", "Holz" } },
		{ "Mbules\xC3\xAB",      { "Roof Tile", "Tile" } },
		{ "Dheu",         { "Gravel", "Earth", "Grass" } },
	};
	GS::Array<API_Attribute> mats;
	ACAPI_Attribute_GetAttributesByType (API_MaterialID, mats);
	auto it = map.find (group);
	if (it != map.end ()) {
		for (const std::string& cand : it->second) {
			for (const API_Attribute& a : mats) {
				API_Attribute full {};
				GS::UniString n;
				full.header.typeID = API_MaterialID;
				full.header.index = a.header.index;
				full.header.uniStringNamePtr = &n;
				if (ACAPI_Attribute_Get (&full) == NoError && Lower (ToUtf8 (n)).find (Lower (cand)) != std::string::npos)
					return a.header.index;
			}
		}
	}
	return mats.IsEmpty () ? ACAPI_CreateAttributeIndex (1) : mats[0].header.index;
}

bool CompositeBuilder::EnsureMaterial (const Material& m, bool update, API_AttributeIndex* index, Json::Value& report)
{
	GS::UniString name = ToUni (MaterialAttributeName (m));
	API_AttributeIndex existing;
	const bool exists = TemplateSetup::FindAttributeByName (API_BuildingMaterialID, name, &existing);
	if (exists && !update) { *index = existing; return true; }

	GS::UniString id = ToUni (m.archicadId.empty () ? m.id : m.archicadId);
	GS::UniString manufacturer ("ARBS Studio");
	GS::UniString description = ToUni (m.group + " \xC2\xB7 \xCE\xBB=" + Html::Num (m.lambda, 3) + " W/mK \xC2\xB7 \xCE\xBC=" + Html::Num (m.mu, 0));

	API_Attribute attr {};
	attr.header.typeID = API_BuildingMaterialID;
	attr.header.uniStringNamePtr = &name;
	if (exists) {
		attr.header.index = existing;
		ACAPI_Attribute_Get (&attr);
		attr.header.uniStringNamePtr = &name;
	}
	attr.buildingMaterial.id = &id;
	attr.buildingMaterial.manufacturer = &manufacturer;
	attr.buildingMaterial.description = &description;

	Int32 uiPriority = m.priority;		// prioriteti i nderprerjes si ne UI (0..999)
	Int32 elemPriority = 0;
	ACAPI_Element_UI2ElemPriority (&uiPriority, &elemPriority);
	attr.buildingMaterial.connPriority = elemPriority;
	attr.buildingMaterial.cutFill = FindFill (GetCatalog ().FillCandidates (m.id));
	attr.buildingMaterial.cutFillPen = 2;
	attr.buildingMaterial.cutFillBackgroundPen = 0;
	attr.buildingMaterial.cutMaterial = FindSurface (m.group);
	attr.buildingMaterial.cutFillOrientation = APIFillOrientation_ProjectOrigin;
	attr.buildingMaterial.thermalConductivity = m.lambda;
	attr.buildingMaterial.density = m.density;
	attr.buildingMaterial.heatCapacity = m.heatCapacity;
	attr.buildingMaterial.showUncutLines = true;

	const GSErrCode err = exists ? ACAPI_Attribute_Modify (&attr, nullptr) : ACAPI_Attribute_Create (&attr, nullptr);
	if (err != NoError) {
		report["errors"].Push (Json::Value ("Materiali " + m.name + " (kodi " + std::to_string (static_cast<long long> (err)) + ")"));
		return false;
	}
	*index = attr.header.index;
	report[exists ? "updated" : "materials"] = report.GetInt (exists ? "updated" : "materials") + 1;
	return true;
}

Json::Value CompositeBuilder::Create (const std::vector<std::string>& codes, bool updateExisting)
{
	Json::Value report = Json::Value::MakeObject ();
	report.Set ("materials", 0);
	report.Set ("composites", 0);
	report.Set ("updated", 0);
	report.Set ("errors", Json::Value::MakeArray ());

	const Catalog& cat = GetCatalog ();
	TemplateSetup::RunCommand ("ARBS: Kompozitat", [&] () -> GSErrCode {
		std::map<std::string, API_AttributeIndex> matIndex;
		for (const Composite& c : cat.Composites ()) {
			if (!codes.empty () && std::find (codes.begin (), codes.end (), c.code) == codes.end ()) continue;

			// materialet e kompozites
			bool ok = true;
			for (const Layer& l : c.layers) {
				if (matIndex.count (l.material->id) > 0) continue;
				API_AttributeIndex idx;
				if (!EnsureMaterial (*l.material, updateExisting, &idx, report)) { ok = false; break; }
				matIndex[l.material->id] = idx;
			}
			if (!ok) continue;

			GS::UniString name = ToUni (CompositeAttributeName (c));
			API_AttributeIndex existing;
			const bool exists = TemplateSetup::FindAttributeByName (API_CompWallID, name, &existing);
			if (exists && !updateExisting) continue;

			const short nComps = static_cast<short> (c.layers.size ());
			API_AttributeDefExt defs {};
			defs.cwall_compItems = reinterpret_cast<API_CWallComponent**> (BMAllocateHandle (static_cast<GSSize> (nComps * sizeof (API_CWallComponent)), ALLOCATE_CLEAR, 0));
			defs.cwall_compLItems = reinterpret_cast<API_CWallLineComponent**> (BMAllocateHandle (static_cast<GSSize> ((nComps + 1) * sizeof (API_CWallLineComponent)), ALLOCATE_CLEAR, 0));
			if (defs.cwall_compItems == nullptr || defs.cwall_compLItems == nullptr) {
				ACAPI_DisposeAttrDefsHdlsExt (&defs);
				report["errors"].Push (Json::Value ("Memorie e pamjaftueshme"));
				return APIERR_MEMFULL;
			}
			for (short i = 0; i < nComps; ++i) {
				const Layer& l = c.layers[static_cast<size_t> (i)];
				API_CWallComponent& comp = (*defs.cwall_compItems)[i];
				comp.buildingMaterial = matIndex[l.material->id];
				comp.framePen = 1;
				int bits = 0;
				if (l.core)   bits |= APICWallComp_Core;
				if (l.finish) bits |= APICWallComp_Finish;
				comp.flagBits = static_cast<short> (bits);
				comp.fillThick = l.thickness;
			}
			for (short i = 0; i <= nComps; ++i) {
				API_CWallLineComponent& line = (*defs.cwall_compLItems)[i];
				line.ltypeInd = APISolidLineAttributeIndex;
				line.linePen = static_cast<short> ((i == 0 || i == nComps) ? 1 : 3);	// kontur i trashe, ndarje e holle
			}

			API_Attribute attr {};
			attr.header.typeID = API_CompWallID;
			attr.header.uniStringNamePtr = &name;
			const int usageFlags = (c.usage == "wall") ? APICWall_ForWall : (APICWall_ForSlab | APICWall_ForRoof | APICWall_ForShell);
			attr.header.flags = static_cast<short> (usageFlags);
			attr.compWall.totalThick = c.TotalThickness ();
			attr.compWall.nComps = nComps;

			GSErrCode err;
			if (exists) {
				attr.header.index = existing;
				err = ACAPI_Attribute_ModifyExt (&attr, &defs);
			} else {
				err = ACAPI_Attribute_CreateExt (&attr, &defs);
			}
			ACAPI_DisposeAttrDefsHdlsExt (&defs);
			if (err != NoError) {
				report["errors"].Push (Json::Value ("Kompozita " + c.code + " (kodi " + std::to_string (static_cast<long long> (err)) + ")"));
				continue;
			}
			report[exists ? "updated" : "composites"] = report.GetInt (exists ? "updated" : "composites") + 1;
		}
		return NoError;
	});
	return report;
}

}	// namespace AC
}	// namespace ARBS
