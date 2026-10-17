// =============================================================================
// ARBS Studio - Core : BuildingPhysics.hpp
// - Koeficienti i kalimit te nxehtesise U sipas SK EN ISO 6946
// - Kontrolli kundrejt Umax te Rregullores MMPH Nr. 04/18 (Kosove)
// - Difuzioni i avullit te ujit - metoda Glaser (periudha e kondensimit 60 dite,
//   periudha e tharjes 90 dite), si ne praktiken e projektimit ne Kosove
// =============================================================================
#pragma once

#include "Json.hpp"

#include <string>
#include <vector>

namespace ARBS {

struct Material {
	std::string id;
	std::string name;
	std::string group;
	double lambda = 1.0;			// W/(mK)
	double density = 1000.0;		// kg/m3
	double heatCapacity = 1000.0;	// J/(kgK)
	double mu = 1.0;				// faktori i rezistences ndaj difuzionit te avullit [-]
	bool   air = false;				// shtrese ajri / e ventiluar
	std::string fill;				// emri i preferuar i shrafures ne Archicad
	int    priority = 500;			// prioriteti i nderprerjes 0..999
	std::string archicadId;			// ID ne Archicad (p.sh. "ARBS-BET-01")
	static Material FromJson (const Json::Value& v);
};

struct Layer {
	const Material* material = nullptr;
	double thickness = 0.0;		// m
	bool   core = false;		// berthama (konstruktive)
	bool   finish = false;		// shtrese perfundimtare
	bool   ventilated = false;	// jashte shtreses se ventiluar: nuk merret ne U
};

enum class HeatFlow { Horizontal, Upward, Downward };

enum class BoundaryType {
	Exterior,		// ajri i jashtem
	Ground,			// kontakt me token (Rse = 0)
	Unheated,		// hapesire e pangrohur (Rse = Rsi)
	Heated			// hapesire e ngrohur (ndarese)
};

struct Composite {
	std::string code;			// p.sh. "MJ-01"
	std::string name;
	std::string usage;			// "wall", "slab", "roof"
	std::string element;		// kategoria per Umax (shih umaxKey)
	std::string umaxKey;		// celesi ne tabelen Umax
	std::string description;
	HeatFlow    flow = HeatFlow::Horizontal;
	BoundaryType boundary = BoundaryType::Exterior;
	std::vector<Layer> layers;	// nga jashte -> brenda (mure) / nga lart -> poshte (pllaka, kulme)
	bool insideIsLast = true;	// per pllaka/kulme: ana e ngrohte (brenda) eshte poshte
	double TotalThickness () const;
};

struct UResult {
	double rsi = 0.13, rse = 0.04;
	double rTotal = 0.0;
	double u = 0.0;
	double umax = 0.0;				// 0 nese nuk aplikohet
	bool   compliant = true;
	std::vector<double> layerR;		// R per cdo shtrese (0 per shtresat e ventiluara)
	double arealMass = 0.0;			// kg/m2
	double sd = 0.0;				// m (shuma e trashesive ekuivalente te ajrit)
};

struct GlaserClimate {
	double thetaI = 20.0, phiI = 0.60;	// brenda
	double thetaE = -10.0, phiE = 0.90;	// jashte, periudha e kondensimit
	double condDays = 60.0;
	double dryTheta = 18.0, dryPhi = 0.65;	// periudha e tharjes (te dyja anet)
	double dryDays = 90.0;
};

struct GlaserInterface {
	double x = 0.0;			// pozicioni nga ana e brendshme [m]
	double sdFromInside = 0.0;
	double theta = 0.0;
	double psat = 0.0;
	double p = 0.0;			// presioni i pjesshem (pa kondensim)
	std::string label;
};

struct GlaserResult {
	std::vector<GlaserInterface> points;	// siperfaqja e brendshme -> e jashtme
	bool   condensation = false;
	int    planeFrom = -1, planeTo = -1;	// indekset e ndarjeve ku kondensohet
	double condensedKgM2 = 0.0;				// gjate periudhes se kondensimit
	double dryingKgM2 = 0.0;				// kapaciteti i tharjes gjate veres
	bool   dries = true;
	bool   ok = true;
	double surfaceTempInside = 0.0;		// temperatura e siperfaqes se brendshme
	double fRsi = 0.0;					// faktori i temperatures (rrezik mykut nese < 0.75)
	std::string verdict;
};

class BuildingPhysics {
public:
	static double SurfaceRsi (HeatFlow f);
	static UResult ComputeU (const Composite& c, double umax);
	static GlaserResult Glaser (const Composite& c, const GlaserClimate& climate);
	static double SaturationPressure (double theta);	// Pa, EN ISO 13788
	static const char* BoundaryName (BoundaryType b);
	static const char* FlowName (HeatFlow f);
};

}	// namespace ARBS
