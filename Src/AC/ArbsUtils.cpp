// =============================================================================
// ARBS Studio - shtresa Archicad : ArbsUtils.cpp
// =============================================================================
#include "AC/ArbsUtils.hpp"

#include "Core/Embedded.hpp"

#include "File.hpp"
#include "FileSystem.hpp"
#include "Folder.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <map>
#include <thread>

#if defined (WINDOWS)
#include <shellapi.h>
#include <objbase.h>
#endif

#ifndef ADDON_VERSION
#define ADDON_VERSION "0"
#endif

namespace ARBS {
namespace AC {

GS::UniString ToUni (const std::string& utf8)
{
	return GS::UniString (utf8.c_str (), CC_UTF8);
}

std::string ToUtf8 (const GS::UniString& s)
{
	return std::string (s.ToCStr (CC_UTF8).Get ());
}

std::string ToUtf8 (const GS::uchar_t* s)
{
	return ToUtf8 (GS::UniString (s));
}

void CopyToUChar (const GS::UniString& s, GS::uchar_t* dst, size_t capacity)
{
	if (dst == nullptr || capacity == 0) return;
	GS::ucsncpy (dst, s.ToUStr ().Get (), capacity - 1);	// UStr jeton deri ne fund te shprehjes
	dst[capacity - 1] = 0;
}

static Catalog s_catalog;
static bool s_loaded = false;
static std::string s_error;

const Catalog& GetCatalog ()
{
	if (!s_loaded) {
		s_loaded = true;
		std::map<std::string, std::string> files;
		for (const std::string& name : Catalog::FileNames ()) {
			std::string content;
			if (Embedded::Get (name, content))
				files[name] = content;
		}
		if (!s_catalog.LoadFromStrings (files, &s_error))
			Report ("ARBS Studio: gabim ne te dhenat e ngulitura: " + s_error);
	}
	return s_catalog;
}

std::string CatalogError ()
{
	GetCatalog ();
	return s_error;
}

std::string Base64 (const std::string& in)
{
	static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve ((in.size () + 2) / 3 * 4);
	size_t i = 0;
	while (i + 2 < in.size ()) {
		const unsigned v = (static_cast<unsigned char> (in[i]) << 16) | (static_cast<unsigned char> (in[i + 1]) << 8) | static_cast<unsigned char> (in[i + 2]);
		out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += tbl[v & 63];
		i += 3;
	}
	if (i + 1 == in.size ()) {
		const unsigned v = static_cast<unsigned char> (in[i]) << 16;
		out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += "==";
	} else if (i + 2 == in.size ()) {
		const unsigned v = (static_cast<unsigned char> (in[i]) << 16) | (static_cast<unsigned char> (in[i + 1]) << 8);
		out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += '=';
	}
	return out;
}

std::string LogoDataUri ()
{
	static std::string uri;
	if (uri.empty ()) {
		std::string png;
		if (Embedded::Get ("logo_arbs.png", png))
			uri = "data:image/png;base64," + Base64 (png);
	}
	return uri;
}

std::string LocationToUtf8 (const IO::Location& loc)
{
	GS::UniString path;
	loc.ToPath (&path);
	return ToUtf8 (path);
}

IO::Location OutputFolder (const Json::Value& project)
{
	IO::Location loc;
	if (IO::fileSystem.GetSpecialLocation (IO::FileSystem::UserDocuments, &loc) != NoError) {
		ACAPI_GetOwnLocation (&loc);
		loc.DeleteLastLocalName ();
	}
	loc.AppendToLocal (IO::Name ("ARBS Studio"));
	std::string folder = project.GetString ("nr");
	const std::string name = project.GetString ("emri");
	if (!name.empty ()) folder += (folder.empty () ? "" : " - ") + name;
	if (folder.empty ()) folder = "Projekti";
	// pa karaktere te palejuara per emra dosjesh
	std::string clean;
	for (char c : folder) clean += (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') ? '-' : c;
	loc.AppendToLocal (IO::Name (ToUni (clean)));
	IO::fileSystem.CreateFolderTree (loc);
	return loc;
}

std::vector<std::string> SaveOutputs (const Json::Value& project, const std::vector<ReportOutput>& outputs, GSErrCode* errOut)
{
	std::vector<std::string> paths;
	const IO::Location folder = OutputFolder (project);
	GSErrCode err = NoError;
	for (const ReportOutput& r : outputs) {
		IO::Location loc = folder;
		loc.AppendToLocal (IO::Name (ToUni (r.fileName)));
		IO::File file (loc, IO::File::Create);
		err = file.GetStatus ();
		if (err == NoError) err = file.Open (IO::File::WriteEmptyMode);
		if (err == NoError) {
			err = file.WriteBin (r.content.data (), static_cast<USize> (r.content.size ()));
			file.Close ();
		}
		if (err != NoError) {
			Report ("ARBS Studio: nuk u ruajt skedari " + r.fileName);
			break;
		}
		paths.push_back (LocationToUtf8 (loc));
	}
	if (errOut != nullptr) *errOut = err;
	return paths;
}

// Hapja e skedarit me programin e sistemit ne nje thread te vecante: ShellExecute (Word/DDE)
// mund te prese gjate dhe nuk duhet te bllokoje Archicad-in apo paleten.
bool OpenWithSystem (const std::string& utf8Path)
{
	if (utf8Path.empty ()) return false;
	Log::Write ("OPEN  " + utf8Path);
#if defined (WINDOWS)
	const GS::UniString p = ToUni (utf8Path);
	std::wstring wide (reinterpret_cast<const wchar_t*> (p.ToUStr ().Get ()));
	try {
		std::thread ([wide] () {
			const HRESULT hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			ShellExecuteW (nullptr, L"open", wide.c_str (), nullptr, nullptr, SW_SHOWNORMAL);
			if (SUCCEEDED (hr)) CoUninitialize ();
		}).detach ();
	} catch (...) {
		return false;
	}
	return true;
#else
	std::string cmd = "open \"";
	for (char c : utf8Path) { if (c == '"' || c == '\\' || c == '$' || c == '`') cmd += '\\'; cmd += c; }
	cmd += "\"";
	try {
		std::thread ([cmd] () { (void) std::system (cmd.c_str ()); }).detach ();
	} catch (...) {
		return false;
	}
	return true;
#endif
}

void Report (const std::string& msg)
{
	ACAPI_WriteReport (ToUni (msg), false);
}

// -----------------------------------------------------------------------------
// Log
// -----------------------------------------------------------------------------
namespace Log {

namespace {
	using Clock = std::chrono::steady_clock;
	std::deque<std::string> s_lines;			// e gjithe sesioni (kufizuar)
	std::string             s_path;
	bool                    s_failed = false;
	int                     s_nextId = 0;
	int                     s_opId = 0;
	std::string             s_opCmd;
	Clock::time_point       s_opStart;
	Clock::time_point       s_lastStage;

	std::string Now ()
	{
		const std::time_t t = std::time (nullptr);
		std::tm tmv {};
#if defined (WINDOWS)
		localtime_s (&tmv, &t);
#else
		localtime_r (&t, &tmv);
#endif
		char buf[32];
		std::strftime (buf, sizeof (buf), "%Y-%m-%d %H:%M:%S", &tmv);
		return buf;
	}

	IO::Location Folder ()
	{
		IO::Location loc;
		if (IO::fileSystem.GetSpecialLocation (IO::FileSystem::UserDocuments, &loc) != NoError) {
			ACAPI_GetOwnLocation (&loc);
			loc.DeleteLastLocalName ();
		}
		loc.AppendToLocal (IO::Name ("ARBS Studio"));
		loc.AppendToLocal (IO::Name ("Log"));
		IO::fileSystem.CreateFolderTree (loc);
		return loc;
	}

	void Flush ()
	{
		if (s_failed) return;
		IO::Location loc = Folder ();
		if (s_path.empty ()) {
			const std::time_t t = std::time (nullptr);
			std::tm tmv {};
#if defined (WINDOWS)
			localtime_s (&tmv, &t);
#else
			localtime_r (&t, &tmv);
#endif
			char name[64];
			std::strftime (name, sizeof (name), "arbs_log_%Y%m%d_%H%M%S.txt", &tmv);
			s_path = name;
		}
		loc.AppendToLocal (IO::Name (ToUni (s_path)));
		std::string all = "\xEF\xBB\xBF";
		for (const std::string& l : s_lines) { all += l; all += "\r\n"; }
		IO::File file (loc, IO::File::Create);
		if (file.GetStatus () != NoError || file.Open (IO::File::WriteEmptyMode) != NoError) { s_failed = true; return; }
		file.WriteBin (all.data (), static_cast<USize> (all.size ()));
		file.Close ();
	}
}

void Write (const std::string& line)
{
	if (s_lines.empty ()) {
		s_lines.push_back ("ARBS Studio " ADDON_VERSION " - log diagnostik, sesioni filloi " + Now ());
#if defined (WINDOWS)
		s_lines.push_back ("Platforma: Windows");
#else
		s_lines.push_back ("Platforma: macOS");
#endif
	}
	s_lines.push_back (Now () + "  " + line);
	while (s_lines.size () > 5000) s_lines.erase (s_lines.begin () + 2);
	Flush ();
}

int Begin (const std::string& command)
{
	s_opId = ++s_nextId;
	s_opCmd = command;
	s_opStart = Clock::now ();
	s_lastStage = s_opStart;
	Write ("#" + std::to_string (s_opId) + " START " + command);
	return s_opId;
}

long long ElapsedMs ()
{
	return static_cast<long long> (std::chrono::duration_cast<std::chrono::milliseconds> (Clock::now () - s_opStart).count ());
}

void Stage (const std::string& stage, long long count)
{
	const Clock::time_point now = Clock::now ();
	const long long stepMs = static_cast<long long> (std::chrono::duration_cast<std::chrono::milliseconds> (now - s_lastStage).count ());
	s_lastStage = now;
	Write ("#" + std::to_string (s_opId) + "   " + stage + (count >= 0 ? " [" + std::to_string (count) + "]" : std::string ()) +
		   "  +" + std::to_string (stepMs) + " ms (gjithsej " + std::to_string (ElapsedMs ()) + " ms)");
}

void End (bool ok, const std::string& error)
{
	Write ("#" + std::to_string (s_opId) + " END " + s_opCmd + (ok ? " OK" : " GABIM: " + error) + "  " + std::to_string (ElapsedMs ()) + " ms");
}

std::string FolderPath ()  { return LocationToUtf8 (Folder ()); }

std::string CurrentPath ()
{
	if (s_path.empty ()) Write ("(log-u u hap)");
	IO::Location loc = Folder ();
	loc.AppendToLocal (IO::Name (ToUni (s_path)));
	return LocationToUtf8 (loc);
}

}	// namespace Log

}	// namespace AC
}	// namespace ARBS
