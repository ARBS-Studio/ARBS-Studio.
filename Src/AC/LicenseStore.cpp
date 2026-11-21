// =============================================================================
// ARBS Studio - shtresa Archicad : LicenseStore.cpp
// =============================================================================
#include "AC/LicenseStore.hpp"

#include "AC/ProjectStore.hpp"

#include "File.hpp"
#include "FileSystem.hpp"

#include <ctime>

namespace ARBS {
namespace AC {

namespace {

IO::Location StateFile ()
{
	IO::Location loc;
	if (IO::fileSystem.GetSpecialLocation (IO::FileSystem::UserHome, &loc) != NoError) {
		ACAPI_GetOwnLocation (&loc);
		loc.DeleteLastLocalName ();
	}
#if defined (WINDOWS)
	loc.AppendToLocal (IO::Name ("AppData"));
	loc.AppendToLocal (IO::Name ("Local"));
#else
	loc.AppendToLocal (IO::Name ("Library"));
	loc.AppendToLocal (IO::Name ("Application Support"));
#endif
	loc.AppendToLocal (IO::Name ("ARBS Studio"));
	IO::fileSystem.CreateFolderTree (loc);
	loc.AppendToLocal (IO::Name ("arbs_state.json"));
	return loc;
}

Json::Value ReadState ()
{
	IO::File file (StateFile (), IO::File::Fail);
	if (file.GetStatus () != NoError || file.Open (IO::File::ReadMode) != NoError) return Json::Value::MakeObject ();
	UInt64 size = 0;
	file.GetDataLength (&size);
	std::string s (static_cast<size_t> (size), '\0');
	if (size > 0) file.ReadBin (&s[0], static_cast<USize> (size));
	file.Close ();
	try {
		Json::Value v = Json::Value::Parse (s);
		return v.IsObject () ? v : Json::Value::MakeObject ();
	} catch (...) {
		return Json::Value::MakeObject ();
	}
}

void WriteState (const Json::Value& st)
{
	IO::File file (StateFile (), IO::File::Create);
	if (file.GetStatus () != NoError || file.Open (IO::File::WriteEmptyMode) != NoError) return;
	const std::string s = st.Dump ();
	file.WriteBin (s.data (), static_cast<USize> (s.size ()));
	file.Close ();
}

Date Today ()
{
	const std::time_t t = std::time (nullptr);
	std::tm tmv {};
#if defined (WINDOWS)
	localtime_s (&tmv, &t);
#else
	localtime_r (&t, &tmv);
#endif
	return Date (tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

Date Earliest (const Date& a, bool hasA, const Date& b, bool hasB)
{
	if (hasA && hasB) return a < b ? a : b;
	return hasA ? a : b;
}

// lexon te dyja vendet, bashkon dhe i shkruan perseri; kthen celesin, daten e pare dhe daten "sot"
void Sync (std::string* key, Date* first, Date* today, const std::string* newKey)
{
	Json::Value file = ReadState ();
	Json::Value office = ProjectStore::LoadOffice ();
	Json::Value pref = office[LicenseStore::OfficeKey ()];
	if (!pref.IsObject ()) pref = Json::Value::MakeObject ();

	Date f1, f2, l1, l2;
	const bool hf1 = Date::Parse (file.GetString ("first"), f1);
	const bool hf2 = Date::Parse (pref.GetString ("first"), f2);
	const bool hl1 = Date::Parse (file.GetString ("last"), l1);
	const bool hl2 = Date::Parse (pref.GetString ("last"), l2);
	Date now = Today ();
	Date firstRun = (hf1 || hf2) ? Earliest (f1, hf1, f2, hf2) : now;
	if (now < firstRun) firstRun = now;
	// ora e kompjuterit e kthyer pas: perdoret data me e vone e pare
	if (hl1 && now < l1) now = l1;
	if (hl2 && now < l2) now = l2;

	std::string k = newKey != nullptr ? *newKey : file.GetString ("key");
	if (k.empty ()) k = pref.GetString ("key");

	file.Set ("first", firstRun.ToIso ());
	file.Set ("last", now.ToIso ());
	file.Set ("key", k);
	WriteState (file);

	if (pref.GetString ("first") != firstRun.ToIso () || pref.GetString ("last") != now.ToIso () || pref.GetString ("key") != k) {
		pref.Set ("first", firstRun.ToIso ());
		pref.Set ("last", now.ToIso ());
		pref.Set ("key", k);
		office.Set (LicenseStore::OfficeKey (), pref);
		ProjectStore::SaveOffice (office);
	}
	*key = k;
	*first = firstRun;
	*today = now;
}

}	// namespace

LicenseStatus LicenseStore::Current ()
{
	std::string key;
	Date first, today;
	Sync (&key, &first, &today, nullptr);
	return License::Evaluate (key, first, today);
}

LicenseStatus LicenseStore::Activate (const std::string& keyIn, std::string* error)
{
	std::string key;
	for (char c : keyIn) if (c != ' ' && c != '\r' && c != '\n' && c != '\t') key += c;
	const LicenseInfo li = License::Parse (key);
	if (!li.signatureOk) {
		if (error != nullptr) *error = li.error;
		return Current ();
	}
	std::string k;
	Date first, today;
	Sync (&k, &first, &today, nullptr);
	const LicenseStatus test = License::Evaluate (key, first, today);
	if (test.state != LicenseStatus::Active) {
		if (error != nullptr)
			*error = today < li.from ? "Licenca fillon m\xC3\xAB " + li.from.ToAlb () + "."
									 : "Ky \xC3\xA7" "el\xC3\xABs ka skaduar m\xC3\xAB " + li.to.ToAlb () + ".";
		return Current ();
	}
	Sync (&k, &first, &today, &key);
	return License::Evaluate (k, first, today);
}

Json::Value LicenseStore::ToJson (const LicenseStatus& st)
{
	Json::Value j = Json::Value::MakeObject ();
	j.Set ("state", st.StateName ());
	j.Set ("usable", st.usable);
	j.Set ("daysLeft", static_cast<double> (st.daysLeft));
	j.Set ("warn", st.warn);
	j.Set ("trialEnd", st.trialEnd.ToAlb ());
	if (st.license.signatureOk) {
		j.Set ("id", st.license.id);
		j.Set ("name", st.license.name);
		j.Set ("from", st.license.from.ToAlb ());
		j.Set ("to", st.license.to.ToAlb ());
	}
	if (!st.keyError.empty ()) j.Set ("keyError", st.keyError);
	Json::Value c = Json::Value::MakeObject ();
	c.Set ("name", License::ContactName ());
	c.Set ("email", License::ContactEmail ());
	c.Set ("phone", License::ContactPhone ());
	j.Set ("contact", c);
	return j;
}

}	// namespace AC
}	// namespace ARBS
