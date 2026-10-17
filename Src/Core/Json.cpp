// =============================================================================
// ARBS Studio - Core : Json.cpp
// =============================================================================
#include "Json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace ARBS {
namespace Json {

namespace {
const Value&       NullValue ()   { static const Value v; return v; }
const std::string& EmptyString () { static const std::string s; return s; }
const Array&       EmptyArray ()  { static const Array a; return a; }
const Object&      EmptyObject () { static const Object o; return o; }
}

Value::Value () : type (Type::Null) {}
Value::Value (std::nullptr_t) : type (Type::Null) {}
Value::Value (bool v) : type (Type::Bool), b (v) {}
Value::Value (int v) : type (Type::Number), n (v) {}
Value::Value (long v) : type (Type::Number), n (static_cast<double> (v)) {}
Value::Value (long long v) : type (Type::Number), n (static_cast<double> (v)) {}
Value::Value (unsigned v) : type (Type::Number), n (v) {}
Value::Value (double v) : type (Type::Number), n (v) {}
Value::Value (const char* v) : type (Type::String), s (v != nullptr ? v : "") {}
Value::Value (const std::string& v) : type (Type::String), s (v) {}
Value::Value (std::string&& v) : type (Type::String), s (std::move (v)) {}
Value::Value (const Array& v) : type (Type::ArrayT), a (new Array (v)) {}
Value::Value (Array&& v) : type (Type::ArrayT), a (new Array (std::move (v))) {}
Value::Value (const Object& v) : type (Type::ObjectT), o (new Object (v)) {}
Value::Value (Object&& v) : type (Type::ObjectT), o (new Object (std::move (v))) {}

Value::Value (const Value& other) :
	type (other.type), b (other.b), n (other.n), s (other.s),
	a (other.a ? new Array (*other.a) : nullptr),
	o (other.o ? new Object (*other.o) : nullptr)
{
}

Value& Value::operator= (const Value& other)
{
	if (this != &other) {
		Value tmp (other);
		*this = std::move (tmp);
	}
	return *this;
}

bool Value::AsBool (bool def) const
{
	if (type == Type::Bool)   return b;
	if (type == Type::Number) return n != 0.0;
	if (type == Type::String) return s == "true" || s == "1" || s == "po";
	return def;
}

double Value::AsNumber (double def) const
{
	if (type == Type::Number) return n;
	if (type == Type::Bool)   return b ? 1.0 : 0.0;
	if (type == Type::String && !s.empty ()) {
		std::string t = s;
		for (char& c : t) if (c == ',') c = '.';	// presje dhjetore shqip
		char* end = nullptr;
		double v = std::strtod (t.c_str (), &end);
		if (end != t.c_str ()) return v;
	}
	return def;
}

int Value::AsInt (int def) const
{
	if (type == Type::Null) return def;
	return static_cast<int> (std::lround (AsNumber (def)));
}

const std::string& Value::AsString () const
{
	return type == Type::String ? s : EmptyString ();
}

std::string Value::AsString (const std::string& def) const
{
	if (type == Type::String) return s;
	if (type == Type::Number) {
		char buf[64];
		if (std::floor (n) == n && std::fabs (n) < 1e15)
			std::snprintf (buf, sizeof (buf), "%.0f", n);
		else
			std::snprintf (buf, sizeof (buf), "%.6g", n);
		return buf;
	}
	if (type == Type::Bool) return b ? "true" : "false";
	return def;
}

const Array& Value::AsArray () const   { return (type == Type::ArrayT && a) ? *a : EmptyArray (); }
const Object& Value::AsObject () const { return (type == Type::ObjectT && o) ? *o : EmptyObject (); }

Array& Value::AsArray ()
{
	if (type != Type::ArrayT) { *this = Value (Array ()); }
	return *a;
}

Object& Value::AsObject ()
{
	if (type != Type::ObjectT) { *this = Value (Object ()); }
	return *o;
}

bool Value::Has (const std::string& key) const
{
	for (const Member& m : AsObject ())
		if (m.first == key) return true;
	return false;
}

const Value& Value::operator[] (const std::string& key) const
{
	for (const Member& m : AsObject ())
		if (m.first == key) return m.second;
	return NullValue ();
}

Value& Value::operator[] (const std::string& key)
{
	Object& obj = AsObject ();
	for (Member& m : obj)
		if (m.first == key) return m.second;
	obj.emplace_back (key, Value ());
	return obj.back ().second;
}

Value& Value::Set (const std::string& key, Value v)
{
	Value& slot = (*this)[key];
	slot = std::move (v);
	return slot;
}

std::string Value::GetString (const std::string& key, const std::string& def) const
{
	const Value& v = (*this)[key];
	return v.IsNull () ? def : v.AsString (def);
}

double Value::GetNumber (const std::string& key, double def) const { return (*this)[key].AsNumber (def); }
int    Value::GetInt    (const std::string& key, int def) const    { return (*this)[key].AsInt (def); }
bool   Value::GetBool   (const std::string& key, bool def) const   { return (*this)[key].AsBool (def); }

size_t Value::Size () const
{
	if (type == Type::ArrayT)  return a ? a->size () : 0;
	if (type == Type::ObjectT) return o ? o->size () : 0;
	return 0;
}

const Value& Value::operator[] (size_t i) const
{
	const Array& arr = AsArray ();
	return i < arr.size () ? arr[i] : NullValue ();
}

Value& Value::Push (Value v)
{
	Array& arr = AsArray ();
	arr.push_back (std::move (v));
	return arr.back ();
}

// ---------------------------------------------------------------- serializim

std::string Escape (const std::string& in)
{
	std::string out;
	out.reserve (in.size () + 8);
	for (unsigned char c : in) {
		switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			default:
				if (c < 0x20) {
					char buf[8];
					std::snprintf (buf, sizeof (buf), "\\u%04x", c);
					out += buf;
				} else {
					out += static_cast<char> (c);	// UTF-8 kalon pa ndryshim
				}
		}
	}
	return out;
}

static void NewLine (std::string& out, bool pretty, int indent)
{
	if (!pretty) return;
	out += '\n';
	out.append (static_cast<size_t> (indent) * 2, ' ');
}

void Value::DumpTo (std::string& out, bool pretty, int indent) const
{
	switch (type) {
		case Type::Null: out += "null"; break;
		case Type::Bool: out += b ? "true" : "false"; break;
		case Type::Number: {
			if (!std::isfinite (n)) { out += "null"; break; }
			char buf[64];
			if (std::floor (n) == n && std::fabs (n) < 1e15)
				std::snprintf (buf, sizeof (buf), "%.0f", n);
			else
				std::snprintf (buf, sizeof (buf), "%.10g", n);
			out += buf;
			break;
		}
		case Type::String: out += '"'; out += Escape (s); out += '"'; break;
		case Type::ArrayT: {
			out += '[';
			const Array& arr = AsArray ();
			for (size_t i = 0; i < arr.size (); ++i) {
				if (i > 0) out += ',';
				NewLine (out, pretty, indent + 1);
				arr[i].DumpTo (out, pretty, indent + 1);
			}
			if (!arr.empty ()) NewLine (out, pretty, indent);
			out += ']';
			break;
		}
		case Type::ObjectT: {
			out += '{';
			const Object& obj = AsObject ();
			for (size_t i = 0; i < obj.size (); ++i) {
				if (i > 0) out += ',';
				NewLine (out, pretty, indent + 1);
				out += '"'; out += Escape (obj[i].first); out += pretty ? "\": " : "\":";
				obj[i].second.DumpTo (out, pretty, indent + 1);
			}
			if (!obj.empty ()) NewLine (out, pretty, indent);
			out += '}';
			break;
		}
	}
}

std::string Value::Dump (bool pretty) const
{
	std::string out;
	DumpTo (out, pretty, 0);
	return out;
}

// ---------------------------------------------------------------- parser

namespace {

class Parser {
public:
	explicit Parser (const std::string& t) : text (t) {}

	Value ParseDocument ()
	{
		SkipBom ();
		Value v = ParseValue ();
		SkipWs ();
		if (pos != text.size ()) Fail ("karaktere te teperta pas JSON");
		return v;
	}

private:
	const std::string& text;
	size_t pos = 0;
	int depth = 0;

	[[noreturn]] void Fail (const std::string& msg) const
	{
		throw ParseError ("JSON: " + msg + " (pozicioni " + std::to_string (pos) + ")");
	}

	void SkipBom ()
	{
		if (text.size () >= 3 && (unsigned char) text[0] == 0xEF && (unsigned char) text[1] == 0xBB && (unsigned char) text[2] == 0xBF)
			pos = 3;
	}

	void SkipWs ()
	{
		while (pos < text.size ()) {
			char c = text[pos];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos; continue; }
			// lejojme komente // ne skedaret e konfigurimit
			if (c == '/' && pos + 1 < text.size () && text[pos + 1] == '/') {
				while (pos < text.size () && text[pos] != '\n') ++pos;
				continue;
			}
			break;
		}
	}

	bool Match (const char* lit)
	{
		size_t len = std::strlen (lit);
		if (text.compare (pos, len, lit) == 0) { pos += len; return true; }
		return false;
	}

	Value ParseValue ()
	{
		SkipWs ();
		if (pos >= text.size ()) Fail ("fund i papritur");
		if (++depth > 200) Fail ("thellesi e tepert");
		Value result;
		char c = text[pos];
		if (c == '{')      result = ParseObject ();
		else if (c == '[') result = ParseArray ();
		else if (c == '"') result = Value (ParseString ());
		else if (Match ("true"))  result = Value (true);
		else if (Match ("false")) result = Value (false);
		else if (Match ("null"))  result = Value ();
		else result = ParseNumber ();
		--depth;
		return result;
	}

	Value ParseObject ()
	{
		Object obj;
		++pos;	// {
		SkipWs ();
		if (pos < text.size () && text[pos] == '}') { ++pos; return Value (std::move (obj)); }
		while (true) {
			SkipWs ();
			if (pos >= text.size () || text[pos] != '"') Fail ("pritej celesi");
			std::string key = ParseString ();
			SkipWs ();
			if (pos >= text.size () || text[pos] != ':') Fail ("pritej ':'");
			++pos;
			Value v = ParseValue ();
			obj.emplace_back (std::move (key), std::move (v));
			SkipWs ();
			if (pos < text.size () && text[pos] == ',') { ++pos; SkipWs (); if (pos < text.size () && text[pos] == '}') { ++pos; break; } continue; }
			if (pos < text.size () && text[pos] == '}') { ++pos; break; }
			Fail ("pritej ',' ose '}'");
		}
		return Value (std::move (obj));
	}

	Value ParseArray ()
	{
		Array arr;
		++pos;	// [
		SkipWs ();
		if (pos < text.size () && text[pos] == ']') { ++pos; return Value (std::move (arr)); }
		while (true) {
			arr.push_back (ParseValue ());
			SkipWs ();
			if (pos < text.size () && text[pos] == ',') { ++pos; SkipWs (); if (pos < text.size () && text[pos] == ']') { ++pos; break; } continue; }
			if (pos < text.size () && text[pos] == ']') { ++pos; break; }
			Fail ("pritej ',' ose ']'");
		}
		return Value (std::move (arr));
	}

	static void AppendUtf8 (std::string& out, uint32_t cp)
	{
		if (cp < 0x80) out += static_cast<char> (cp);
		else if (cp < 0x800) { out += static_cast<char> (0xC0 | (cp >> 6)); out += static_cast<char> (0x80 | (cp & 0x3F)); }
		else if (cp < 0x10000) { out += static_cast<char> (0xE0 | (cp >> 12)); out += static_cast<char> (0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char> (0x80 | (cp & 0x3F)); }
		else { out += static_cast<char> (0xF0 | (cp >> 18)); out += static_cast<char> (0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char> (0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char> (0x80 | (cp & 0x3F)); }
	}

	uint32_t ParseHex4 ()
	{
		if (pos + 4 > text.size ()) Fail ("\\u e paplote");
		uint32_t v = 0;
		for (int i = 0; i < 4; ++i) {
			char c = text[pos++];
			v <<= 4;
			if (c >= '0' && c <= '9') v |= static_cast<uint32_t> (c - '0');
			else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t> (c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t> (c - 'A' + 10);
			else Fail ("hex i pavlefshem");
		}
		return v;
	}

	std::string ParseString ()
	{
		std::string out;
		++pos;	// "
		while (true) {
			if (pos >= text.size ()) Fail ("string i pambyllur");
			char c = text[pos++];
			if (c == '"') break;
			if (c != '\\') { out += c; continue; }
			if (pos >= text.size ()) Fail ("escape i paplote");
			char e = text[pos++];
			switch (e) {
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u': {
					uint32_t cp = ParseHex4 ();
					if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= text.size () && text[pos] == '\\' && text[pos + 1] == 'u') {
						pos += 2;
						uint32_t lo = ParseHex4 ();
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					}
					AppendUtf8 (out, cp);
					break;
				}
				default: Fail ("escape i panjohur");
			}
		}
		return out;
	}

	Value ParseNumber ()
	{
		size_t start = pos;
		if (pos < text.size () && (text[pos] == '-' || text[pos] == '+')) ++pos;
		while (pos < text.size () && (std::isdigit (static_cast<unsigned char> (text[pos])) || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' || text[pos] == '-' || text[pos] == '+'))
			++pos;
		if (start == pos) Fail ("vlere e papritur");
		std::string num = text.substr (start, pos - start);
		char* end = nullptr;
		double v = std::strtod (num.c_str (), &end);
		if (end == num.c_str () || *end != '\0') Fail ("numer i pavlefshem");
		return Value (v);
	}
};

}	// namespace

Value Value::Parse (const std::string& t)
{
	Parser p (t);
	return p.ParseDocument ();
}

}	// namespace Json
}	// namespace ARBS
