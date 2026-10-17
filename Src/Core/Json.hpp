// =============================================================================
// ARBS Studio - Core
// Json.hpp : biblioteke e vogel JSON (pa varesi te jashtme), UTF-8
// =============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ARBS {
namespace Json {

class Value;
using Array  = std::vector<Value>;
using Member = std::pair<std::string, Value>;
using Object = std::vector<Member>;	// ruan rendin e celesave

class ParseError : public std::runtime_error {
public:
	explicit ParseError (const std::string& msg) : std::runtime_error (msg) {}
};

class Value {
public:
	enum class Type { Null, Bool, Number, String, ArrayT, ObjectT };

	Value ();
	Value (std::nullptr_t);
	Value (bool b);
	Value (int n);
	Value (long n);
	Value (long long n);
	Value (unsigned n);
	Value (double n);
	Value (const char* s);
	Value (const std::string& s);
	Value (std::string&& s);
	Value (const Array& a);
	Value (Array&& a);
	Value (const Object& o);
	Value (Object&& o);

	Value (const Value& other);
	Value (Value&& other) noexcept = default;
	Value& operator= (const Value& other);
	Value& operator= (Value&& other) noexcept = default;
	~Value () = default;

	static Value MakeArray ()  { return Value (Array ()); }
	static Value MakeObject () { return Value (Object ()); }

	Type GetType () const { return type; }
	bool IsNull ()   const { return type == Type::Null; }
	bool IsBool ()   const { return type == Type::Bool; }
	bool IsNumber () const { return type == Type::Number; }
	bool IsString () const { return type == Type::String; }
	bool IsArray ()  const { return type == Type::ArrayT; }
	bool IsObject () const { return type == Type::ObjectT; }

	bool               AsBool   (bool def = false) const;
	double             AsNumber (double def = 0.0) const;
	int                AsInt    (int def = 0) const;
	const std::string& AsString () const;		// "" nese nuk eshte string
	std::string        AsString (const std::string& def) const;
	const Array&       AsArray  () const;		// bosh nese nuk eshte array
	Array&             AsArray  ();
	const Object&      AsObject () const;
	Object&            AsObject ();

	// Objekt
	bool         Has (const std::string& key) const;
	const Value& operator[] (const std::string& key) const;	// Null nese mungon
	Value&       operator[] (const std::string& key);		// krijon nese mungon
	Value&       Set (const std::string& key, Value v);
	std::string  GetString (const std::string& key, const std::string& def = "") const;
	double       GetNumber (const std::string& key, double def = 0.0) const;
	int          GetInt    (const std::string& key, int def = 0) const;
	bool         GetBool   (const std::string& key, bool def = false) const;

	// Array
	size_t       Size () const;
	const Value& operator[] (size_t i) const;
	Value&       Push (Value v);

	std::string Dump (bool pretty = false) const;
	static Value Parse (const std::string& text);

private:
	void DumpTo (std::string& out, bool pretty, int indent) const;

	Type        type;
	bool        b = false;
	double      n = 0.0;
	std::string s;
	std::unique_ptr<Array>  a;	// kopjim i thelle ne copy-ctor
	std::unique_ptr<Object> o;
};

std::string Escape (const std::string& s);

}	// namespace Json
}	// namespace ARBS
