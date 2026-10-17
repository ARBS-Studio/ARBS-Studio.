// =============================================================================
// ARBS Studio - Core : DynamicPlan.cpp
// =============================================================================
#include "DynamicPlan.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace ARBS {

namespace {

int EvalDuration (const Json::Value& d, const PlanParameters& p)
{
	if (d.IsNumber ()) return std::max (0, d.AsInt ());
	// { "base": 5, "perFloor": 4, "perBasement": 6, "per100m2": 1.5, "min": 2, "max": 60 }
	double v = d.GetNumber ("base", 1.0);
	v += d.GetNumber ("perFloor", 0.0) * std::max (0, p.floorsAbove);
	v += d.GetNumber ("perBasement", 0.0) * std::max (0, p.floorsBelow);
	v += d.GetNumber ("per100m2", 0.0) * p.grossArea / 100.0;
	int r = static_cast<int> (std::ceil (v - 1e-9));
	r = std::max (r, d.GetInt ("min", 0));
	if (d.Has ("max")) r = std::min (r, d.GetInt ("max"));
	return std::max (0, r);
}

bool Condition (const Json::Value& t, const PlanParameters& p)
{
	const std::string cond = t.GetString ("when");
	if (cond.empty ()) return true;
	if (cond == "basement")   return p.floorsBelow > 0;
	if (cond == "noBasement") return p.floorsBelow == 0;
	if (cond == "multiStorey") return p.floorsAbove > 1;
	return true;
}

}	// namespace

void DynamicPlan::ConfigureCalendar (KosovoCalendar& cal, const PlanParameters& p)
{
	cal.SetSaturdayWorking (p.saturdayWorking);
	cal.ClearNonWorkingRanges ();
	for (const DateRange& r : p.collectiveLeave)
		cal.AddNonWorkingRange (r);
	if (p.winterStopForWeatherTasks) {
		// periudha dimerore per 10 vite nga fillimi
		for (int y = p.start.y - 1; y <= p.start.y + 10; ++y) {
			DateRange r;
			r.from = Date (y, p.winterFromMonth, std::min (p.winterFromDay, DaysInMonth (y, p.winterFromMonth)));
			const int toYear = (p.winterToMonth < p.winterFromMonth) ? y + 1 : y;
			r.to = Date (toYear, p.winterToMonth, std::min (p.winterToDay, DaysInMonth (toYear, p.winterToMonth)));
			r.name = "Periudha dim\xC3\xABrore (punime t\xC3\xAB ndjeshme ndaj motit)";
			r.weatherOnly = true;
			cal.AddNonWorkingRange (r);
		}
	}
}

std::vector<PlanTask> DynamicPlan::FromTemplate (const Json::Value& tmpl, const PlanParameters& p)
{
	std::vector<PlanTask> tasks;
	std::map<std::string, bool> included;
	int groupNo = 0;
	for (const Json::Value& g : tmpl["groups"].AsArray ()) {
		++groupNo;
		int taskNo = 0;
		for (const Json::Value& t : g["tasks"].AsArray ()) {
			if (!Condition (t, p)) continue;
			++taskNo;
			PlanTask task;
			task.id = t.GetString ("id");
			task.wbs = std::to_string (groupNo) + "." + std::to_string (taskNo);
			task.group = g.GetString ("name");
			task.name = t.GetString ("name");
			task.duration = EvalDuration (t["duration"], p);
			task.weatherSensitive = t.GetBool ("weather", false);
			task.crew = t.GetString ("crew");
			task.notes = t.GetString ("notes");
			for (const Json::Value& l : t["after"].AsArray ()) {
				PlanLink link;
				if (l.IsString ()) {
					link.predecessor = l.AsString ();
				} else {
					link.predecessor = l.GetString ("id");
					link.type = l.GetString ("type", "FS");
					link.lag = l.GetInt ("lag", 0);
				}
				task.links.push_back (link);
			}
			included[task.id] = true;
			tasks.push_back (task);
		}
	}
	// largo varesite ndaj detyrave te perjashtuara (p.sh. bodrumi), trashego varesite e tyre
	std::map<std::string, std::vector<PlanLink>> removedLinks;
	for (const Json::Value& g : tmpl["groups"].AsArray ())
		for (const Json::Value& t : g["tasks"].AsArray ())
			if (!Condition (t, p))
				for (const Json::Value& l : t["after"].AsArray ())
					removedLinks[t.GetString ("id")].push_back ({ l.IsString () ? l.AsString () : l.GetString ("id"), "FS", 0 });

	for (PlanTask& task : tasks) {
		std::vector<PlanLink> fixedLinks;
		std::vector<PlanLink> queue = task.links;
		int guard = 0;
		while (!queue.empty () && ++guard < 1000) {
			PlanLink l = queue.back ();
			queue.pop_back ();
			if (included.count (l.predecessor) > 0) { fixedLinks.push_back (l); continue; }
			for (const PlanLink& inherited : removedLinks[l.predecessor])
				queue.push_back (inherited);
		}
		task.links = fixedLinks;
	}
	return tasks;
}

std::vector<PlanTask> DynamicPlan::FromJson (const Json::Value& arr)
{
	std::vector<PlanTask> tasks;
	for (const Json::Value& t : arr.AsArray ()) {
		PlanTask task;
		task.id = t.GetString ("id");
		task.wbs = t.GetString ("wbs");
		task.group = t.GetString ("group");
		task.name = t.GetString ("name");
		task.duration = std::max (0, t.GetInt ("duration", 1));
		task.weatherSensitive = t.GetBool ("weather", false);
		task.crew = t.GetString ("crew");
		task.notes = t.GetString ("notes");
		for (const Json::Value& l : t["links"].AsArray ())
			task.links.push_back ({ l.GetString ("id"), l.GetString ("type", "FS"), l.GetInt ("lag", 0) });
		tasks.push_back (task);
	}
	return tasks;
}

Json::Value DynamicPlan::ToJson (const std::vector<PlanTask>& tasks)
{
	Json::Value arr = Json::Value::MakeArray ();
	for (const PlanTask& t : tasks) {
		Json::Value o = Json::Value::MakeObject ();
		o.Set ("id", t.id);
		o.Set ("wbs", t.wbs);
		o.Set ("group", t.group);
		o.Set ("name", t.name);
		o.Set ("duration", t.duration);
		o.Set ("weather", t.weatherSensitive);
		o.Set ("crew", t.crew);
		o.Set ("notes", t.notes);
		Json::Value links = Json::Value::MakeArray ();
		for (const PlanLink& l : t.links) {
			Json::Value lo = Json::Value::MakeObject ();
			lo.Set ("id", l.predecessor);
			lo.Set ("type", l.type);
			lo.Set ("lag", l.lag);
			links.Push (lo);
		}
		o.Set ("links", links);
		o.Set ("start", t.es.ToIso ());
		o.Set ("finish", t.ef.ToIso ());
		o.Set ("float", t.totalFloat);
		o.Set ("critical", t.critical);
		arr.Push (o);
	}
	return arr;
}

// Shton n dite pune (n mund te jete negative) duke filluar nga dita e punes d
static Date ShiftWorking (const KosovoCalendar& cal, const Date& d, int n, bool weather)
{
	Date x = d;
	if (n > 0) {
		for (int i = 0; i < n; ++i) x = cal.NextWorkingDay (x.AddDays (1), weather);
	} else if (n < 0) {
		for (int i = 0; i < -n; ++i) x = cal.PrevWorkingDay (x.AddDays (-1), weather);
	}
	return x;
}

std::string DynamicPlan::Schedule (std::vector<PlanTask>& tasks, const KosovoCalendar& cal, const Date& projectStart)
{
	std::map<std::string, size_t> index;
	for (size_t i = 0; i < tasks.size (); ++i) {
		if (tasks[i].id.empty ()) tasks[i].id = "T" + std::to_string (i + 1);
		index[tasks[i].id] = i;
	}

	// renditja topologjike (Kahn)
	std::vector<int> indeg (tasks.size (), 0);
	std::vector<std::vector<size_t>> succ (tasks.size ());
	for (size_t i = 0; i < tasks.size (); ++i) {
		for (const PlanLink& l : tasks[i].links) {
			auto it = index.find (l.predecessor);
			if (it == index.end ())
				return "Detyra \"" + tasks[i].name + "\" ka paraardh\xC3\xABs t\xC3\xAB panjohur: " + l.predecessor;
			succ[it->second].push_back (i);
			++indeg[i];
		}
	}
	std::vector<size_t> order;
	std::vector<size_t> ready;
	for (size_t i = 0; i < tasks.size (); ++i) if (indeg[i] == 0) ready.push_back (i);
	while (!ready.empty ()) {
		std::sort (ready.begin (), ready.end (), std::greater<size_t> ());
		size_t i = ready.back ();
		ready.pop_back ();
		order.push_back (i);
		for (size_t s : succ[i]) if (--indeg[s] == 0) ready.push_back (s);
	}
	if (order.size () != tasks.size ())
		return "Varesite e detyrave formojn\xC3\xAB nj\xC3\xAB cik\xC3\xABl (rrethor). Kontrolloni lidhjet.";

	// kalimi perpara
	for (size_t i : order) {
		PlanTask& t = tasks[i];
		Date es = cal.NextWorkingDay (projectStart, t.weatherSensitive);
		for (const PlanLink& l : t.links) {
			const PlanTask& p = tasks[index[l.predecessor]];
			Date candidate;
			if (l.type == "SS") {
				candidate = ShiftWorking (cal, cal.NextWorkingDay (p.es, t.weatherSensitive), l.lag, t.weatherSensitive);
			} else {	// FS
				const Date base = (p.duration == 0) ? p.ef.AddDays (-1) : p.ef;
				candidate = cal.StartAfter (base, 0, t.weatherSensitive);
				candidate = ShiftWorking (cal, candidate, l.lag, t.weatherSensitive);
			}
			if (es < candidate) es = candidate;
		}
		t.es = cal.NextWorkingDay (es, t.weatherSensitive);
		t.ef = (t.duration == 0) ? t.es : cal.FinishDate (t.es, t.duration, t.weatherSensitive);
		t.calendarDays = static_cast<int> (t.ef.ToDays () - t.es.ToDays () + 1);
	}

	// kalimi prapa (kalendari i pergjithshem) per rrugen kritike
	Date projectEnd = ProjectFinish (tasks);
	for (PlanTask& t : tasks) { t.lf = projectEnd; }
	for (auto it = order.rbegin (); it != order.rend (); ++it) {
		PlanTask& t = tasks[*it];
		Date lf = projectEnd;
		bool hasSucc = false;
		for (size_t s : succ[*it]) {
			const PlanTask& st = tasks[s];
			for (const PlanLink& l : st.links) {
				if (l.predecessor != t.id) continue;
				Date limit;
				if (l.type == "SS") {
					// LS(t) <= LS(s) - lag  =>  LF(t) = LS(t) + dur - 1
					Date ls = ShiftWorking (cal, st.ls, -l.lag, false);
					limit = (t.duration == 0) ? ls : ShiftWorking (cal, ls, t.duration - 1, false);
				} else {
					Date ls = ShiftWorking (cal, st.ls, -l.lag, false);
					limit = cal.PrevWorkingDay (ls.AddDays (-1), false);
					if (t.duration == 0) limit = ls;
				}
				if (!hasSucc || limit < lf) lf = limit;
				hasSucc = true;
			}
		}
		t.lf = lf;
		t.ls = (t.duration == 0) ? lf : ShiftWorking (cal, lf, -(t.duration - 1), false);
		const int fl = cal.CountWorkingDays (t.ef, t.lf, false) - 1;
		t.totalFloat = (t.lf < t.ef) ? 0 : std::max (0, fl);
		t.critical = t.totalFloat == 0;
	}
	return "";
}

Date DynamicPlan::ProjectFinish (const std::vector<PlanTask>& tasks)
{
	Date end;
	bool first = true;
	for (const PlanTask& t : tasks) {
		if (first || end < t.ef) end = t.ef;
		first = false;
	}
	return end;
}

}	// namespace ARBS
