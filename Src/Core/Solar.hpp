// =============================================================================
// ARBS Studio - Core : Solar.hpp
// Pozicioni i diellit (algoritmi NOAA), lindja/perendimi, oret e insolimit
// teorik te fasadave dhe gjatesia e hijeve. Ora lokale e Kosoves: CET/CEST.
// =============================================================================
#pragma once

#include "Date.hpp"

#include <string>
#include <vector>

namespace ARBS {

struct SunPosition {
	double azimuth = 0.0;	// grade nga Veriu, ne drejtim te akrepave te ores
	double altitude = 0.0;	// grade mbi horizont (me refraksion)
};

struct SunDay {
	Date   date;
	double sunrise = 0.0;	// ore lokale (dhjetore)
	double sunset = 0.0;
	double solarNoon = 0.0;
	double dayLength = 0.0;	// ore
	double noonAltitude = 0.0;
	double riseAzimuth = 0.0, setAzimuth = 0.0;
	bool   dst = false;
};

struct Site {
	std::string name;
	double latitude = 42.6629;	// Prishtine
	double longitude = 21.1655;
	double altitude = 652.0;
	double baseUtcOffset = 1.0;	// CET
	bool   useEuDst = true;
};

class Solar {
public:
	static bool        IsEuDst (const Date& d, double localHour, double baseOffset);
	static double      UtcOffset (const Date& d, double localHour, const Site& s);
	static SunPosition Position (const Site& s, const Date& d, double localHour);
	static SunDay      Day (const Site& s, const Date& d);

	// Ore insolimi teorik ne nje fasade me azimut normal "facadeAzimuth"
	// (diell mbi "minAltitude" dhe kendi me normalen < 90 - "minIncidence")
	static double FacadeSunHours (const Site& s, const Date& d, double facadeAzimuth,
								  double minAltitude = 5.0, double minIncidence = 0.0, double stepMinutes = 5.0);

	// gjatesia e hijes per lartesi H ne oren e dhene (0 nese dielli nen horizont)
	static double ShadowLength (double height, double altitudeDeg);

	static std::string FormatHour (double h);			// "07:24"
	static const char* OrientationName (double azimuth);	// "Jug", "Juglindje" ...
};

}	// namespace ARBS
