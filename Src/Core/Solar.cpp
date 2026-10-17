// =============================================================================
// ARBS Studio - Core : Solar.cpp  (NOAA Solar Calculator)
// =============================================================================
#include "Solar.hpp"

#include <cmath>
#include <cstdio>

namespace ARBS {

static const double kPi = 3.14159265358979323846;
static double Rad (double d) { return d * kPi / 180.0; }
static double Deg (double r) { return r * 180.0 / kPi; }

static Date LastSunday (int year, int month)
{
	Date d (year, month, DaysInMonth (year, month));
	while (d.Weekday () != 6) d = d.AddDays (-1);
	return d;
}

bool Solar::IsEuDst (const Date& d, double localHour, double baseOffset)
{
	// BE: nga e diela e fundit e marsit 01:00 UTC deri te e diela e fundit e tetorit 01:00 UTC
	const Date start = LastSunday (d.y, 3);
	const Date end = LastSunday (d.y, 10);
	const double utcHour = localHour - baseOffset;
	const double t = static_cast<double> (d.ToDays ()) + utcHour / 24.0;
	const double t0 = static_cast<double> (start.ToDays ()) + 1.0 / 24.0;
	const double t1 = static_cast<double> (end.ToDays ()) + 1.0 / 24.0;
	return t >= t0 && t < t1;
}

double Solar::UtcOffset (const Date& d, double localHour, const Site& s)
{
	return s.baseUtcOffset + ((s.useEuDst && IsEuDst (d, localHour, s.baseUtcOffset)) ? 1.0 : 0.0);
}

namespace {

struct SolarCore {
	double declination;	// rad
	double eqTime;		// minuta
};

SolarCore Core (double julianCentury)
{
	const double T = julianCentury;
	const double L0 = std::fmod (280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);
	const double M = 357.52911 + T * (35999.05029 - 0.0001537 * T);
	const double e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
	const double C = std::sin (Rad (M)) * (1.914602 - T * (0.004817 + 0.000014 * T))
				   + std::sin (Rad (2 * M)) * (0.019993 - 0.000101 * T)
				   + std::sin (Rad (3 * M)) * 0.000289;
	const double trueLong = L0 + C;
	const double omega = 125.04 - 1934.136 * T;
	const double lambda = trueLong - 0.00569 - 0.00478 * std::sin (Rad (omega));
	const double eps0 = 23.0 + (26.0 + ((21.448 - T * (46.815 + T * (0.00059 - T * 0.001813)))) / 60.0) / 60.0;
	const double eps = eps0 + 0.00256 * std::cos (Rad (omega));
	SolarCore sc;
	sc.declination = std::asin (std::sin (Rad (eps)) * std::sin (Rad (lambda)));
	const double y = std::tan (Rad (eps / 2.0)) * std::tan (Rad (eps / 2.0));
	sc.eqTime = 4.0 * Deg (y * std::sin (2.0 * Rad (L0)) - 2.0 * e * std::sin (Rad (M))
						 + 4.0 * e * y * std::sin (Rad (M)) * std::cos (2.0 * Rad (L0))
						 - 0.5 * y * y * std::sin (4.0 * Rad (L0)) - 1.25 * e * e * std::sin (2.0 * Rad (M)));
	return sc;
}

double JulianCentury (const Date& d, double utcHour)
{
	const double jd = static_cast<double> (d.ToDays ()) + 2440587.5 + utcHour / 24.0;
	return (jd - 2451545.0) / 36525.0;
}

double Refraction (double elev)
{
	if (elev > 85.0) return 0.0;
	const double te = std::tan (Rad (elev));
	double r;
	if (elev > 5.0)        r = 58.1 / te - 0.07 / (te * te * te) + 0.000086 / std::pow (te, 5);
	else if (elev > -0.575) r = 1735.0 + elev * (-518.2 + elev * (103.4 + elev * (-12.79 + elev * 0.711)));
	else                    r = -20.774 / te;
	return r / 3600.0;
}

}	// namespace

SunPosition Solar::Position (const Site& s, const Date& d, double localHour)
{
	const double tz = UtcOffset (d, localHour, s);
	const double utc = localHour - tz;
	const SolarCore sc = Core (JulianCentury (d, utc));
	const double trueSolarTime = std::fmod (localHour * 60.0 + sc.eqTime + 4.0 * s.longitude - 60.0 * tz + 1440.0 * 4, 1440.0);
	double hourAngle = trueSolarTime / 4.0 - 180.0;
	if (hourAngle < -180.0) hourAngle += 360.0;

	const double lat = Rad (s.latitude);
	const double cosZen = std::sin (lat) * std::sin (sc.declination) + std::cos (lat) * std::cos (sc.declination) * std::cos (Rad (hourAngle));
	const double zenith = Deg (std::acos (std::fmax (-1.0, std::fmin (1.0, cosZen))));
	const double elev = 90.0 - zenith;

	double az;
	const double denom = std::cos (lat) * std::sin (Rad (zenith));
	if (std::fabs (denom) > 1e-9) {
		double azRad = ((std::sin (lat) * std::cos (Rad (zenith))) - std::sin (sc.declination)) / denom;
		azRad = std::fmax (-1.0, std::fmin (1.0, azRad));
		if (hourAngle > 0.0) az = std::fmod (Deg (std::acos (azRad)) + 180.0, 360.0);
		else                 az = std::fmod (540.0 - Deg (std::acos (azRad)), 360.0);
	} else {
		az = (s.latitude > 0.0) ? 180.0 : 0.0;
	}
	SunPosition p;
	p.azimuth = az;
	p.altitude = elev + Refraction (elev);
	return p;
}

SunDay Solar::Day (const Site& s, const Date& d)
{
	SunDay day;
	day.date = d;
	day.dst = s.useEuDst && IsEuDst (d, 12.0, s.baseUtcOffset);
	const double tz = s.baseUtcOffset + (day.dst ? 1.0 : 0.0);

	// iterim i thjeshte: vlera ne mesdite lokale
	const SolarCore sc = Core (JulianCentury (d, 12.0 - tz));
	const double lat = Rad (s.latitude);
	const double zen = Rad (90.833);
	double cosHa = std::cos (zen) / (std::cos (lat) * std::cos (sc.declination)) - std::tan (lat) * std::tan (sc.declination);
	const double noonMin = 720.0 - 4.0 * s.longitude - sc.eqTime + tz * 60.0;
	day.solarNoon = noonMin / 60.0;
	if (cosHa > 1.0) {				// nate polare
		day.sunrise = day.sunset = day.solarNoon;
		day.dayLength = 0.0;
	} else if (cosHa < -1.0) {		// dite polare
		day.sunrise = 0.0; day.sunset = 24.0; day.dayLength = 24.0;
	} else {
		const double ha = Deg (std::acos (cosHa));
		day.sunrise = (noonMin - 4.0 * ha) / 60.0;
		day.sunset = (noonMin + 4.0 * ha) / 60.0;
		day.dayLength = 8.0 * ha / 60.0;
	}
	day.noonAltitude = Position (s, d, day.solarNoon).altitude;
	day.riseAzimuth = Position (s, d, day.sunrise + 0.02).azimuth;
	day.setAzimuth = Position (s, d, day.sunset - 0.02).azimuth;
	return day;
}

double Solar::FacadeSunHours (const Site& s, const Date& d, double facadeAzimuth, double minAltitude, double minIncidence, double stepMinutes)
{
	double minutes = 0.0;
	for (double t = 0.0; t < 24.0 * 60.0; t += stepMinutes) {
		const SunPosition p = Position (s, d, (t + stepMinutes / 2.0) / 60.0);
		if (p.altitude < minAltitude) continue;
		double diff = std::fabs (std::fmod (p.azimuth - facadeAzimuth + 540.0, 360.0) - 180.0);
		if (diff < 90.0 - minIncidence) minutes += stepMinutes;
	}
	return minutes / 60.0;
}

double Solar::ShadowLength (double height, double altitudeDeg)
{
	if (altitudeDeg <= 0.5) return 0.0;
	return height / std::tan (Rad (altitudeDeg));
}

std::string Solar::FormatHour (double h)
{
	if (h < 0.0) h += 24.0;
	int total = static_cast<int> (std::lround (h * 60.0));
	char buf[16];
	std::snprintf (buf, sizeof (buf), "%02d:%02d", (total / 60) % 24, total % 60);
	return buf;
}

const char* Solar::OrientationName (double az)
{
	static const char* names[] = { "Veri", "Verilindje", "Lindje", "Juglindje", "Jug", "Jugper\xC3\xABndim", "Per\xC3\xABndim", "Veriper\xC3\xABndim" };
	int idx = static_cast<int> (std::floor (std::fmod (az + 22.5 + 360.0, 360.0) / 45.0)) % 8;
	return names[idx];
}

}	// namespace ARBS
