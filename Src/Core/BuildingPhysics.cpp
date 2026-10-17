// =============================================================================
// ARBS Studio - Core : BuildingPhysics.cpp
// =============================================================================
#include "BuildingPhysics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ARBS {

static const double kDelta0 = 2.0e-10;	// kg/(m*s*Pa) - permeabiliteti i ajrit (EN ISO 13788)

Material Material::FromJson (const Json::Value& v)
{
	Material m;
	m.id = v.GetString ("id");
	m.name = v.GetString ("name");
	m.group = v.GetString ("group");
	m.lambda = v.GetNumber ("lambda", 1.0);
	m.density = v.GetNumber ("density", 1000.0);
	m.heatCapacity = v.GetNumber ("c", 1000.0);
	m.mu = v.GetNumber ("mu", 1.0);
	m.air = v.GetBool ("air", false);
	m.fill = v.GetString ("fill");
	m.priority = v.GetInt ("priority", 500);
	m.archicadId = v.GetString ("acId", m.id);
	return m;
}

double Composite::TotalThickness () const
{
	double t = 0.0;
	for (const Layer& l : layers) t += l.thickness;
	return t;
}

double BuildingPhysics::SurfaceRsi (HeatFlow f)
{
	switch (f) {
		case HeatFlow::Upward:   return 0.10;
		case HeatFlow::Downward: return 0.17;
		default:                 return 0.13;
	}
}

const char* BuildingPhysics::BoundaryName (BoundaryType b)
{
	switch (b) {
		case BoundaryType::Ground:   return "Kontakt me tok\xC3\xABn";
		case BoundaryType::Unheated: return "Hap\xC3\xABsir\xC3\xAB e pangrohur";
		case BoundaryType::Heated:   return "Hap\xC3\xABsir\xC3\xAB e ngrohur";
		default:                     return "Ajri i jasht\xC3\xABm";
	}
}

const char* BuildingPhysics::FlowName (HeatFlow f)
{
	switch (f) {
		case HeatFlow::Upward:   return "lart";
		case HeatFlow::Downward: return "posht\xC3\xAB";
		default:                 return "horizontal";
	}
}

UResult BuildingPhysics::ComputeU (const Composite& c, double umax)
{
	UResult r;
	r.rsi = SurfaceRsi (c.flow);
	bool hasVentilated = false;
	for (const Layer& l : c.layers) if (l.ventilated) hasVentilated = true;

	switch (c.boundary) {
		case BoundaryType::Ground:   r.rse = 0.0; break;
		case BoundaryType::Unheated:
		case BoundaryType::Heated:   r.rse = r.rsi; break;
		default:                     r.rse = hasVentilated ? r.rsi : 0.04; break;
	}

	double sum = r.rsi + r.rse;
	for (const Layer& l : c.layers) {
		double rl = 0.0;
		if (!l.ventilated && l.material != nullptr && l.material->lambda > 0.0 && !l.material->air)
			rl = l.thickness / l.material->lambda;
		else if (!l.ventilated && l.material != nullptr && l.material->air)
			rl = std::min (0.18, l.thickness / std::max (0.01, l.material->lambda));	// shtrese ajri e paventiluar (ISO 6946, tab. 8 e perafruar)
		r.layerR.push_back (rl);
		sum += rl;
		if (l.material != nullptr) {
			r.arealMass += l.thickness * l.material->density;
			if (!l.ventilated) r.sd += l.thickness * l.material->mu;
		}
	}
	r.rTotal = sum;
	r.u = sum > 0.0 ? 1.0 / sum : 0.0;
	r.umax = umax;
	r.compliant = (umax <= 0.0) || (r.u <= umax + 1e-9);
	return r;
}

double BuildingPhysics::SaturationPressure (double theta)
{
	if (theta >= 0.0)
		return 610.5 * std::exp (17.269 * theta / (237.3 + theta));
	return 610.5 * std::exp (21.875 * theta / (265.5 + theta));
}

GlaserResult BuildingPhysics::Glaser (const Composite& c, const GlaserClimate& cl)
{
	GlaserResult res;

	// shtresat nga brenda -> jashte, pa shtresat e ventiluara
	std::vector<const Layer*> layers;
	for (const Layer& l : c.layers)
		if (!l.ventilated && l.material != nullptr) layers.push_back (&l);
	if (c.insideIsLast) std::reverse (layers.begin (), layers.end ());

	if (c.boundary != BoundaryType::Exterior || layers.empty ()) {
		res.verdict = "Kontrolli i difuzionit nuk aplikohet p\xC3\xABr k\xC3\xABt\xC3\xAB kufi (" + std::string (BoundaryName (c.boundary)) + ").";
		res.ok = true;
		return res;
	}

	const double rsi = SurfaceRsi (c.flow);
	bool hasVentilated = false;
	for (const Layer& l : c.layers) if (l.ventilated) hasVentilated = true;
	const double rse = hasVentilated ? rsi : 0.04;

	double rTot = rsi + rse;
	double sdTot = 0.0;
	for (const Layer* l : layers) {
		rTot += l->thickness / std::max (1e-6, l->material->lambda);
		sdTot += l->thickness * l->material->mu;
	}
	const double q = (cl.thetaI - cl.thetaE) / rTot;
	const double pi = cl.phiI * SaturationPressure (cl.thetaI);
	const double pe = cl.phiE * SaturationPressure (cl.thetaE);

	// pikat ne ndarjet e shtresave
	double theta = cl.thetaI - q * rsi;
	double x = 0.0, sd = 0.0;
	res.surfaceTempInside = theta;
	res.fRsi = (theta - cl.thetaE) / (cl.thetaI - cl.thetaE);
	{
		GlaserInterface gi; gi.x = 0; gi.sdFromInside = 0; gi.theta = theta; gi.psat = SaturationPressure (theta); gi.label = "brenda";
		res.points.push_back (gi);
	}
	for (const Layer* l : layers) {
		theta -= q * l->thickness / std::max (1e-6, l->material->lambda);
		x += l->thickness;
		sd += l->thickness * l->material->mu;
		GlaserInterface gi; gi.x = x; gi.sdFromInside = sd; gi.theta = theta; gi.psat = SaturationPressure (theta); gi.label = l->material->name;
		res.points.push_back (gi);
	}
	for (GlaserInterface& g : res.points)
		g.p = (sdTot > 0.0) ? pi + (pe - pi) * g.sdFromInside / sdTot : pi;

	bool any = false;
	for (const GlaserInterface& g : res.points)
		if (g.p > g.psat + 0.5) any = true;

	if (!any) {
		res.condensation = false;
		res.ok = true;
		res.verdict = "Nuk paraqitet kondensim i avullit t\xC3\xAB ujit n\xC3\xAB konstruksion. Konstruksioni plot\xC3\xABson k\xC3\xABrkesat p\xC3\xABr difuzionin e avullit.";
		return res;
	}

	// tangjentet nga brenda dhe nga jashte ne kurben psat(sd)
	const size_t n = res.points.size ();
	int k1 = -1, k2 = -1;
	double best1 = 1e300, best2 = 1e300;
	for (size_t j = 1; j < n; ++j) {
		const double s = res.points[j].sdFromInside;
		if (s <= 1e-9) continue;
		const double slope = (res.points[j].psat - pi) / s;
		if (slope < best1) { best1 = slope; k1 = static_cast<int> (j); }
	}
	for (size_t j = 0; j + 1 < n; ++j) {
		const double s = sdTot - res.points[j].sdFromInside;
		if (s <= 1e-9) continue;
		const double slope = (res.points[j].psat - pe) / s;
		if (slope < best2) { best2 = slope; k2 = static_cast<int> (j); }
	}
	if (k1 < 0 || k2 < 0) { k1 = k2 = static_cast<int> (n) - 1; }
	if (k1 > k2) std::swap (k1, k2);

	const double sd1 = std::max (1e-6, res.points[k1].sdFromInside);
	const double sd2 = std::max (1e-6, sdTot - res.points[k2].sdFromInside);
	const double gIn  = kDelta0 * (pi - res.points[k1].psat) / sd1;
	const double gOut = kDelta0 * (res.points[k2].psat - pe) / sd2;
	const double g = gIn - gOut;
	if (g <= 0.0) {
		res.condensation = false;
		res.ok = true;
		res.verdict = "Presioni i avullit prek kurb\xC3\xABn e ngopjes por fluksi neto \xC3\xABsht\xC3\xAB zero - nuk ka akumulim lag\xC3\xABshtie.";
		return res;
	}
	res.condensation = true;
	res.planeFrom = k1;
	res.planeTo = k2;
	res.condensedKgM2 = g * cl.condDays * 86400.0;

	const double ps18 = SaturationPressure (cl.dryTheta);
	const double pside = cl.dryPhi * ps18;
	const double gDry = kDelta0 * ((ps18 - pside) / sd1 + (ps18 - pside) / sd2);
	res.dryingKgM2 = gDry * cl.dryDays * 86400.0;
	res.dries = res.dryingKgM2 >= res.condensedKgM2;
	res.ok = res.dries && res.condensedKgM2 <= 0.5;

	char buf[1024];
	std::snprintf (buf, sizeof (buf),
		"Kondensimi paraqitet n\xC3\xAB zon\xC3\xABn %s. Sasia e lag\xC3\xABshtis\xC3\xAB s\xC3\xAB kondensuar gjat\xC3\xAB %.0f dit\xC3\xABve: %.3f kg/m\xC2\xB2; "
		"sasia q\xC3\xAB mund t\xC3\xAB thahet gjat\xC3\xAB %.0f dit\xC3\xABve t\xC3\xAB ver\xC3\xABs: %.3f kg/m\xC2\xB2. %s",
		(k1 == k2) ? ("pas shtres\xC3\xABs \"" + res.points[k1].label + "\"").c_str ()
				   : ("nd\xC3\xABrmjet \"" + res.points[k1].label + "\" dhe \"" + res.points[k2].label + "\"").c_str (),
		cl.condDays, res.condensedKgM2, cl.dryDays, res.dryingKgM2,
		res.ok ? "Lag\xC3\xABshtia thahet plot\xC3\xABsisht - konstruksioni plot\xC3\xABson k\xC3\xABrkesat p\xC3\xABr difuzionin e avullit."
			   : "Konstruksioni NUK plot\xC3\xABson k\xC3\xABrkesat - rekomandohet barrier\xC3\xAB avulli n\xC3\xAB an\xC3\xABn e ngroht\xC3\xAB ose ndryshim i rendit t\xC3\xAB shtresave.");
	res.verdict = buf;
	return res;
}

}	// namespace ARBS
