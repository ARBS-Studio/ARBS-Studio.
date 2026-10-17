// =============================================================================
// ARBS Studio - shtresa Archicad : ArbsUtils.cpp
// =============================================================================
#include "AC/ArbsUtils.hpp"

#include "Core/Embedded.hpp"

#include "File.hpp"
#include "FileSystem.hpp"
#include "Folder.hpp"

#include <cstdlib>
#include <map>

#if defined (WINDOWS)
#include <shellapi.h>
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

bool OpenWithSystem (const std::string& utf8Path)
{
	if (utf8Path.empty ()) return false;
#if defined (WINDOWS)
	const GS::UniString p = ToUni (utf8Path);
	const HINSTANCE h = ShellExecuteW (nullptr, L"open", reinterpret_cast<const wchar_t*> (p.ToUStr ().Get ()), nullptr, nullptr, SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR> (h) > 32;
#else
	std::string cmd = "open \"";
	for (char c : utf8Path) { if (c == '"' || c == '\\' || c == '$' || c == '`') cmd += '\\'; cmd += c; }
	cmd += "\"";
	return std::system (cmd.c_str ()) == 0;
#endif
}

void Report (const std::string& msg)
{
	ACAPI_WriteReport (ToUni (msg), false);
}

}	// namespace AC
}	// namespace ARBS
