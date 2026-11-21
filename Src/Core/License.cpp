// =============================================================================
// ARBS Studio - Core : License.cpp
// Verifikimi Ed25519 sipas TweetNaCl (D. J. Bernstein et al., domen publik), vetem pjesa e
// verifikimit, plus SHA-512 (FIPS 180-4). Konstantet jane llogaritur nga perkufizimet.
// =============================================================================
#include "License.hpp"
#include "Json.hpp"

#include <cctype>
#include <cstdint>
#include <cstring>

namespace ARBS {

namespace {

typedef long long i64;
typedef i64 gf[16];

// celesi publik i ARBS Studio (celesi privat ndodhet vetem ne ARBS License Manager)
const unsigned char kPublicKey[32] = { 0x4c, 0x1b, 0x0f, 0xeb, 0x6d, 0xca, 0x52, 0xa9, 0x9e, 0x26, 0xfc, 0x6e, 0x89, 0x4c, 0xa5, 0x9e, 0x68, 0x8e, 0xad, 0x5e, 0xc5, 0x5c, 0x1e, 0xda, 0xe4, 0x12, 0x67, 0x3f, 0xb1, 0x6a, 0x44, 0x46 };

const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070, 0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0, 0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
const gf X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c, 0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
const gf Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
const gf I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43, 0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};
const i64 L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
const uint64_t K512[80] = {0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};
const uint64_t H512[8] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};

const gf gf0 = { 0 };
const gf gf1 = { 1 };

uint64_t R64 (uint64_t x, int c) { return (x >> c) | (x << (64 - c)); }
uint64_t Load64 (const unsigned char* x) { uint64_t u = 0; for (int i = 0; i < 8; ++i) u = (u << 8) | x[i]; return u; }
void Store64 (unsigned char* x, uint64_t u) { for (int i = 7; i >= 0; --i) { x[i] = static_cast<unsigned char> (u); u >>= 8; } }

void Sha512Block (uint64_t h[8], const unsigned char* p)
{
	uint64_t w[80];
	for (int i = 0; i < 16; ++i) w[i] = Load64 (p + 8 * i);
	for (int i = 16; i < 80; ++i) {
		const uint64_t s0 = R64 (w[i - 15], 1) ^ R64 (w[i - 15], 8) ^ (w[i - 15] >> 7);
		const uint64_t s1 = R64 (w[i - 2], 19) ^ R64 (w[i - 2], 61) ^ (w[i - 2] >> 6);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	uint64_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
	for (int i = 0; i < 80; ++i) {
		const uint64_t S1 = R64 (e, 14) ^ R64 (e, 18) ^ R64 (e, 41);
		const uint64_t ch = (e & f) ^ (~e & g);
		const uint64_t t1 = hh + S1 + ch + K512[i] + w[i];
		const uint64_t S0 = R64 (a, 28) ^ R64 (a, 34) ^ R64 (a, 39);
		const uint64_t mj = (a & b) ^ (a & c) ^ (b & c);
		const uint64_t t2 = S0 + mj;
		hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void Set (gf r, const gf a) { for (int i = 0; i < 16; ++i) r[i] = a[i]; }

void Car (gf o)
{
	for (int i = 0; i < 16; ++i) {
		o[i] += (static_cast<i64> (1) << 16);
		const i64 c = o[i] >> 16;
		o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
		o[i] -= c * 65536;
	}
}

void Sel (gf p, gf q, int b)
{
	const i64 c = ~(static_cast<i64> (b) - 1);
	for (int i = 0; i < 16; ++i) { const i64 t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; }
}

void Pack (unsigned char* o, const gf n)
{
	gf m, t;
	Set (t, n);
	Car (t); Car (t); Car (t);
	for (int j = 0; j < 2; ++j) {
		m[0] = t[0] - 0xffed;
		for (int i = 1; i < 15; ++i) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; }
		m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
		const int b = static_cast<int> ((m[15] >> 16) & 1);
		m[14] &= 0xffff;
		Sel (t, m, 1 - b);
	}
	for (int i = 0; i < 16; ++i) { o[2 * i] = static_cast<unsigned char> (t[i] & 0xff); o[2 * i + 1] = static_cast<unsigned char> (t[i] >> 8); }
}

bool Neq (const gf a, const gf b)
{
	unsigned char c[32], d[32];
	Pack (c, a);
	Pack (d, b);
	return std::memcmp (c, d, 32) != 0;
}

int Par (const gf a) { unsigned char d[32]; Pack (d, a); return d[0] & 1; }

void Unpack (gf o, const unsigned char* n)
{
	for (int i = 0; i < 16; ++i) o[i] = n[2 * i] + (static_cast<i64> (n[2 * i + 1]) << 8);
	o[15] &= 0x7fff;
}

void A (gf o, const gf a, const gf b) { for (int i = 0; i < 16; ++i) o[i] = a[i] + b[i]; }
void Z (gf o, const gf a, const gf b) { for (int i = 0; i < 16; ++i) o[i] = a[i] - b[i]; }

void M (gf o, const gf a, const gf b)
{
	i64 t[31];
	for (int i = 0; i < 31; ++i) t[i] = 0;
	for (int i = 0; i < 16; ++i) for (int j = 0; j < 16; ++j) t[i + j] += a[i] * b[j];
	for (int i = 0; i < 15; ++i) t[i] += 38 * t[i + 16];
	for (int i = 0; i < 16; ++i) o[i] = t[i];
	Car (o);
	Car (o);
}

void S (gf o, const gf a) { M (o, a, a); }

void Inv (gf o, const gf i)
{
	gf c;
	Set (c, i);
	for (int a = 253; a >= 0; --a) { S (c, c); if (a != 2 && a != 4) M (c, c, i); }
	Set (o, c);
}

void Pow2523 (gf o, const gf i)
{
	gf c;
	Set (c, i);
	for (int a = 250; a >= 0; --a) { S (c, c); if (a != 1) M (c, c, i); }
	Set (o, c);
}

void Add (gf p[4], gf q[4])
{
	gf a, b, c, d, t, e, f, g, h;
	Z (a, p[1], p[0]); Z (t, q[1], q[0]); M (a, a, t);
	A (b, p[0], p[1]); A (t, q[0], q[1]); M (b, b, t);
	M (c, p[3], q[3]); M (c, c, D2);
	M (d, p[2], q[2]); A (d, d, d);
	Z (e, b, a); Z (f, d, c); A (g, d, c); A (h, b, a);
	M (p[0], e, f); M (p[1], h, g); M (p[2], g, f); M (p[3], e, h);
}

void Cswap (gf p[4], gf q[4], int b) { for (int i = 0; i < 4; ++i) Sel (p[i], q[i], b); }

void PackPoint (unsigned char* r, gf p[4])
{
	gf tx, ty, zi;
	Inv (zi, p[2]);
	M (tx, p[0], zi);
	M (ty, p[1], zi);
	Pack (r, ty);
	r[31] ^= static_cast<unsigned char> (Par (tx) << 7);
}

void ScalarMult (gf p[4], gf q[4], const unsigned char* s)
{
	Set (p[0], gf0); Set (p[1], gf1); Set (p[2], gf1); Set (p[3], gf0);
	for (int i = 255; i >= 0; --i) {
		const int b = (s[i / 8] >> (i & 7)) & 1;
		Cswap (p, q, b);
		Add (q, p);
		Add (p, p);
		Cswap (p, q, b);
	}
}

void ScalarBase (gf p[4], const unsigned char* s)
{
	gf q[4];
	Set (q[0], X); Set (q[1], Y); Set (q[2], gf1); M (q[3], X, Y);
	ScalarMult (p, q, s);
}

void ModL (unsigned char* r, i64 x[64])
{
	i64 carry;
	for (int i = 63; i >= 32; --i) {
		carry = 0;
		int j;
		for (j = i - 32; j < i - 12; ++j) {
			x[j] += carry - 16 * x[i] * L[j - (i - 32)];
			carry = (x[j] + 128) >> 8;
			x[j] -= carry * 256;
		}
		x[j] += carry;
		x[i] = 0;
	}
	carry = 0;
	for (int j = 0; j < 32; ++j) { x[j] += carry - (x[31] >> 4) * L[j]; carry = x[j] >> 8; x[j] &= 255; }
	for (int j = 0; j < 32; ++j) x[j] -= carry * L[j];
	for (int i = 0; i < 32; ++i) { x[i + 1] += x[i] >> 8; r[i] = static_cast<unsigned char> (x[i] & 255); }
}

void Reduce (unsigned char* r)
{
	i64 x[64];
	for (int i = 0; i < 64; ++i) x[i] = static_cast<i64> (r[i]);
	for (int i = 0; i < 64; ++i) r[i] = 0;
	ModL (r, x);
}

bool UnpackNeg (gf r[4], const unsigned char p[32])
{
	gf t, chk, num, den, den2, den4, den6;
	Set (r[2], gf1);
	Unpack (r[1], p);
	S (num, r[1]);
	M (den, num, D);
	Z (num, num, r[2]);
	A (den, r[2], den);
	S (den2, den); S (den4, den2); M (den6, den4, den2);
	M (t, den6, num); M (t, t, den);
	Pow2523 (t, t);
	M (t, t, num); M (t, t, den); M (t, t, den); M (r[0], t, den);
	S (chk, r[0]); M (chk, chk, den);
	if (Neq (chk, num)) M (r[0], r[0], I);
	S (chk, r[0]); M (chk, chk, den);
	if (Neq (chk, num)) return false;
	if (Par (r[0]) == (p[31] >> 7)) Z (r[0], gf0, r[0]);
	M (r[3], r[0], r[1]);
	return true;
}

}	// namespace

void License::Sha512 (const unsigned char* msg, size_t len, unsigned char out[64])
{
	uint64_t h[8];
	for (int i = 0; i < 8; ++i) h[i] = H512[i];
	size_t off = 0;
	for (; off + 128 <= len; off += 128) Sha512Block (h, msg + off);
	unsigned char tail[256];
	std::memset (tail, 0, sizeof (tail));
	const size_t rest = len - off;
	if (rest > 0) std::memcpy (tail, msg + off, rest);
	tail[rest] = 0x80;
	const size_t n = rest < 112 ? 128 : 256;
	const uint64_t bits = static_cast<uint64_t> (len) * 8u;
	Store64 (tail + n - 8, bits);
	Store64 (tail + n - 16, static_cast<uint64_t> (len >> 61));
	Sha512Block (h, tail);
	if (n == 256) Sha512Block (h, tail + 128);
	for (int i = 0; i < 8; ++i) Store64 (out + 8 * i, h[i]);
}

bool License::Verify (const unsigned char sig[64], const unsigned char* msg, size_t len, const unsigned char pk[32])
{
	gf p[4], q[4];
	if (!UnpackNeg (q, pk)) return false;
	std::string buf (reinterpret_cast<const char*> (sig), 32);
	buf.append (reinterpret_cast<const char*> (pk), 32);
	buf.append (reinterpret_cast<const char*> (msg), len);
	unsigned char h[64];
	Sha512 (reinterpret_cast<const unsigned char*> (buf.data ()), buf.size (), h);
	Reduce (h);
	ScalarMult (p, q, h);
	ScalarBase (q, sig + 32);
	Add (p, q);
	unsigned char t[32];
	PackPoint (t, p);
	return std::memcmp (t, sig, 32) == 0;
}

std::string License::Base64UrlDecode (const std::string& s)
{
	std::string out;
	int val = 0, bits = -8;
	for (char c : s) {
		int v;
		if (c >= 'A' && c <= 'Z') v = c - 'A';
		else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
		else if (c >= '0' && c <= '9') v = c - '0' + 52;
		else if (c == '-' || c == '+') v = 62;
		else if (c == '_' || c == '/') v = 63;
		else continue;
		val = (val << 6) | v;
		bits += 6;
		if (bits >= 0) { out.push_back (static_cast<char> ((val >> bits) & 0xFF)); bits -= 8; }
	}
	return out;
}

LicenseInfo License::Parse (const std::string& keyIn)
{
	LicenseInfo li;
	std::string key;
	for (char c : keyIn) if (!std::isspace (static_cast<unsigned char> (c))) key += c;	// email-et i ndajne rreshtat
	if (key.empty ()) { li.error = "Nuk ka \xC3\xA7" "el\xC3\xABs licence."; return li; }
	if (key.rfind ("ARBS-", 0) != 0 || key.find ('.') == std::string::npos) { li.error = "Formati i \xC3\xA7" "el\xC3\xABsit nuk \xC3\xABsht\xC3\xAB i sakt\xC3\xAB."; return li; }
	const size_t dot = key.find ('.');
	const std::string payload = Base64UrlDecode (key.substr (5, dot - 5));
	const std::string sig = Base64UrlDecode (key.substr (dot + 1));
	if (sig.size () != 64 || payload.empty ()) { li.error = "\xC3\x87" "el\xC3\xABsi \xC3\xABsht\xC3\xAB i paplot\xC3\xAB (kopjoni t\xC3\xAB gjith\xC3\xAB rreshtin)."; return li; }
	const std::string msg = "ARBS-LIC1|" + payload;
	if (!Verify (reinterpret_cast<const unsigned char*> (sig.data ()), reinterpret_cast<const unsigned char*> (msg.data ()), msg.size (), kPublicKey)) {
		li.error = "\xC3\x87" "el\xC3\xABsi nuk \xC3\xABsht\xC3\xAB i vlefsh\xC3\xABm.";
		return li;
	}
	try {
		const Json::Value j = Json::Value::Parse (payload);
		if (j.GetInt ("v") != 1) { li.error = "Version i panjohur i licenc\xC3\xABs."; return li; }
		li.id = j.GetString ("id");
		li.name = j.GetString ("n");
		li.email = j.GetString ("e");
		if (!Date::Parse (j.GetString ("f"), li.from) || !Date::Parse (j.GetString ("t"), li.to)) { li.error = "Datat e licenc\xC3\xABs mungojn\xC3\xAB."; return li; }
	} catch (...) {
		li.error = "P\xC3\xABrmbajtja e licenc\xC3\xABs \xC3\xABsht\xC3\xAB e d\xC3\xABmtuar.";
		return li;
	}
	li.signatureOk = true;
	return li;
}

const char* LicenseStatus::StateName () const
{
	switch (state) {
		case Active:    return "active";
		case Trial:     return "trial";
		case Expired:   return "expired";
		default:        return "trialOver";
	}
}

LicenseStatus License::Evaluate (const std::string& key, const Date& firstRun, const Date& today)
{
	LicenseStatus st;
	st.trialEnd = firstRun.AddDays (kTrialDays - 1);	// dita e pare + 13 = 14 dite
	bool hadLicense = false;
	if (!key.empty ()) {
		const LicenseInfo li = Parse (key);
		if (li.signatureOk) {
			st.license = li;
			hadLicense = true;
			if (li.from <= today && today <= li.to) {
				st.state = LicenseStatus::Active;
				st.usable = true;
				st.daysLeft = li.to.ToDays () - today.ToDays () + 1;
				st.warn = st.daysLeft <= kWarnDays;
				return st;
			}
			if (today < li.from) { st.keyError = "Licenca fillon m\xC3\xAB " + li.from.ToAlb () + "."; hadLicense = false; }
		} else {
			st.keyError = li.error;
		}
	}
	if (today <= st.trialEnd && !hadLicense) {
		st.state = LicenseStatus::Trial;
		st.usable = true;
		st.daysLeft = st.trialEnd.ToDays () - today.ToDays () + 1;
		return st;
	}
	st.state = hadLicense ? LicenseStatus::Expired : LicenseStatus::TrialOver;
	return st;
}

}	// namespace ARBS
