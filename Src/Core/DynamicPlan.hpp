// =============================================================================
// ARBS Studio - Core : DynamicPlan.hpp
// Plani dinamik (Gantt) i punimeve me kalendarin e punes te Kosoves.
// Varesite: FS (mbaron-fillon) dhe SS (fillon-fillon) me vonese ne dite pune.
// =============================================================================
#pragma once

#include "KosovoCalendar.hpp"
#include "Json.hpp"

#include <string>
#include <vector>

namespace ARBS {

struct PlanLink {
	std::string predecessor;
	std::string type = "FS";		// FS ose SS
	int         lag = 0;			// dite pune (mund te jete negative per mbivendosje)
};

struct PlanTask {
	std::string id;
	std::string wbs;				// p.sh. "3.2"
	std::string group;				// faza / grupi i punimeve
	std::string name;
	int         duration = 1;		// dite pune (0 = pike kontrolli / milestone)
	bool        weatherSensitive = false;
	std::string crew;
	std::string notes;
	std::vector<PlanLink> links;

	// rezultatet
	Date es, ef, ls, lf;
	int  totalFloat = 0;
	bool critical = false;
	int  calendarDays = 0;
};

struct PlanParameters {
	Date        start;
	double      grossArea = 300.0;	// m2 BTS
	int         floorsAbove = 2;	// kate mbi toke (perfshire perdhesen)
	int         floorsBelow = 0;	// bodrum
	bool        saturdayWorking = false;
	bool        winterStopForWeatherTasks = true;
	int         winterFromMonth = 12, winterFromDay = 15;
	int         winterToMonth = 2,  winterToDay = 28;
	std::vector<DateRange> collectiveLeave;	// pushime kolektive
	std::string projectName;
};

class DynamicPlan {
public:
	// Krijon detyrat nga shablloni (Data/plan_template.json) dhe parametrat e projektit
	static std::vector<PlanTask> FromTemplate (const Json::Value& tmpl, const PlanParameters& p);
	static std::vector<PlanTask> FromJson (const Json::Value& tasks);
	static Json::Value           ToJson (const std::vector<PlanTask>& tasks);

	// Llogarit datat (CPM) me kalendarin e dhene. Kthen "" ose mesazh gabimi (cikel etj.)
	static std::string Schedule (std::vector<PlanTask>& tasks, const KosovoCalendar& cal, const Date& projectStart);

	static void ConfigureCalendar (KosovoCalendar& cal, const PlanParameters& p);
	static Date ProjectFinish (const std::vector<PlanTask>& tasks);
};

}	// namespace ARBS
