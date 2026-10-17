// =============================================================================
// ARBS Studio - shtresa Archicad : ModelReader.cpp
// =============================================================================
#include "AC/ModelReader.hpp"

#include "AC/CompositeBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ARBS {
namespace AC {

Json::Value ModelReader::Stories ()
{
	Json::Value arr = Json::Value::MakeArray ();
	API_StoryInfo si {};
	if (ACAPI_ProjectSetting_GetStorySettings (&si) != NoError || si.data == nullptr) return arr;
	for (Int32 i = 0; i <= si.lastStory - si.firstStory; ++i) {
		const API_StoryType& st = (*si.data)[i];
		Json::Value o = Json::Value::MakeObject ();
		o.Set ("index", static_cast<int> (st.index));
		o.Set ("name", ToUtf8 (st.uName));
		o.Set ("level", st.level);
		arr.Push (o);
	}
	BMKillHandle (reinterpret_cast<GSHandle*> (&si.data));
	return arr;
}

double ModelReader::NorthDeg ()
{
	API_PlaceInfo pi {};
	if (ACAPI_GeoLocation_GetPlaceSets (&pi) != NoError) return 0.0;
	return pi.north * 180.0 / 3.14159265358979323846;
}

static std::string LibPartName (Int32 libInd)
{
	if (libInd <= 0) return "";
	API_LibPart lp {};
	lp.index = libInd;
	if (ACAPI_LibraryPart_Get (&lp) != NoError) return "";
	const std::string name = ToUtf8 (lp.docu_UName);
	if (lp.location != nullptr) { delete lp.location; lp.location = nullptr; }
	return name;
}

static std::string ElementId (const API_Guid& guid)
{
	GS::UniString s;
	if (ACAPI_Element_GetElementInfoString (&guid, &s) != NoError) return "";
	return ToUtf8 (s);
}

static GS::Array<API_Guid> Elements (API_ElemTypeID type, bool selectionOnly, const std::set<API_Guid>& selection)
{
	GS::Array<API_Guid> list;
	if (ACAPI_Element_GetElemList (API_ElemType (type), &list) != NoError) return list;
	if (!selectionOnly) return list;
	GS::Array<API_Guid> filtered;
	for (const API_Guid& g : list) if (selection.count (g) > 0) filtered.Push (g);
	return filtered;
}

static std::map<Int32, std::string> CompositeNames ()
{
	std::map<Int32, std::string> names;
	GS::Array<API_Attribute> comps;
	ACAPI_Attribute_GetAttributesByType (API_CompWallID, comps);
	for (const API_Attribute& a : comps) {
		API_Attribute full {};
		GS::UniString n;
		full.header.typeID = API_CompWallID;
		full.header.index = a.header.index;
		full.header.uniStringNamePtr = &n;
		if (ACAPI_Attribute_Get (&full) == NoError)
			names[a.header.index.ToInt32_Deprecated ()] = ToUtf8 (n);
	}
	return names;
}

Json::Value ModelReader::Read (bool selectionOnly)
{
	Json::Value model = Json::Value::MakeObject ();
	model.Set ("stories", Stories ());
	model.Set ("north", NorthDeg ());

	std::set<API_Guid> selection;
	if (selectionOnly) {
		API_SelectionInfo selInfo {};
		GS::Array<API_Neig> neigs;
		ACAPI_Selection_Get (&selInfo, &neigs, false, false);
		BMKillHandle (reinterpret_cast<GSHandle*> (&selInfo.marquee.coords));
		for (const API_Neig& n : neigs) selection.insert (n.guid);
	}

	// --- zonat
	Json::Value zones = Json::Value::MakeArray ();
	for (const API_Guid& g : Elements (API_ZoneID, false, selection)) {
		API_Element el {};
		el.header.guid = g;
		if (ACAPI_Element_Get (&el) != NoError) continue;
		API_ElementQuantity q {};
		API_Quantities quantities;
		quantities.elements = &q;
		API_QuantitiesMask mask;
		ACAPI_ELEMENT_QUANTITY_MASK_SETFULL (mask);
		double area = 0.0;
		if (ACAPI_Element_GetQuantities (g, nullptr, &quantities, &mask) == NoError) area = q.zone.area;
		Json::Value z = Json::Value::MakeObject ();
		z.Set ("story", static_cast<int> (el.header.floorInd));
		z.Set ("number", ToUtf8 (el.zone.roomNoStr));
		z.Set ("name", ToUtf8 (el.zone.roomName));
		z.Set ("area", std::round (area * 100.0) / 100.0);
		zones.Push (z);
	}
	model.Set ("zones", zones);

	// --- dyert dhe dritaret
	Json::Value openings = Json::Value::MakeArray ();
	const std::pair<API_ElemTypeID, const char*> openingTypes[] = { { API_DoorID, "door" }, { API_WindowID, "window" } };
	for (const auto& ot : openingTypes) {
		for (const API_Guid& g : Elements (ot.first, selectionOnly, selection)) {
			API_Element el {};
			el.header.guid = g;
			if (ACAPI_Element_Get (&el) != NoError) continue;
			const API_WindowType& w = (ot.first == API_DoorID) ? el.door : el.window;
			Json::Value o = Json::Value::MakeObject ();
			o.Set ("kind", ot.second);
			o.Set ("id", ElementId (g));
			o.Set ("type", LibPartName (w.openingBase.libInd));
			o.Set ("width", w.openingBase.width);
			o.Set ("height", w.openingBase.height);
			o.Set ("sill", w.lower);
			o.Set ("story", static_cast<int> (el.header.floorInd));
			openings.Push (o);
		}
	}
	model.Set ("openings", openings);

	// --- inventari (objektet dhe ndricimi)
	Json::Value objects = Json::Value::MakeArray ();
	const API_ElemTypeID objectTypes[] = { API_ObjectID, API_LampID };
	std::map<Int32, std::string> libCache;
	for (API_ElemTypeID t : objectTypes) {
		for (const API_Guid& g : Elements (t, selectionOnly, selection)) {
			API_Element el {};
			el.header.guid = g;
			if (ACAPI_Element_Get (&el) != NoError) continue;
			const Int32 libInd = el.object.libInd;
			if (libCache.count (libInd) == 0) libCache[libInd] = LibPartName (libInd);
			Json::Value o = Json::Value::MakeObject ();
			o.Set ("type", libCache[libInd].empty () ? std::string ("Objekt") : libCache[libInd]);
			o.Set ("id", ElementId (g));
			o.Set ("story", static_cast<int> (el.header.floorInd));
			objects.Push (o);
		}
	}
	model.Set ("objects", objects);

	// --- kompozitat e perdorura (siperfaqet) per fiziken e nderteses
	const std::map<Int32, std::string> compNames = CompositeNames ();
	std::map<std::string, double> areaByCode;
	std::map<std::string, std::string> nameByCode;
	auto addComposite = [&] (const API_AttributeIndex& idx, double area) {
		auto it = compNames.find (idx.ToInt32_Deprecated ());
		if (it == compNames.end ()) return;
		const std::string code = CompositeBuilder::CodeFromName (it->second);
		if (code.empty ()) return;
		areaByCode[code] += area;
		nameByCode[code] = it->second;
	};
	const API_ElemTypeID structTypes[] = { API_WallID, API_SlabID, API_RoofID };
	for (API_ElemTypeID t : structTypes) {
		for (const API_Guid& g : Elements (t, false, selection)) {
			API_Element el {};
			el.header.guid = g;
			if (ACAPI_Element_Get (&el) != NoError) continue;
			API_ElementQuantity q {};
			API_Quantities quantities;
			quantities.elements = &q;
			API_QuantitiesMask mask;
			ACAPI_ELEMENT_QUANTITY_MASK_SETFULL (mask);
			const bool hasQ = ACAPI_Element_GetQuantities (g, nullptr, &quantities, &mask) == NoError;
			if (t == API_WallID && el.wall.modelElemStructureType == API_CompositeStructure)
				addComposite (el.wall.composite, hasQ ? std::max (q.wall.surface1, q.wall.surface2) : 0.0);
			else if (t == API_SlabID && el.slab.modelElemStructureType == API_CompositeStructure)
				addComposite (el.slab.composite, hasQ ? q.slab.topSurface : 0.0);
			else if (t == API_RoofID && el.roof.shellBase.modelElemStructureType == API_CompositeStructure)
				addComposite (el.roof.shellBase.composite, hasQ ? q.roof.topSurface : 0.0);
		}
	}
	Json::Value comps = Json::Value::MakeArray ();
	for (const auto& kv : areaByCode) {
		Json::Value c = Json::Value::MakeObject ();
		c.Set ("code", kv.first);
		c.Set ("name", nameByCode[kv.first]);
		c.Set ("area", std::round (kv.second * 10.0) / 10.0);
		comps.Push (c);
	}
	model.Set ("composites", comps);
	return model;
}

}	// namespace AC
}	// namespace ARBS
