// =============================================================================
// ARBS Studio - shtresa Archicad : ModelReader.cpp
// =============================================================================
#include "AC/ModelReader.hpp"

#include "AC/CompositeBuilder.hpp"
#include "Core/Structure.hpp"

#include <algorithm>
#include <cctype>
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

// qendra e elementit ne plan (per caktimin ne bllok)
static bool Center (const API_Elem_Head& head, double* x, double* y)
{
	API_Box3D box {};
	if (ACAPI_Element_CalcBounds (&head, &box) != NoError) return false;
	*x = (box.xMin + box.xMax) / 2.0;
	*y = (box.yMin + box.yMax) / 2.0;
	return true;
}

// konturi i jashtem i poligonit te zones
static Json::Value ZonePolygon (const API_Guid& guid)
{
	std::vector<Pt> poly;
	API_ElementMemo memo {};
	if (ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_Polygon) == NoError && memo.coords != nullptr) {
		const Int32 nCoords = static_cast<Int32> (BMGetHandleSize (reinterpret_cast<GSHandle> (memo.coords)) / static_cast<GSSize> (sizeof (API_Coord))) - 1;
		Int32 last = nCoords;
		if (memo.pends != nullptr && BMGetHandleSize (reinterpret_cast<GSHandle> (memo.pends)) >= static_cast<GSSize> (2 * sizeof (Int32)))
			last = std::min (last, (*memo.pends)[1]);
		for (Int32 i = 1; i <= last; ++i) poly.push_back ({ (*memo.coords)[i].x, (*memo.coords)[i].y });
	}
	ACAPI_DisposeElemMemoHdls (&memo);
	return Geo::ToJson (poly);
}

static std::string ZoneCategoryName (const API_AttributeIndex& idx, std::map<Int32, std::string>& cache)
{
	const Int32 key = idx.ToInt32_Deprecated ();
	auto it = cache.find (key);
	if (it != cache.end ()) return it->second;
	API_Attribute a {};
	GS::UniString n;
	a.header.typeID = API_ZoneCatID;
	a.header.index = idx;
	a.header.uniStringNamePtr = &n;
	const std::string name = (ACAPI_Attribute_Get (&a) == NoError) ? ToUtf8 (n) : std::string ();
	cache[key] = name;
	return name;
}

// kodi i bllokut/hyrjes: numri i zones, ose pjesa e emrit pas fjales kyce ("Blloku B1" -> "B1")
static std::string ZoneCode (const std::string& number, const std::string& name)
{
	if (!number.empty ()) return number;
	const size_t sp = name.find (' ');
	return sp == std::string::npos ? name : name.substr (sp + 1);
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

	// --- zonat: dhomat, blloqet, hyrjet dhe njesite (sipas kategorise ose emrit)
	Json::Value zones = Json::Value::MakeArray ();
	Json::Value blocks = Json::Value::MakeArray ();
	Json::Value entrances = Json::Value::MakeArray ();
	Json::Value unitZones = Json::Value::MakeArray ();
	std::map<Int32, std::string> catCache;
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
		const std::string number = ToUtf8 (el.zone.roomNoStr);
		const std::string name = ToUtf8 (el.zone.roomName);
		const std::string role = Structure::ZoneRole (name, ZoneCategoryName (el.zone.catInd, catCache));
		Json::Value z = Json::Value::MakeObject ();
		z.Set ("story", static_cast<int> (el.header.floorInd));
		z.Set ("number", number);
		z.Set ("name", name);
		z.Set ("area", std::round (area * 100.0) / 100.0);
		z.Set ("x", el.zone.pos.x);
		z.Set ("y", el.zone.pos.y);
		if (role == "room") { zones.Push (z); continue; }
		z.Set ("poly", ZonePolygon (g));
		if (role == "unit") { unitZones.Push (z); continue; }
		z.Set ("code", ZoneCode (number, name));
		(role == "block" ? blocks : entrances).Push (z);
	}
	model.Set ("zones", zones);
	model.Set ("blocks", blocks);
	model.Set ("entrances", entrances);
	model.Set ("unitZones", unitZones);

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
			double cx = 0.0, cy = 0.0;
			if (Center (el.header, &cx, &cy)) { o.Set ("x", cx); o.Set ("y", cy); }
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
			o.Set ("x", el.object.pos.x);
			o.Set ("y", el.object.pos.y);
			objects.Push (o);
		}
	}
	model.Set ("objects", objects);

	// --- kompozitat e perdorura (siperfaqet) dhe elementet per paramasen
	const std::map<Int32, std::string> compNames = CompositeNames ();
	std::map<std::string, double> areaByCode;
	std::map<std::string, std::string> nameByCode;
	std::map<std::string, double> unknownByName;
	std::map<Int32, bool> concreteCache;
	std::map<std::string, double> elems;
	// ndarja sipas bllokut: celesi "<blloku>|<0/1 nentoke>"
	std::map<std::string, std::map<std::string, double>> compParts, elemParts;
	std::string curPart;
	auto addComposite = [&] (const API_AttributeIndex& idx, double area) {
		auto it = compNames.find (idx.ToInt32_Deprecated ());
		if (it == compNames.end ()) return;
		const std::string code = CompositeBuilder::CodeFromName (it->second);
		if (code.empty ()) { unknownByName[it->second] += area; return; }
		areaByCode[code] += area;
		nameByCode[code] = it->second;
		compParts[curPart][code] += area;
	};
	auto addElem = [&] (const char* key, double v) {
		elems[key] += v;
		elemParts[curPart][key] += v;
	};
	auto isConcrete = [&] (const API_AttributeIndex& idx) -> bool {
		const Int32 key = idx.ToInt32_Deprecated ();
		auto it = concreteCache.find (key);
		if (it != concreteCache.end ()) return it->second;
		API_Attribute a {};
		GS::UniString n;
		a.header.typeID = API_BuildingMaterialID;
		a.header.index = idx;
		a.header.uniStringNamePtr = &n;
		bool conc = false;
		if (ACAPI_Attribute_Get (&a) == NoError) {
			std::string low = ToUtf8 (n);
			for (char& c : low) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
			conc = low.find ("concrete") != std::string::npos || low.find ("beton") != std::string::npos || low.find ("b/a") != std::string::npos;
		}
		concreteCache[key] = conc;
		return conc;
	};
	const API_ElemTypeID structTypes[] = { API_WallID, API_SlabID, API_RoofID, API_ColumnID, API_BeamID };
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
			if (!hasQ) continue;
			double cx = 0.0, cy = 0.0;
			const std::string block = Center (el.header, &cx, &cy) ? Structure::BlockAt (blocks, cx, cy) : std::string ();
			curPart = block + (el.header.floorInd < 0 ? "|1" : "|0");
			if (t == API_WallID) {
				const double area = std::max (q.wall.surface1, q.wall.surface2);
				if (el.wall.modelElemStructureType == API_CompositeStructure) addComposite (el.wall.composite, area);
				else if (el.wall.modelElemStructureType == API_BasicStructure) {
					const char* k = isConcrete (el.wall.buildingMaterial) ? "conc" : "mas";
					addElem ((std::string ("E:wall:") + k + ":v").c_str (), q.wall.volume);
					addElem ((std::string ("E:wall:") + k + ":a").c_str (), area);
				}
			} else if (t == API_SlabID) {
				if (el.slab.modelElemStructureType == API_CompositeStructure) addComposite (el.slab.composite, q.slab.topSurface);
				else if (el.slab.modelElemStructureType == API_BasicStructure) {
					addElem ("E:slab:conc:v", q.slab.volume);
					addElem ("E:slab:conc:a", q.slab.topSurface);
				}
			} else if (t == API_RoofID) {
				if (el.roof.shellBase.modelElemStructureType == API_CompositeStructure) addComposite (el.roof.shellBase.composite, q.roof.topSurface);
				else addElem ("E:roof:a", q.roof.topSurface);
				addElem ("E:roof:eaves:l", q.roof.eavesLength);
				addElem ("E:roof:ridge:l", q.roof.ridgesLength + q.roof.hipsLength);
			} else if (t == API_ColumnID) {
				addElem ("E:column:v", q.column.coreVolume + q.column.veneVolume);
			} else if (t == API_BeamID) {
				addElem ("E:beam:v", q.beam.volume);
			}
		}
	}
	for (const API_Guid& g : Elements (API_StairID, false, selection)) {
		API_Element el {};
		el.header.guid = g;
		if (ACAPI_Element_Get (&el) != NoError) continue;
		double cx = 0.0, cy = 0.0;
		curPart = (Center (el.header, &cx, &cy) ? Structure::BlockAt (blocks, cx, cy) : std::string ()) + (el.header.floorInd < 0 ? "|1" : "|0");
		addElem ("E:stair:n", 1.0);
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
	Json::Value unknown = Json::Value::MakeArray ();
	for (const auto& kv : unknownByName) {
		if (kv.second < 0.05) continue;
		Json::Value u = Json::Value::MakeObject ();
		u.Set ("name", kv.first);
		u.Set ("area", std::round (kv.second * 10.0) / 10.0);
		unknown.Push (u);
	}
	model.Set ("unknownComposites", unknown);
	Json::Value elements = Json::Value::MakeObject ();
	for (const auto& kv : elems) elements.Set (kv.first, std::round (kv.second * 1000.0) / 1000.0);
	model.Set ("elements", elements);

	Json::Value cparts = Json::Value::MakeArray ();
	for (const auto& kv : compParts) {
		const size_t bar = kv.first.rfind ('|');
		for (const auto& ca : kv.second) {
			Json::Value c = Json::Value::MakeObject ();
			c.Set ("code", ca.first);
			c.Set ("area", std::round (ca.second * 100.0) / 100.0);
			c.Set ("block", kv.first.substr (0, bar));
			c.Set ("below", kv.first.substr (bar + 1) == "1");
			cparts.Push (c);
		}
	}
	model.Set ("compositeParts", cparts);
	Json::Value eparts = Json::Value::MakeArray ();
	for (const auto& kv : elemParts) {
		const size_t bar = kv.first.rfind ('|');
		Json::Value e = Json::Value::MakeObject ();
		e.Set ("block", kv.first.substr (0, bar));
		e.Set ("below", kv.first.substr (bar + 1) == "1");
		Json::Value vals = Json::Value::MakeObject ();
		for (const auto& v : kv.second) vals.Set (v.first, std::round (v.second * 1000.0) / 1000.0);
		e.Set ("values", vals);
		eparts.Push (e);
	}
	model.Set ("elementParts", eparts);
	return model;
}

}	// namespace AC
}	// namespace ARBS
