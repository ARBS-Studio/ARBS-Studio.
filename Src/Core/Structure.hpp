// =============================================================================
// ARBS Studio - Core : Structure.hpp
// Struktura e projekteve me disa blloqe/hyrje: Kompleksi -> Blloku -> Hyrja -> Kati -> Njesia.
//
// Si shenohet modeli ne Archicad (zona me kategori ose emer):
//   Blloku : zone me kategori "ARBS Blloku" ose emer "Blloku ..." ; numri i zones = kodi (B1, B2 ...)
//   Hyrja  : zone me kategori "ARBS Hyrja"  ose emer "Hyrja ..."  ; numri = kodi (H1, H2 ...)
//   Njesia : zone me kategori "ARBS Njesia" ose emer "Banesa ...", "Lokal ...", "Zyre ..." ;
//            numri = nr. i njesise; poligoni = siperfaqja bruto e njesise
//   Dhomat : zonat e zakonshme; i takojne njesise ku bie pika e etiketes
// Elementet (mure, pllaka, kolona ...) i caktohen bllokut ku bie qendra e tyre ne plan.
// Katet nentoke mund te jene "pjese e perbashket" (garazh/podium nen disa blloqe).
// =============================================================================
#pragma once

#include "Json.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ARBS {

struct Pt { double x = 0.0, y = 0.0; };

namespace Geo {
	bool   PointInPolygon (const std::vector<Pt>& poly, double x, double y);
	double Area (const std::vector<Pt>& poly);
	std::vector<Pt> FromJson (const Json::Value& arr);	// [[x,y], ...]
	Json::Value     ToJson (const std::vector<Pt>& poly);
}

struct StructureOptions {
	bool   sharedBasement = true;		// katet nentoke = pjese e perbashket
	double kBalcony = 1.0;				// koeficientet e siperfaqeve te jashtme (i vendos projektuesi)
	double kLoggia = 1.0;
	double kTerrace = 1.0;
	bool   coefficientsSet = false;		// false = projektuesi nuk i ka vendosur (perdoret 1,00)
};

class Structure {
public:
	static const char* CommonPart () { return "PB"; }	// kodi i pjeses se perbashket

	// Klasifikimi i zonave: "block" | "entrance" | "unit" | "room"
	static std::string ZoneRole (const std::string& name, const std::string& category);
	static std::string UnitKind (const std::string& name);	// banesa, lokal, zyre, garazh, depo, parking, tjeter
	static std::string UnitKindLabel (const std::string& kind);
	static std::string OuterKind (const std::string& roomName);	// ballkon, lozhe, terase ose ""
	static bool        IsBedroom (const std::string& roomName);

	static StructureOptions OptionsFromProject (const Json::Value& project);

	// Kodi i bllokut ku bie pika (model["blocks"]); "" nese jashte
	static std::string BlockAt (const Json::Value& blocks, double x, double y);
	static std::string EntranceAt (const Json::Value& entrances, double x, double y);

	// Pjesa: kodi i bllokut, ose "PB" (nentoke e perbashket / jashte blloqeve)
	static std::string PartOf (const std::string& block, int story, bool hasBlocks, bool sharedBasement);

	static bool HasBlocks (const Json::Value& model);

	// Ndan modelin ne pjese: [("B1", model_B1), ("B2", ...), ("PB", ...)].
	// Pa blloqe: nje pjese e vetme me kod "".
	static std::vector<std::pair<std::string, Json::Value>> SplitModel (const Json::Value& model, const StructureOptions& opt);

	// Njesite me dhomat, siperfaqet dhe pjesen e hapesirave te perbashketa.
	// Nese modeli nuk ka zona njesish, perdoren njesite e percaktuara me dore (manual).
	static Json::Value Units (const Json::Value& model, const StructureOptions& opt, const Json::Value& manual = Json::Value ());

	// Njesite me dore: tipet e banesave dhe vendosja e tyre ne blloqe/hyrje/kate:
	//   { "types": [ { "code": "A", "kind": "banesa", "name": "Banesa tip A",
	//                  "rooms": [ { "name": "Qendrimi ditor", "area": 24.5 }, ... ], "gross": 78.0 } ],
	//     "placements": [ { "type": "A", "block": "B1", "entrance": "H1", "floors": "1-7", "perFloor": 2 } ] }
	static Json::Value ManualUnits (const Json::Value& manual, const StructureOptions& opt);
	static std::vector<int> ParseFloors (const std::string& spec);	// "1-7", "1,3,5", "P" = 0

	// Emri i bllokut (nga model["blocks"]) per kodin
	static std::string BlockName (const Json::Value& model, const std::string& code);
};

}	// namespace ARBS
