// =============================================================================
// ARBS Studio - Core : License.hpp
// Licenca e abonimit (e pavarur nga licenca e Archicad-it).
//
// Celesi:  ARBS-<payload base64url>.<nenshkrimi base64url>
//   payload = JSON {"v":1,"id":"ARBS-2026-0001","n":"Emri i klientit","e":"email","f":"2026-11-21","t":"2027-11-21"}
//   nenshkrimi = Ed25519 mbi "ARBS-LIC1|" + payload, me celesin privat te ARBS Studio
//   (vetem ne "ARBS License Manager"); ne add-on gjendet vetem celesi publik.
// Gjendjet: licenca aktive (deri ne daten "t" perfshire), prove 14 dite nga nisja e pare,
// e skaduar / prova e mbaruar -> add-on-i shfaq vetem kontaktin dhe fushen per celesin.
// =============================================================================
#pragma once

#include "Date.hpp"
#include <string>

namespace ARBS {

struct LicenseInfo {
	bool        signatureOk = false;
	std::string id, name, email;
	Date        from, to;
	std::string error;		// pse celesi nuk pranohet
};

struct LicenseStatus {
	enum State { Active, Trial, Expired, TrialOver } state = TrialOver;
	bool        usable = false;		// funksionet e add-on-it lejohen
	long        daysLeft = 0;		// dite te mbetura (licence ose prove)
	bool        warn = false;		// licenca skadon se shpejti (<= 30 dite)
	LicenseInfo license;			// kur ka celes te vlefshem
	std::string keyError;			// celesi i ruajtur nuk u pranua
	Date        trialEnd;
	const char* StateName () const;
};

class License {
public:
	static constexpr int  kTrialDays = 14;
	static constexpr int  kWarnDays = 30;

	// Verifikon nenshkrimin dhe lexon permbajtjen (pa e krahasuar me daten).
	static LicenseInfo Parse (const std::string& key);

	// Gjendja per sot: celesi i ruajtur (mund te jete bosh) dhe data e nisjes se pare.
	static LicenseStatus Evaluate (const std::string& key, const Date& firstRun, const Date& today);

	// Ed25519 (verifikim) - publik per testet
	static bool Verify (const unsigned char sig[64], const unsigned char* msg, size_t len, const unsigned char pk[32]);
	static void Sha512 (const unsigned char* msg, size_t len, unsigned char out[64]);
	static std::string Base64UrlDecode (const std::string& s);

	// kontakti i shfaqur kur licenca mungon
	static const char* ContactName ()  { return "ARBS Studio"; }
	static const char* ContactEmail () { return "service@arbsstudio.com"; }
	static const char* ContactPhone () { return "+38345591070"; }
};

}	// namespace ARBS
