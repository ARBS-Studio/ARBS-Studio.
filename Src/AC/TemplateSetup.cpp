// =============================================================================
// ARBS Studio - shtresa Archicad : TemplateSetup.cpp
// =============================================================================
#include "AC/TemplateSetup.hpp"

#include <cstring>
#include <map>
#include <set>

namespace ARBS {
namespace AC {

// -----------------------------------------------------------------------------
// Ndihmese
// -----------------------------------------------------------------------------

GSErrCode TemplateSetup::RunCommand (const char* name, const std::function<GSErrCode ()>& fn)
{
	// funksionet "non-undoable data structure modifier" thirren ne nje komande pa undo
	GSErrCode err = ACAPI_CallCommand (ToUni (name), fn);
	if (err == APIERR_REFUSEDCMD)		// jemi tashme brenda nje komande
		err = fn ();
	return err;
}

bool TemplateSetup::FindAttributeByName (API_AttrTypeID type, const GS::UniString& name, API_AttributeIndex* index)
{
	API_Attr_Head head {};
	head.typeID = type;
	GS::UniString n = name;
	head.uniStringNamePtr = &n;
	if (ACAPI_Attribute_Search (&head) != NoError) return false;
	if (index != nullptr) *index = head.index;
	return true;
}

static void AddError (Json::Value& report, const std::string& msg, GSErrCode err)
{
	report["errors"].Push (Json::Value (msg + " (kodi " + std::to_string (static_cast<long long> (err)) + ")"));
}

static std::string LayerCode (const GS::UniString& name)
{
	const std::string s = ToUtf8 (name);
	const size_t sp = s.find (' ');
	return sp == std::string::npos ? s : s.substr (0, sp);
}

static const Json::Value* FindPhase (const std::string& code)
{
	for (const Json::Value& p : GetCatalog ().Phases ()["phases"].AsArray ())
		if (p.GetString ("code") == code) return &p;
	return nullptr;
}

// -----------------------------------------------------------------------------
// Shtresat
// -----------------------------------------------------------------------------

int TemplateSetup::CreateLayers (Json::Value& report)
{
	int created = 0;
	for (const Json::Value& l : GetCatalog ().Layers ()["layers"].AsArray ()) {
		GS::UniString name = ToUni (l.GetString ("code") + " " + l.GetString ("name"));
		if (FindAttributeByName (API_LayerID, name, nullptr)) {
			report["skipped"]["layers"] = report["skipped"].GetInt ("layers") + 1;
			continue;
		}
		API_Attribute attr {};
		attr.header.typeID = API_LayerID;
		attr.header.uniStringNamePtr = &name;
		attr.layer.conClassId = l.GetInt ("intersection", 1);
		const GSErrCode err = ACAPI_Attribute_Create (&attr, nullptr);
		if (err == NoError) ++created;
		else AddError (report, "Shtresa " + ToUtf8 (name), err);
	}
	return created;
}

int TemplateSetup::CreateCombinations (Json::Value& report)
{
	// harta kodi -> (indeksi, grupi i nderprerjes)
	struct LayerInfo { API_AttributeIndex index; Int32 conClassId; std::string code; };
	std::vector<LayerInfo> all;
	GS::Array<API_Attribute> layers;
	ACAPI_Attribute_GetAttributesByType (API_LayerID, layers);
	for (const API_Attribute& a : layers) {
		API_Attribute full {};
		GS::UniString name;
		full.header.typeID = API_LayerID;
		full.header.index = a.header.index;
		full.header.uniStringNamePtr = &name;
		if (ACAPI_Attribute_Get (&full) != NoError) continue;
		all.push_back ({ a.header.index, full.layer.conClassId, LayerCode (name) });
	}

	int created = 0;
	for (const Json::Value& c : GetCatalog ().Layers ()["combinations"].AsArray ()) {
		std::set<std::string> show, wire;
		bool showAll = false;
		for (const Json::Value& s : c["show"].AsArray ()) { if (s.AsString () == "*") showAll = true; show.insert (s.AsString ()); }
		for (const Json::Value& s : c["wire"].AsArray ()) wire.insert (s.AsString ());

		API_AttributeDef defs {};
		defs.layer_statItems = new GS::HashTable<API_AttributeIndex, API_LayerStat> ();
		for (const LayerInfo& li : all) {
			API_LayerStat st {};
			const bool isAppLayer = li.index == APIApplicationLayerAttributeIndex;
			const bool visible = showAll || isAppLayer || show.count (li.code) > 0 || wire.count (li.code) > 0;
			int flags = visible ? 0 : APILay_Hidden;
			if (wire.count (li.code) > 0) flags |= APILay_ForceToWire;
			st.lFlags = static_cast<short> (flags);
			st.conClassId = li.conClassId;
			defs.layer_statItems->Add (li.index, st);
		}

		GS::UniString name = ToUni (c.GetString ("name"));
		API_Attribute attr {};
		attr.header.typeID = API_LayerCombID;
		attr.header.uniStringNamePtr = &name;
		attr.layerComb.lNumb = static_cast<Int32> (all.size ());

		API_AttributeIndex existing;
		GSErrCode err;
		if (FindAttributeByName (API_LayerCombID, name, &existing)) {
			attr.header.index = existing;
			err = ACAPI_Attribute_Modify (&attr, &defs);		// rifreskon kombinimin ekzistues
		} else {
			err = ACAPI_Attribute_Create (&attr, &defs);
			if (err == NoError) ++created;
		}
		ACAPI_DisposeAttrDefsHdls (&defs);
		if (err != NoError) AddError (report, "Kombinimi " + c.GetString ("name"), err);
	}
	return created;
}

// -----------------------------------------------------------------------------
// View Map
// -----------------------------------------------------------------------------

static bool GetMapRoot (API_NavigatorMapID mapId, API_Guid* root)
{
	API_NavigatorSet set {};
	set.mapId = mapId;
	if (ACAPI_Navigator_GetNavigatorSet (&set) != NoError) return false;
	*root = set.rootGuid;
	return true;
}

static bool FindChild (API_NavigatorMapID mapId, const API_Guid& parent, const GS::UniString& name, API_NavigatorItemTypeID type, API_Guid* found)
{
	API_NavigatorItem item {};
	item.guid = parent;
	item.mapId = mapId;
	GS::Array<API_NavigatorItem> children;
	if (ACAPI_Navigator_GetNavigatorChildrenItems (&item, &children) != NoError) return false;
	for (const API_NavigatorItem& ch : children) {
		if (ch.itemType != type) continue;
		if (GS::UniString (ch.uName) == name) { *found = ch.guid; return true; }
	}
	return false;
}

static GSErrCode EnsureFolder (const API_Guid& parent, const GS::UniString& name, API_Guid* folder, int* createdCount)
{
	if (FindChild (API_PublicViewMap, parent, name, API_FolderNavItem, folder)) return NoError;
	API_NavigatorItem item {};
	item.mapId = API_PublicViewMap;
	item.itemType = API_FolderNavItem;
	item.customName = true;
	CopyToUChar (name, item.uName, API_UniLongNameLen);
	const GS::Guid parentGs = APIGuid2GSGuid (parent);
	const GSErrCode err = ACAPI_Navigator_NewNavigatorView (&item, nullptr, &parentGs, nullptr);
	if (err == NoError) { *folder = item.guid; if (createdCount) ++*createdCount; }
	return err;
}

static void ConfigureView (const API_Guid& viewGuid, const std::string& combination, Int32 scale, const GS::UniString& customName, Json::Value& report)
{
	API_NavigatorItem item {};
	if (ACAPI_Navigator_GetNavigatorItem (&viewGuid, &item) != NoError) return;
	if (!customName.IsEmpty ()) {
		item.customName = true;
		CopyToUChar (customName, item.uName, API_UniLongNameLen);
	}
	API_NavigatorView view {};
	if (ACAPI_Navigator_GetNavigatorView (&item, &view) != NoError) return;
	view.saveLaySet = true;
	view.saveDScale = true;
	view.drawingScale = scale;
	std::memset (view.layerCombination, 0, sizeof (view.layerCombination));
	std::strncpy (view.layerCombination, combination.c_str (), sizeof (view.layerCombination) - 1);	// UTF-8
	if (view.layerStats != nullptr) { delete view.layerStats; view.layerStats = nullptr; }
	GSErrCode err = ACAPI_Navigator_ChangeNavigatorView (&item, &view);
	if (err != NoError)
		AddError (report, "Pamja " + ToUtf8 (customName), err);
	// lirimi i memories sipas shembullit Navigator_Test te DevKit-it
	if (view.modelViewOpt != nullptr) {
		ACAPI_FreeGDLModelViewOptionsPtr (&view.modelViewOpt->gdlOptions);
		BMKillPtr (reinterpret_cast<GSPtr*> (&view.modelViewOpt));
	}
	BMKillPtr (reinterpret_cast<GSPtr*> (&view.pens));
}

static Int32 PhaseScale (const std::string& phase, bool section)
{
	if (phase == "PK") return 200;
	if (phase == "PZ") return 50;
	if (phase == "PKR") return section ? 50 : 100;
	return 100;
}

void TemplateSetup::CreateViewMap (const SetupOptions& opt, Json::Value& report)
{
	API_Guid root;
	if (!GetMapRoot (API_PublicViewMap, &root)) { report["errors"].Push (Json::Value ("View Map nuk u gjet")); return; }

	// burimet nga Project Map
	auto search = [] (API_NavigatorItemTypeID type) {
		API_NavigatorItem item {};
		item.mapId = API_ProjectMap;
		item.itemType = type;
		GS::Array<API_NavigatorItem> items;
		ACAPI_Navigator_SearchNavigatorItem (&item, &items);
		return items;
	};
	const GS::Array<API_NavigatorItem> stories = search (API_StoryNavItem);
	const GS::Array<API_NavigatorItem> sections = search (API_SectionNavItem);
	const GS::Array<API_NavigatorItem> elevations = search (API_ElevationNavItem);

	int folders = 0, views = 0;
	for (const std::string& code : opt.phases) {
		const Json::Value* phase = FindPhase (code);
		if (phase == nullptr) continue;
		const std::string phaseName = code + " - " + phase->GetString ("name");
		const std::string combo = phase->GetString ("combination");
		API_Guid phaseFolder;
		if (EnsureFolder (root, ToUni (phaseName), &phaseFolder, &folders) != NoError) { AddError (report, "Dosja " + phaseName, APIERR_GENERAL); continue; }

		std::map<std::string, API_Guid> sub;
		for (const Json::Value& f : GetCatalog ().Phases ()["viewMap"]["folders"].AsArray ()) {
			API_Guid g;
			if (EnsureFolder (phaseFolder, ToUni (f.AsString ()), &g, &folders) == NoError) sub[f.AsString ()] = g;
		}

		auto cloneAll = [&] (const GS::Array<API_NavigatorItem>& src, const std::string& folderName, const char* kind, bool section) {
			auto it = sub.find (folderName);
			if (it == sub.end ()) return;
			for (const API_NavigatorItem& s : src) {
				API_Guid created = APINULLGuid;
				if (ACAPI_Navigator_CloneProjectMapItemToViewMap (&s.guid, &it->second, &created) != NoError || created == APINULLGuid) continue;
				const Int32 scale = PhaseScale (code, section);
				const std::string title = code + " | " + kind + " | " + ToUtf8 (s.uName) + " | 1:" + std::to_string (static_cast<long long> (scale));
				ConfigureView (created, combo, scale, ToUni (title), report);
				++views;
			}
		};
		cloneAll (stories, "Bazat", "Baza", false);
		cloneAll (sections, "Prerjet", "Prerja", true);
		cloneAll (elevations, "Pamjet", "Pamja", true);
	}
	report["created"].Set ("folders", folders);
	report["created"].Set ("views", views);
}

// -----------------------------------------------------------------------------
// Layout Book
// -----------------------------------------------------------------------------

static bool EnsureMaster (const std::string& size, API_DatabaseUnId* master, int* created)
{
	struct Paper { const char* name; double w, h; };
	static const Paper papers[] = { { "A4", 297.0, 210.0 }, { "A3", 420.0, 297.0 }, { "A2", 594.0, 420.0 }, { "A1", 841.0, 594.0 }, { "A0", 1189.0, 841.0 } };
	const Paper* p = &papers[1];
	for (const Paper& x : papers) if (size == x.name) p = &x;
	const GS::UniString name = ToUni (std::string ("ARBS ") + p->name + " horizontale");

	// kerko masterin ekzistues
	GS::Array<API_DatabaseUnId> masters;
	ACAPI_Database_GetMasterLayoutDatabases (nullptr, &masters);
	for (const API_DatabaseUnId& id : masters) {
		API_DatabaseInfo info {};
		info.databaseUnId = id;
		if (ACAPI_Window_GetDatabaseInfo (&info) == NoError && GS::UniString (info.name) == name) { *master = id; return true; }
	}

	API_DatabaseInfo info {};
	info.typeID = APIWind_MasterLayoutID;
	CopyToUChar (name, info.name, API_UniLongNameLen);
	if (ACAPI_Database_NewDatabase (&info) != NoError) return false;
	API_LayoutInfo li {};
	CopyToUChar (name, li.layoutName, API_UniLongNameLen);
	li.sizeX = p->w;
	li.sizeY = p->h;
	li.leftMargin = 20.0;	// per lidhje
	li.topMargin = 10.0;
	li.rightMargin = 10.0;
	li.bottomMargin = 10.0;
	ACAPI_Navigator_ChangeLayoutSets (&li, &info.databaseUnId);
	*master = info.databaseUnId;
	if (created) ++*created;
	return true;
}

static bool EnsureSubset (const API_Guid& parent, const std::string& name, const std::string& prefix, bool useUpper, API_Guid* result, int* created)
{
	const GS::UniString uname = ToUni (name);
	if (FindChild (API_LayoutMap, parent, uname, API_SubSetNavItem, result)) return true;
	API_SubSet ss {};
	ACAPI_Navigator_GetSubSetDefault (&ss);
	CopyToUChar (uname, ss.name, API_LongNameLen);
	CopyToUChar (ToUni (prefix), ss.ownPrefix, API_LongNameLen);
	ss.addOwnPrefix = true;
	ss.useUpperPrefix = useUpper;
	ss.numberingStyle = API_NS_01;
	ss.startAt = 1;
	if (ACAPI_Navigator_CreateSubSet (&ss, &parent) != NoError) return false;
	if (created) ++*created;
	return FindChild (API_LayoutMap, parent, uname, API_SubSetNavItem, result);
}

void TemplateSetup::CreateLayoutBook (const SetupOptions& opt, Json::Value& report)
{
	API_Guid root;
	if (!GetMapRoot (API_LayoutMap, &root)) { report["errors"].Push (Json::Value ("Layout Book nuk u gjet")); return; }

	// katet per fletat "perStorey"
	std::vector<std::string> storyNames;
	API_StoryInfo si {};
	if (ACAPI_ProjectSetting_GetStorySettings (&si) == NoError && si.data != nullptr) {
		for (Int32 i = 0; i <= si.lastStory - si.firstStory; ++i)
			storyNames.push_back (ToUtf8 ((*si.data)[i].uName));
		BMKillHandle (reinterpret_cast<GSHandle*> (&si.data));
	}
	if (storyNames.empty ()) storyNames.push_back ("P\xC3\xABrdhesa");

	std::map<std::string, std::string> disciplineNames;
	for (const Json::Value& d : GetCatalog ().Phases ()["disciplines"].AsArray ())
		disciplineNames[d.GetString ("code")] = d.GetString ("name");

	int subsets = 0, layouts = 0, masters = 0;
	for (const std::string& code : opt.phases) {
		const Json::Value* phase = FindPhase (code);
		if (phase == nullptr) continue;
		API_Guid phaseSubset;
		const std::string phaseName = code + " - " + phase->GetString ("name");
		if (!EnsureSubset (root, phaseName, code + "-", false, &phaseSubset, &subsets)) { AddError (report, "Subset " + phaseName, APIERR_GENERAL); continue; }

		std::map<std::string, API_Guid> discSubsets;
		for (const Json::Value& sheet : (*phase)["sheets"].AsArray ()) {
			const std::string d = sheet.GetString ("d", "A");
			if (discSubsets.count (d) == 0) {
				API_Guid g;
				if (!EnsureSubset (phaseSubset, d + " - " + disciplineNames[d], d, true, &g, &subsets)) continue;
				discSubsets[d] = g;
			}
			API_DatabaseUnId master {};
			if (!EnsureMaster (sheet.GetString ("size", "A3"), &master, &masters)) continue;

			std::vector<std::string> names;
			if (sheet.GetBool ("perStorey") && opt.perStoreySheets) {
				for (const std::string& st : storyNames) names.push_back (sheet.GetString ("name") + " \xE2\x80\x93 " + st);
			} else {
				names.push_back (sheet.GetString ("name"));
			}
			for (const std::string& n : names) {
				const std::string full = n + (sheet.GetString ("scale").empty () ? "" : " " + sheet.GetString ("scale"));
				API_Guid existing;
				if (FindChild (API_LayoutMap, discSubsets[d], ToUni (full), API_LayoutNavItem, &existing)) continue;
				API_LayoutInfo li {};
				CopyToUChar (ToUni (full), li.layoutName, API_UniLongNameLen);
				li.showMasterBelow = true;
				API_Guid parent = discSubsets[d];
				if (ACAPI_Navigator_CreateLayout (&li, &master, &parent) == NoError) ++layouts;
				else AddError (report, "Fleta " + full, APIERR_GENERAL);
			}
		}
	}
	report["created"].Set ("subsets", subsets);
	report["created"].Set ("layouts", layouts);
	report["created"].Set ("masters", masters);
}

// -----------------------------------------------------------------------------

Json::Value TemplateSetup::Run (const SetupOptions& opt)
{
	Json::Value report = Json::Value::MakeObject ();
	report.Set ("created", Json::Value::MakeObject ());
	report.Set ("skipped", Json::Value::MakeObject ());
	report.Set ("errors", Json::Value::MakeArray ());

	if (opt.layers || opt.combinations) {
		RunCommand ("ARBS: Shtresat", [&] () -> GSErrCode {
			if (opt.layers) report["created"].Set ("layers", CreateLayers (report));
			if (opt.combinations) report["created"].Set ("combinations", CreateCombinations (report));
			return NoError;
		});
	}
	if (opt.viewMap)
		RunCommand ("ARBS: View Map", [&] () -> GSErrCode { CreateViewMap (opt, report); return NoError; });
	if (opt.layoutBook)
		RunCommand ("ARBS: Layout Book", [&] () -> GSErrCode { CreateLayoutBook (opt, report); return NoError; });
	return report;
}

}	// namespace AC
}	// namespace ARBS
