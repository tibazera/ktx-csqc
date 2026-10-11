/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  $Id$
 */

#include "g_local.h"

void NextLevel(void);
void IdlebotForceStart(void);
void StartMatch(void);
void remove_specs_wizards(void);
void lastscore_add(void);
void StartLogs(void);
void StopLogs(void);
void ClearDemoMarkers(void);

void EM_CorrectStats(void);
void MatchEndStats(void);
void SM_PrepareTeamsStats(void);
void SM_PrepareShowscores(void);
static void SM_ExecuteQueuedSpawnEffects(void);

void race_match_start(void);
qbool race_can_cancel_demo(void);

extern int g_matchstarttime;
qbool initial_match_spawns;
int maxPlayerCount;

// Return count of players which have state cs_connected or cs_spawned.
// It is weird because used string comparision so I treat it as slow and idiotic but it return more players than CountPlayers().
int WeirdCountPlayers(void)
{
	gedict_t *p;
	int num;
	char state[16];

	for (num = 0, p = world + 1; p <= world + MAX_CLIENTS; p++)
	{
		infokey(p, "*spectator", state, sizeof(state));

		if (state[0])
		{
			continue; // ignore spectators
		}

		infokey(p, "*state", state, sizeof(state));

		if (streq(state, "connected") || streq(state, "spawned"))
		{
			num++;
		}
	}

	return num;
}

float CountPlayers(void)
{
	gedict_t *p;
	float num = 0;

	for (p = world; (p = find_plr(p));)
	{
		num++;
	}

	return num;
}

float CountBots(void)
{
	gedict_t *p;
	float num = 0;

	for (p = world; (p = find_plr(p));)
	{
		if (p->isBot)
		{
			num++;
		}
	}

	return num;
}

float CountRPlayers(void)
{
	gedict_t *p;
	float num = 0;

	for (p = world; (p = find_plr(p));)
	{
		if (p->ready)
		{
			num++;
		}
	}

	return num;
}

float CountTeams(void)
{
	gedict_t *p, *p2;
	float num = 0;
	char *s = "";

	for (p = world; (p = find_plr(p));)
	{
		p->k_flag = 0;
	}

	for (p = world; (p = find_plr(p));)
	{
		if (p->k_flag || strnull(s = getteam(p)))
		{
			continue;
		}

		p->k_flag = 1;
		num++;

		for (p2 = p; (p2 = find_plr(p2));)
		{
			if (streq(s, getteam(p2)))
			{
				p2->k_flag = 1;
			}
		}
	}
	return num;
}

// return number of teams where at least one member is ready?
float CountRTeams(void)
{
	gedict_t *p, *p2;
	float num = 0;
	char *s = "";

	for (p = world; (p = find_plr(p));)
	{
		p->k_flag = 0;
	}

	for (p = world; (p = find_plr(p));)
	{
		if (p->k_flag || !p->ready || strnull(s = getteam(p)))
		{
			continue;
		}

		p->k_flag = 1;
		num++;

		for (p2 = p; (p2 = find_plr(p2));)
		{
			if (streq(s, getteam(p2)))
			{
				p2->k_flag = 1;
			}
		}
	}

	return num;
}

// count the members in each team and store the max in maxPlayerCount
// and return 0 if at least one team has less members than 'memcnt'
// else return 1 (even we have more members than memcnt, dunno is this bug <- FIXME)
int CheckMembers(float memcnt)
{
	gedict_t *p, *p2;
	float f1;
	int retVal = 1;
	char *s = "";

	maxPlayerCount = 0;
	for (p = world; (p = find_plr(p));)
	{
		p->k_flag = 0;
	}

	for (p = world; (p = find_plr(p));)
	{
		if (p->k_flag)
		{
			continue;
		}

		p->k_flag = 1;
		f1 = 1;

		if (!strnull(s = getteam(p)))
		{
			for (p2 = p; (p2 = find_plr(p2));)
			{
				if (streq(s, getteam(p2)))
				{
					p2->k_flag = 1;
					f1++;
				}
			}
		}

		maxPlayerCount = max(f1, maxPlayerCount);
		if (f1 < memcnt)
		{
			retVal = 0;
		}
	}

	return retVal;
}

extern demo_marker_t demo_markers[];
extern int demo_marker_index;

void ListDemoMarkers(void)
{
	int i = 0;

	if (!demo_marker_index)
	{
		return;
	}

	G_bprint(2, "%s:\n\235\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236"
				"\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\237\n",
				redtext("Demo markers"));

	for (i = 0; i < demo_marker_index; ++i)
	{
		int total = (int)(demo_markers[i].time - match_start_time);

		G_bprint(2, "%s: %d:%02d \220%s\221\n", redtext("Time"), (total / 60), (total % 60),
					demo_markers[i].markername);
	}

	G_bprint(2, "\235\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236"
				"\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\237\n");
}

void EM_on_MatchEndBreak(int isBreak)
{
	gedict_t *p;

	for (p = world; (p = find_client(p));)
	{
		if (isBreak)
		{
			on_match_break(p);
		}
		else
		{
			on_match_end(p);
		}
	}
}

// WARNING: if we are skip log, we are also delete demo

// qwleague series: k_series_maps holds a space-separated ordered map list. These
// read it positionally. The series state lives entirely in cvars, which survive
// `changelevel` (player-entity stats get zeroed on a level change, cvars do not).
int mm_series_map_count(void)
{
	const char *p = cvar_string("k_series_maps");
	int n = 0;

	while (*p)
	{
		while (*p == ' ' || *p == '\t') { p++; }
		if (!*p) { break; }
		while (*p && *p != ' ' && *p != '\t') { p++; }
		n++;
	}
	return n;
}

void mm_series_map_at(int idx, char *out, int out_sz)
{
	const char *p = cvar_string("k_series_maps");
	char tok[64];
	int i = 0;
	size_t n;

	out[0] = 0;
	while (*p)
	{
		while (*p == ' ' || *p == '\t') { p++; }
		if (!*p) { break; }
		n = 0;
		while (*p && *p != ' ' && *p != '\t' && n < sizeof(tok) - 1) { tok[n++] = *p++; }
		tok[n] = 0;
		if (i == idx) { strlcpy(out, tok, out_sz); return; }
		i++;
	}
}

// Per-map weapon bans for a matchmade match. k_match_map_rules is
// "<map>:<k_disallow_weapons bitmask> ..." as rendered by the agent (qwleague
// docs/aim-mode.md), and carries EVERY map of the series pool including the
// ones that ban nothing. Returns this map's mask, or 0 when the map isn't
// listed -- which is also what an empty cvar gives, i.e. every mode that
// states no bans, and every older brain.
//
// Those zeros are the point: one server plays a whole series and
// k_disallow_weapons survives the changelevel, so a map with no entry would
// inherit the previous map's bans -- aim's no-LG on end leaking onto povdmm4.
//
// Scanned by hand like mm_series_map_at above rather than with strtok:
// cvar_string hands back an engine-owned buffer and strtok would write into
// it. Masked with DA_WPNS so a malformed value can only ever ban real weapons.
int mm_map_disallow_weapons(void)
{
	const char *p = cvar_string("k_match_map_rules");
	char tok[80];
	char *colon;
	size_t n;

	while (*p)
	{
		while (*p == ' ' || *p == '\t')
		{
			p++;
		}
		if (!*p)
		{
			break;
		}
		n = 0;
		while (*p && *p != ' ' && *p != '\t' && n < sizeof(tok) - 1)
		{
			tok[n++] = *p++;
		}
		tok[n] = 0;
		// Drain an over-long entry, so the scan resumes at the next token
		// instead of re-reading this one's tail as an entry of its own.
		while (*p && *p != ' ' && *p != '\t')
		{
			p++;
		}
		colon = strchr(tok, ':');
		if (!colon)
		{
			continue;
		}
		*colon = 0;
		if (streq(tok, mapname))
		{
			return atoi(colon + 1) & DA_WPNS;
		}
	}

	return 0;
}

// Which team userinfo string is backend team 1 vs team 2
// (k_series_team1/2, sent by the brain in ITS team order — a mid-series swap
// respawn seeds k_series_t1wins/t2wins in that same numbering, so t1/t2 must
// never be guessed from whichever teams happen to be present). Queue matches
// really are "red"/"blue" (spawn.py TEAM_NAMES) and the brain omits the
// cvars, leaving the registered defaults; clan matches (pracs, clan
// tournament fixtures) carry the clans' TAGS here — the old hardcoded
// "red"/"blue" comparisons never matched a tag, so the series score stayed
// 0-0 and a decided series played its whole map list.
static char *mm_series_team1(void)
{
	char *t = cvar_string("k_series_team1");

	return strnull(t) ? "red" : t;
}

static char *mm_series_team2(void)
{
	char *t = cvar_string("k_series_team2");

	return strnull(t) ? "blue" : t;
}

// Series map winner by total frags per series team, bucketed by each player's
// "team" userinfo (the brain forces it for ALL modes, including 1on1 — see
// k_token_teams). We deliberately do NOT use get_scores1()/get_scores2()
// here: those only sum frags for _k_team1/_k_team2, which are unset in a
// teamplay-0 duel, so a duel always read 0-0 and the series never clinched.
// Bucketing by the forced team works for duel and team modes alike.
static void mm_series_team_frags(int *out_t1, int *out_t2)
{
	gedict_t *p;
	int t1 = 0, t2 = 0;

	for (p = world; (p = find_plr(p));)
	{
		char *t = getteam(p);

		if (streq(t, mm_series_team1()))
		{
			t1 += p->s.v.frags;
		}
		else if (streq(t, mm_series_team2()))
		{
			t2 += p->s.v.frags;
		}
	}

	*out_t1 = t1;
	*out_t2 = t2;
}

// qwleague: resolve a still-pending disconnect forfeit at match end (defined
// after the forfeit helpers below). Called from EndMatch so the empty-server
// teardown path can't post a resultless match that lets the abandoner dodge Elo.
static void mm_resolve_pending_forfeit_on_end(void);

void EndMatch(float skip_log)
{
	gedict_t *p;
	int old_match_in_progress = match_in_progress;
	char *tmp;
	float f1;
	qbool is_real_match_end = !isHoonyModeAny() || HM_is_game_over();
	// Matchmade servers self-terminate after a real match end (mm_shutdown
	// block below). The normal NextLevel()/intermission still runs so players
	// get the traditional frozen end-of-match scoreboard — but GotoNextMap()
	// refuses to actually reload the map on these servers (that reload would
	// wipe the mm_shutdown timer and re-arm the auto-start). So the server sits
	// on the intermission screen until the deferred quit fires.
	qbool mm_will_shutdown = is_real_match_end && cvar("k_shutdown_on_end")
			&& old_match_in_progress >= 2;
	qbool f_modified_done = false, f_ruleset_done = false, f_version_done = false;

	if (match_over || !match_in_progress)
	{
		return;
	}

	match_over = 1;
	k_berzerk = 0;

	// qwleague: an abandoned matchmade match can reach EndMatch via the
	// empty-server teardown (all players gone) BEFORE the technical-timeout
	// deadline resolves the pending forfeit. Resolve it now — from who
	// abandoned first — so the stats POST below carries the loss instead of a
	// resultless upload that lets the abandoner escape without Elo change.
	if (is_matchmade_server() && old_match_in_progress == 2)
	{
		mm_resolve_pending_forfeit_on_end();
	}

	remove_projectiles();

	// s: zero the flag
	k_sudden_death = 0;

	if (!strnull(tmp = cvar_string("_k_host")))
	{
		trap_cvar_set("hostname", tmp); // restore host name at match end
	}

	trap_lightstyle(0, "m");

	// spec silence
	{
		int fpd = iKey(world, "fpd");

		fpd = fpd & ~64;
		localcmd("serverinfo fpd %d\n", fpd);

		cvar_fset("sv_spectalk", 1);
	}

	if (isCA())
	{
		CA_MatchBreak();
	}

	if (isHoonyModeAny())
	{
		G_bprint(2, "The point is over\n");
		HM_point_stats();
	}
	else if (deathmatch)
	{
		G_bprint(2, "The match is over\n");
	}

	EM_CorrectStats();

	if (k_bloodfest)
	{
		extern void bloodfest_stats(void);

		// remove any powerup left
		for (p = world; (p = nextent(p));)
		{
			if (streq(p->classname, "item_artifact_invulnerability")
					|| streq(p->classname, "item_artifact_invisibility")
					|| streq(p->classname, "item_artifact_super_damage"))
			{
				ent_remove(p);
				continue;
			}
		}

		bloodfest_stats();
	}

	if ( /* skip_log || */!deathmatch)
	{
		;
	}
	else
	{
		if (is_real_match_end && !isRACE())
		{
			ListDemoMarkers();
		}

		if (is_real_match_end)
		{
			if (!isCA())
			{
				MatchEndStats();
			}

			lastscore_add(); // save game result somewhere, so we can show it later
		}
	}

	if (is_real_match_end)
	{
		for (p = world; (p = find(p, FOFCLSN, "ghost"));)
		{
			ent_remove(p);
		}
	}

	StopTimer(skip_log); // WARNING: if we are skip log, we are also delete demo

	if (is_real_match_end)
	{
		for (f1 = 666; k_teamid >= f1; f1++)
		{
			localcmd("localinfo %d \"\"\n", (int)f1); //removing key
		}

		for (f1 = 1; k_userid >= f1; f1++)
		{
			localcmd("localinfo %d \"\"\n", (int)f1); //removing key
		}
	}

	if (old_match_in_progress == 2)
	{
		for (p = world; (p = find_plr(p));)
		{
			p->ready = 0; // force players be not ready after match is end.

			if (cvar("k_on_end_f_modified") && !f_modified_done)
			{
				stuffcmd(p, "say f_modified\n");
				f_modified_done = true;
			}
			if (cvar("k_on_end_f_ruleset") && !f_ruleset_done)
			{
				stuffcmd(p, "say f_ruleset\n");
				f_ruleset_done = true;
			}
			if (cvar("k_on_end_f_version") && !f_version_done)
			{
				stuffcmd(p, "say f_version\n");
				f_version_done = true;
			}
		}
	}

	EM_on_MatchEndBreak(skip_log);

	if (isHoonyModeAny())
	{
		qbool demomarker_logged = false;

		if (!HM_is_game_over())
		{
			match_over = 0;

			// All bots ready first
			for (p = world; (p = find_plr(p));)
			{
				if (p->isBot)
				{
					p->ready = true;
				}
			}

			for (p = world; (p = find_plr(p));)
			{
				if (!demomarker_logged)
				{
					stuffcmd(p, "//demomark 0 round-%2d\n", HM_current_point());
					demomarker_logged = true;
				}

				stuffcmd(p, "ready\n");
			}
		}
		else
		{
			for (p = world; (p = find_plr(p));)
			{
				stuffcmd(p, "hmstats\n");
			}

			StopLogs();
			NextLevel();
		}
	}
	else
	{
		StopLogs();
		NextLevel();
	}

	// allow ready/break in bloodfest without map reloading.
	if (k_bloodfest || isCA())
	{
		match_over = 0;
	}

	if (is_real_match_end)
	{
		g_matchstarttime = 0;
	}

	if (SpawnicideStatus() == SPAWNICIDE_MATCH)
	{
		SpawnicideDisable();
	}

	// qwleague Bo3/BoN series: on a matchmade server playing a multi-map series,
	// after a real (non-aborted) map end, record this map's winner and decide
	// whether to advance to the next map instead of self-terminating. Series
	// score lives in cvars (survives changelevel). The backend independently
	// aggregates per-map stats for Elo; this only governs when the SERVER stops
	// and which map it loads next.
	if (mm_will_shutdown && ((int) cvar("k_series_bestof")) > 1 && !cvar("k_match_aborted"))
	{
		int bestof = (int) cvar("k_series_bestof");
		int idx    = (int) cvar("k_series_index");
		int t1     = (int) cvar("k_series_t1wins");
		int t2     = (int) cvar("k_series_t2wins");
		int total  = mm_series_map_count();
		int clinch = bestof / 2 + 1;
		qbool playall = cvar("k_series_playall");
		qbool forfeit = cvar_string("k_match_forfeit_loser")[0] != 0;
		int s1, s2;
		qbool decided;

		mm_series_team_frags(&s1, &s2);

		// Record this map's winner. A forfeit loses ONLY this map (credit the
		// OTHER team), so the series plays on unless this clinches it — a single
		// drop no longer ends the whole match. Non-forfeit maps resolve by frags
		// (overtime already broke ties before here).
		if (forfeit)
		{
			int fteam = (int) cvar("k_match_forfeit_team");
			if (fteam == 1)      { t2++; }   // team1 forfeited → team2 wins map
			else if (fteam == 2) { t1++; }   // team2 forfeited → team1 wins map
			else if (s1 > s2)    { t1++; }   // unknown team: fall back to frags
			else if (s2 > s1)    { t2++; }
		}
		else
		{
			if (s1 > s2) { t1++; }
			else if (s2 > s1) { t2++; }
		}
		cvar_fset("k_series_t1wins", t1);
		cvar_fset("k_series_t2wins", t2);

		// Fixed-length series ("Game of N", k_series_playall 1): no clinch —
		// every map in the list is played and the backend picks the winner by
		// map wins. Checked against the LIST only, never t1/t2: a mid-series
		// server-swap respawn seeds k_series_t1wins/t2wins at or past clinch
		// with just the undecided maps in k_series_maps, and those must still
		// be played. The abandon watchdog's series forfeit is a separate path
		// and still ends a playall series early.
		if (playall)
		{
			decided = ((idx + 1) >= total);
		}
		else
		{
			decided = (t1 >= clinch) || (t2 >= clinch) || ((idx + 1) >= total);
		}
		if (!decided)
		{
			cvar_fset("k_series_index", idx + 1);
			cvar_set("k_series_continue", "1");
			mm_will_shutdown = false;  // advance to the next map instead of quitting
			G_bprint(PRINT_HIGH, "%s\n",
					redtext(va("Series %d-%d - loading map %d of %d...",
							t1, t2, idx + 2, total)));
		}
		else
		{
			G_bprint(PRINT_HIGH, "%s\n",
					redtext(va("Series complete - %d-%d.", t1, t2)));
		}
	}

	// qwleague matchmaking: per-match servers self-terminate after stats are
	// posted. We don't quit immediately — sv_web_postfile is asynchronous and
	// killing the process now can interrupt the stats upload. A 15s deferred
	// quit gives the upload time to complete, and also lets players see the
	// final scoreboard.
	if (mm_will_shutdown)
	{
		gedict_t *e = spawn();
		e->classname = "mm_shutdown";
		e->think = (func_t) mm_shutdown_think;
		e->s.v.nextthink = g_globalvars.time + 1;
		e->cnt2 = 15;

		// Let players know the matchmade server is about to recycle.
		G_bprint(PRINT_HIGH, "%s\n",
				redtext("GG! Match complete - server closes in 15 seconds."));
	}
}

// Pre-match clocks. These are per-MAP BUDGETS, not per-ticker timers: leaving
// and re-entering during warmup is free, but it must not RENEW the clock.
// Each map load gets exactly one waiting budget and one warmup budget:
//
//  - mm_wait_left   (k_match_join_deadline, 300s) — total time spent waiting for
//    a full roster, shared by the initial join-deadline ticker
//    (mm_join_deadline_think) and the abandonment watchdog (mm_abandon_think).
//    Drop out and come back as often as you like; the same 5:00 keeps draining,
//    so a disconnect/reconnect loop can't stall the server forever.
//  - mm_warmup_left (k_mm_warmup, 300s) — total warmup time. A player who leaves
//    and returns RESUMES the remaining warmup (mm_prewar_think) instead of
//    restarting it at full length.
//
// Before this, every ticker started a fresh full-length countdown on each
// incarnation, so cycling connect/disconnect extended the pre-match phase
// without bound. (And map 1's watchdog used a hardcoded 60s, which gave a player
// who had already shown up LESS time to come back than one who never showed at
// all — a router reboot during warmup became an abort plus an abandon cooldown.)
static int mm_wait_left = 0;
static int mm_warmup_left = 0;

static void mm_lineup_reset(void);

// Called from world.c on every matchmade map load: a NEW map (including each map
// of a series) gets fresh clocks, a leave/rejoin within the same map does not.
void mm_prematch_clocks_reset(void)
{
	mm_wait_left = (int) bound(30, cvar("k_match_join_deadline"), 600);
	mm_warmup_left = (int) bound(10, cvar("k_mm_warmup"), 600);
	// The previous map's lineup must not decide who is "missing" from this one.
	mm_lineup_reset();
}

// Forfeit helpers (defined further below) used by the series-aware no-show
// handling in mm_join_deadline_think.
static int mm_forfeit_outcome(char *loser, int loser_sz);
static int mm_missing_tokens(char out[][64], int max_out);
static void mm_set_forfeit_team_from_token(const char *tok);
static const char *mm_player_token(gedict_t *p);
void mm_notify_forfeit(void);

// A rostered player who connected as SPECTATOR is invisible to the roster
// checks (they scan players only) and would be counted "missing" — and
// eventually forfeited while literally on the server. Deliberately NOT treated
// as present (that would let a leaver sit in spec to force a no-penalty void);
// instead the no-show/abandon tickers warn them to rejoin as a player.
static void mm_warn_spectating_missing(void)
{
	char missing[16][64];
	int n;
	int i;
	gedict_t *s;

	n = mm_missing_tokens(missing, 16);
	for (i = 0; i < n; i++)
	{
		for (s = world; (s = find_spc(s));)
		{
			if (streq(mm_player_token(s), missing[i]))
			{
				G_sprint(s, PRINT_HIGH, "%s\n",
						redtext("You are connected as SPECTATOR - rejoin as a "
								"player NOW or you will be forfeited."));
			}
		}
	}
}

// Ticks once per second on a matchmade server until either:
//  - 2 humans are present (entity removes itself; mm_maybe_auto_start handles the rest)
//  - the join deadline expires (announces no-show in chat and triggers shutdown)
//  - the match actually starts (entity removes itself)
void mm_join_deadline_think(void)
{
	int humans = 0;
	int needed = (int) cvar("k_mm_players");
	gedict_t *p;
	int left = mm_wait_left;  // shared per-map waiting budget, see mm_prematch_clocks_reset

	if (needed < 2)
	{
		needed = 2;
	}

	// Match is already in progress or done — nothing to wait for.
	if (match_in_progress || match_over)
	{
		ent_remove(self);
		return;
	}

	for (p = world; (p = find_plr(p));)
	{
		if (!p->isBot)
		{
			humans++;
		}
	}

	if (humans >= needed)
	{
		// Everyone made it. mm_maybe_auto_start will start the countdown.
		ent_remove(self);
		return;
	}

	if (left <= 0)
	{
		// Deadline expired with only one (or zero) players.
		gedict_t *sd;
		char loser[64] = "";

		// Series-aware — mirrors mm_abandon_think. A fresh join-deadline ticker
		// is spawned on EVERY map load (world.c FirstFrame), so it also runs at
		// the start of a LATER series map. If a map is already decided
		// (k_series_index > 0) and exactly ONE side is missing, voiding with "no
		// penalty" would throw away the decided map AND let the player who
		// abandoned an earlier map — and never came back for this one — dodge
		// the loss. Instead, forfeit the remaining series to the present player.
		if ((int) cvar("k_series_index") > 0
				&& mm_forfeit_outcome(loser, sizeof(loser)) == 1)
		{
			mm_set_forfeit_team_from_token(loser);
			cvar_set("k_match_forfeit_loser", loser);
			G_bprint(PRINT_HIGH, "%s\n",
					redtext("Opponent did not return for this map - forfeiting the "
							"remaining series to the present player."));
			mm_notify_forfeit();
		}
		else
		{
			// Nothing decided yet (map 1 / 0-0) or both sides gone — a genuine
			// no-show, void with no Elo.
			G_bprint(PRINT_HIGH, "%s\n",
					redtext("Opponent did not connect in time."));
			G_bprint(PRINT_HIGH, "%s\n",
					redtext("Server closing - no penalty for you."));
		}

		sd = spawn();
		sd->classname = "mm_shutdown";
		sd->think = (func_t) mm_shutdown_think;
		sd->s.v.nextthink = g_globalvars.time + 1;
		sd->cnt2 = 15;  // match the post-match flush window — the series-
		                // forfeit POST above is async and must complete
		                // before quit, not just the chat message.

		ent_remove(self);
		return;
	}

	// Nudge a rostered player idling in the spectator slots (every ~15s).
	if (left % 15 == 0)
	{
		mm_warn_spectating_missing();
	}

	mm_wait_left = left - 1;
	self->s.v.nextthink = g_globalvars.time + 1;
}

void mm_shutdown_think(void)
{
	int sec = (int) self->cnt2;

	if (sec <= 0)
	{
		localcmd("\nquit\n");
		trap_executecmd();
		ent_remove(self);
		return;
	}

	// No in-game countdown announcements — the website handles match-end UX.
	// The 15s delay still runs (needed for the stats upload to complete), but
	// silently.
	self->cnt2 = sec - 1;
	self->s.v.nextthink = g_globalvars.time + 1;
}

void SaveOvertimeStats(void)
{
	gedict_t *p;

	if (k_overtime)
	{
		for (p = world; (p = find_plr(p));)
		{
			// save overtime stats
			p->ps.ot_a = (int)p->s.v.armorvalue;
			p->ps.ot_items = p->s.v.items; // float
			p->ps.ot_h = (int)p->s.v.health;
		}
	}
}

void CheckOvertime(void)
{
	// SM_PrepareShowscores() is called in StartMatch(), but in matchless mode this happens when the first player joins.
	// Therefore, teams != 2 yet and the function doesn't do anything.
	// This causes matchless CTF to go into perpetual overtime, since "sc = get_scores1() - get_scores2();" calculated below will always return 0.
	// So, we call this function here to handle this scenario.
	gedict_t *timer, *ed1, *ed2 ;
	int teams, players ;
	int sc;
	int k_mb_overtime;
	int k_exttime; 

	SM_PrepareShowscores();

	ed1 = get_ed_scores1(); ed2 = get_ed_scores2();
	teams = CountTeams();
	players = CountPlayers();
	sc = get_scores1() - get_scores2();
	k_mb_overtime = cvar("k_overtime");
	k_exttime = bound(1, cvar("k_exttime"), 999); // at least some reasonable values

	// In hoonymode the round is timing out, not the match - we're effectively always in suddendeath mode
	if (isHoonyModeAny())
	{
		HM_draw();

		return;
	}

	// If 0 no overtime, 1 overtime, 2 sudden death
	// And if its neither then well we exit
	if (!k_mb_overtime || ((k_mb_overtime != 1) && (k_mb_overtime != 2) && (k_mb_overtime != 3) && (k_mb_overtime != SD_GOLDEN_FRAG)))
	{
		EndMatch(0);

		return;
	}

	// Overtime.
	// Ok we have now decided that the game is ending, so decide overtime wise here what to do.

	if ((isDuel() || isFFA()) && ed1 && ed2)
	{
		sc = ed1->s.v.frags - ed2->s.v.frags;
	}

//	if( k_matchLess ) {
//		k_mb_overtime = 0; // no overtime in matchLess mode
//	}
//	else

	if (lgc_enabled())
	{
		k_mb_overtime = 0; // no overtime in lgc mode
	}

	if ((isTeam() || isCTF()) && (teams != 2))
	{
		k_mb_overtime = 0; // no overtime in case of less than 2 or more than 2 teams
	}
	else if (((isDuel() || isFFA()) && ed1 && ed2) // duel or ffa
			|| ((isTeam() || isCTF()) && (teams == 2) && (players > 1))) // Handle a 2v2 or above team game or 1v1 CTF
	{
		if (((k_mb_overtime == 3) && abs(sc) > 1) // tie-break overtime allowed with one frag difference (c) ktpro
			|| (k_mb_overtime != 3 && k_mb_overtime != SD_GOLDEN_FRAG && abs(sc) > 0)) // time based
				//or sudden death overtime not including golden frag allowed with zero frag difference
		{
			k_mb_overtime = 0;
		}
	}
	else
	{
		k_mb_overtime = 0;
	}

	if (!k_mb_overtime)
	{
		EndMatch(0);

		return;
	}

	k_overtime = k_mb_overtime;
	SaveOvertimeStats();

	G_bprint(2, "time over, the game is a draw\n");

	if (k_overtime == 1)
	{
		// Ok its increase time
		self->cnt = k_exttime;
		self->cnt2 = 60;
		localcmd("serverinfo status \"%d min left\"\n", (int)self->cnt);

		G_bprint(2, "\220%s\221 minute%s overtime follows\n", dig3(k_exttime), count_s(k_exttime));
		self->s.v.nextthink = g_globalvars.time + 1;
		match_end_time += self->cnt * 60;
	}
	else if (k_overtime == 2)
	{
		k_sudden_death = SD_NORMAL;
		match_end_time = 0;
	}
	else if (k_overtime == SD_GOLDEN_FRAG) {
		k_sudden_death = SD_GOLDEN_FRAG;
		match_end_time = 0;
	}
	else
	{
		k_sudden_death = SD_TIEBREAK;
		match_end_time = 0;
	}

	if (k_sudden_death)
	{
		G_bprint(2, "%s %s\n", SD_type_str(), redtext("overtime begins"));

		// added timer removal at sudden death beginning
		for (timer = world; (timer = find(timer, FOFCLSN, "timer"));)
		{
			ent_remove(timer);
		}
	}
}

// Called every second during a match. cnt = minutes, cnt2 = seconds left.
// Tells the time every now and then.
void TimerThink(void)
{
	gedict_t *p;
	int idle_time;
	float f1, f2;

	//if in matchless mode, check that the user hasn't exceeded k_matchless_max_idle_time
	if (k_matchLess && CountPlayers() && match_in_progress && k_matchLess_idle_time)
	{
		for (p = world; (p = find_plr(p));)
		{
			idle_time = (int)(p->client_time - p->attack_finished);
			if (idle_time > k_matchLess_idle_time)
			{
				G_sprint(
						p,
						2,
						"You were forced to reconnect as spectator by exceeding the maximum idle time of %i seconds.\n",
						k_matchLess_idle_time);
				stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO, "spectator 1\n");
				if (!strnull(ezinfokey(p, "Qizmo")))
				{
					stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO,
									"say ,:dis\nwait;wait;wait; say ,:reconnect\n");
				}
				else
				{
					stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO, "disconnect\nwait;wait;reconnect\n");
				}
			}
			else if (idle_time == k_matchLess_idle_warn)
			{
				G_sprint(
						p,
						2,
						"\207%s You will be forced to spectate if you do not fire within %i seconds!\n",
						redtext("WARNING:"), (k_matchLess_idle_time - k_matchLess_idle_warn));
			}
		}
	}

	if (!k_matchLess && !CountPlayers())
	{
		EndMatch(1);

		return;
	}

	if (k_sudden_death)
	{
		return;
	}

	if ((self->k_teamnum < g_globalvars.time) && !k_checkx)
	{
		k_checkx = 1; // global which set to true when some time spend after match start
	}

	// berzerk
	if (k_berzerktime != 0)
	{
		f1 = k_berzerktime;
		f2 = floor(f1 / 60);
		f1 = f1 - (f2 * 60);
		f2 = f2 + 1;
		if ((self->cnt2 == f1) && (self->cnt == f2))
		{
			G_bprint(2, "BERZERK!!!!\n");
			trap_lightstyle(0, "ob");
			k_berzerk = 1;

			for (p = world; (p = find_plr(p));)
			{
				if (strcmp(p->netname, "") && p->s.v.health > 0)
				{
					p->s.v.items = (int)p->s.v.items | (IT_QUAD | IT_INVULNERABILITY);
					p->super_time = 1;
					p->super_damage_finished = g_globalvars.time + 3600;
					p->invincible_time = 1;
					p->invincible_finished = g_globalvars.time + 2;
				}
			}
		}
	}

	(self->cnt2)--;

	if (!self->cnt2)
	{
		self->cnt2 = 60;
		self->cnt -= 1;

		localcmd("serverinfo status \"%d min left\"\n", (int)self->cnt);

		if (!self->cnt)
		{
			CheckOvertime();

			return;
		}

		G_bprint(2, "\220%s\221 minute%s remaining\n", dig3(self->cnt), count_s(self->cnt));

		self->s.v.nextthink = g_globalvars.time + 1;

		if (k_showscores && !isCA())
		{
			if ((current_umode < um2on2on2) || (current_umode > um4on4on4))
			{
				int sc = get_scores1() - get_scores2();

				if (sc)
				{
					G_bprint(2, "%s \220%s\221 leads by %s frag%s\n", redtext("Team"),
								cvar_string((sc > 0 ? "_k_team1" : "_k_team2")), dig3(abs((int)sc)),
								count_s(abs((int)sc)));
				}
				else
				{
					G_bprint(2, "The game is currently a tie\n");
				}
			}
			else
			{
				// if the current UserMode is 2on2on2, 3on3on3, 4on4on4, we have 3 teams
				int s1 = get_scores1();
				int s2 = get_scores2();
				int s3 = get_scores3();
				int sc;

				if ((s1 > s2) && (s1 > s3))
				{
					// Team 1 is leading
					if (s2 > s3)
					{
						sc = get_scores1() - get_scores2();
					}
					else
					{
						sc = get_scores1() - get_scores3();
					}

					G_bprint(2, "%s \220%s\221 leads by %s frag%s\n", redtext("Team"),
								cvar_string("_k_team1"), dig3(abs((int)sc)),
								count_s(abs((int)sc)));
				}
				else if ((s2 > s1) && (s2 > s3))
				{
					// Team 2 is leading
					if (s1 > s3)
					{
						sc = get_scores2() - get_scores1();
					}
					else
					{
						sc = get_scores2() - get_scores3();
					}

					G_bprint(2, "%s \220%s\221 leads by %s frag%s\n", redtext("Team"),
								cvar_string("_k_team2"), dig3(abs((int)sc)),
								count_s(abs((int)sc)));
				}
				else if ((s3 > s1) && (s3 > s2))
				{
					// Team 3 is leading
					if (s1 > s2)
					{
						sc = get_scores3() - get_scores1();
					}
					else
					{
						sc = get_scores3() - get_scores3();
					}

					G_bprint(2, "%s \220%s\221 leads by %s frag%s\n", redtext("Team"),
								cvar_string("_k_team3"), dig3(abs((int)sc)),
								count_s(abs((int)sc)));
				}
				else
				{
					G_bprint(2, "The game is currently a tie\n");
				}
			}
		}

		return;
	}

	if ((self->cnt == 1) && ((self->cnt2 == 30) || (self->cnt2 == 15) || (self->cnt2 <= 10)))
	{
		G_bprint(2, "\220%s\221 second%s\n", dig3(self->cnt2), count_s(self->cnt2));
	}

	self->s.v.nextthink = g_globalvars.time + 1;
}

// remove/add some items from map regarding dmm and game mode
void SM_PrepareMap(void)
{
	gedict_t *p;

	if (isCTF())
	{
		SpawnRunes(cvar("k_ctf_runes"));
	}

	// this must be removed in any cases
	remove_projectiles();

	for (p = world; (p = nextent(p));)
	{
		// going for the if content record..
		if (isRA() || isRACE() || ((deathmatch == 4) && (cvar("k_instagib") || cvar("k_midair")))
				|| cvar("k_noitems") || k_bloodfest)
		{
			if (streq(p->classname, "weapon_nailgun") || streq(p->classname, "weapon_supernailgun")
					|| streq(p->classname, "weapon_supershotgun")
					|| streq(p->classname, "weapon_rocketlauncher")
					|| streq(p->classname, "weapon_grenadelauncher")
					|| streq(p->classname, "weapon_lightning") || streq(p->classname, "item_shells")
					|| streq(p->classname, "item_spikes") || streq(p->classname, "item_rockets")
					|| streq(p->classname, "item_cells") || streq(p->classname, "item_health")
					|| streq(p->classname, "item_armor1") || streq(p->classname, "item_armor2")
					|| streq(p->classname, "item_armorInv")
					|| streq(p->classname, "item_artifact_invulnerability")
					|| streq(p->classname, "item_artifact_envirosuit")
					|| streq(p->classname, "item_artifact_invisibility")
					|| streq(p->classname, "item_artifact_super_damage"))
			{
				soft_ent_remove(p);
				continue;
			}
		}

		if (deathmatch == 2)
		{
			if (streq(p->classname, "item_armor1") || streq(p->classname, "item_armor2")
					|| streq(p->classname, "item_armorInv"))
			{
				soft_ent_remove(p);
				continue;
			}
		}

		if (deathmatch >= 4)
		{
			int disallowed_weapons = (int)cvar("k_disallow_weapons") & DA_WPNS;

			// no weapons for any of this deathmatches (4 or 5),
			// unless ToT mode with item pickup bonus is enabled and
			// the weapon isn't disallowed.
			if (streq(p->classname, "weapon_nailgun"))
			{
				soft_ent_remove(p);
				continue;
			}
			else if (streq(p->classname, "weapon_supernailgun"))
			{
				soft_ent_remove(p);
				continue;
			}
			else if (streq(p->classname, "weapon_supershotgun"))
			{
				soft_ent_remove(p);
				continue;
			}
			else if (streq(p->classname, "weapon_grenadelauncher"))
			{
				soft_ent_remove(p);
				continue;
			}
			else if (streq(p->classname, "weapon_rocketlauncher") &&
				(!FrogbotItemPickupBonus() ||
				(disallowed_weapons & IT_ROCKET_LAUNCHER)))
			{
				soft_ent_remove(p);
				continue;
			}
			else if (streq(p->classname, "weapon_lightning") &&
				(!FrogbotItemPickupBonus() ||
				(disallowed_weapons & IT_LIGHTNING)))
			{
				soft_ent_remove(p);
				continue;
			}

			if (deathmatch == 4)
			{
				// no weapon ammo and megahealth for dmm4
				if (streq(p->classname, "item_shells"))
				{
					soft_ent_remove(p);
					continue;
				}
				else if (streq(p->classname, "item_spikes"))
				{
					soft_ent_remove(p);
					continue;
				}
				else if (streq(p->classname, "item_rockets") &&
					!FrogbotItemPickupBonus())
				{
					soft_ent_remove(p);
					continue;
				}
				else if (streq(p->classname, "item_cells"))
				{
					soft_ent_remove(p);
					continue;
				}
				else if ((streq(p->classname, "item_health") &&
					((int)p->s.v.spawnflags & H_MEGA)) &&
					!FrogbotItemPickupBonus())
				{
					soft_ent_remove(p);
					continue;
				}
			}
		}

		if (k_killquad && streq(p->classname, "item_artifact_super_damage"))
		{	// no normal quad in killquad mode.
			soft_ent_remove(p);
			continue;
		}
	}

	ClearBodyQue(); // hide corpses
}

// put clients in server and reset some params
static void SM_PrepareClients(void)
{
	int hdc, i, j, player_count = 0;
	char *pl_team;
	gedict_t *p, *temp;
	gedict_t *players[MAX_CLIENTS];
	qbool hoonymode_reset = isHoonyModeAny() && HM_current_point() > 0;

	k_teamid = 666;
	localcmd("localinfo 666 \"\"\n");
	trap_executecmd(); // <- this really needed

	initial_match_spawns = true;

	for (p = world; (p = find_plr(p));)
	{
		p->socdDetectionCount = 0;
		p->socdValidationCount = 0;
		p->fStrafeChangeCount = 0;
		p->fFramePerfectStrafeChangeCount = 0;
		p->fLastSideMoveSpeed = 0;
		p->matchStrafeChangeCount = 0;
		p->matchPerfectStrafeCount = 0;
		p->totalStrafeChangeCount = 0;
		p->totalPerfectStrafeCount = 0;
		p->nullStrafeCount = 0;
	}

	for (p = world; (p = find_plr(p));)
	{
		players[player_count++] = p;
		p->socdDetectionCount = 0;
		p->socdValidationCount = 0;
		p->fStrafeChangeCount = 0;
		p->fFramePerfectStrafeChangeCount = 0;
		p->fLastSideMoveSpeed = 0;
		p->matchStrafeChangeCount = 0;
		p->matchPerfectStrafeCount = 0;
		p->totalStrafeChangeCount = 0;
		p->totalPerfectStrafeCount = 0;
		p->nullStrafeCount = 0;
	}

	for (i = player_count - 1; i > 0; i--)
	{
		j = rand() % (i + 1);
		temp = players[i];
		players[i] = players[j];
		players[j] = temp;
	}

	for (j = 0; j < player_count; j++)
	{
		p = players[j];
		if (!k_matchLess)
		{
			// skip setup k_teamnum in matchLess mode
			pl_team = getteam(p);
			p->k_teamnum = 0;

			if (!strnull(pl_team))
			{
				i = 665;

				while (k_teamid > i && !p->k_teamnum)
				{
					i++;

					if (streq(pl_team, ezinfokey(world, va("%d", i))))
					{
						p->k_teamnum = i;
					}
				}

				if (!p->k_teamnum)
				{
					// team not found in localinfo, so put it in
					i++;
					p->k_teamnum = k_teamid = i;
					localcmd("localinfo %d \"%s\"\n", i, pl_team);
					trap_executecmd(); // <- this really needed
				}
			}
			else
				p->k_teamnum = 666;
		}

		if (!hoonymode_reset)
		{
			p->kills = p->suicides = p->friendly = p->deaths = p->s.v.frags = 0;
		}

		hdc = p->ps.handicap; // save player handicap

		if (!hoonymode_reset)
		{
			memset((void*) &(p->ps), 0, sizeof(p->ps)); // clear player stats

			WS_Reset(p); // force reset "new weapon stats"
		}

		p->ps.handicap = hdc; // restore player handicap

		if (isRA())
		{
			if (isWinner(p) || isLoser(p))
			{
				setfullwep(p);
			}

			continue;
		}

		// ignore k_respawn() in case of coop unless bloodfest
		if (!deathmatch && !k_bloodfest)
		{
			continue;
		}

		// ignore k_respawn() in case of CA
		if (isCA())
		{
			continue;
		}

		// ignore k_respawn() in case of race mode
		if (isRACE())
		{
			continue;
		}

		k_respawn(p, false);

		p->k_pauseRequests = MAX_PAUSE_REQUESTS;
	}

	initial_match_spawns = false;

	// now that every player is spawned, do sounds & fog effect
	SM_ExecuteQueuedSpawnEffects();
}

// This creates visual effect & sound effects for players at start of match
// See bug #115... if 2 players spawn next to each other, the first player hears the second, but the second only hears the first based on final location during pre-war
static void SM_ExecuteQueuedSpawnEffects(void)
{
	gedict_t *p;

	for (p = world; (p = find_plr(p));)
	{
		if (p->spawn_effect_queued)
		{
			vec3_t fog_org;

			trap_makevectors(p->s.v.angles);
			VectorMA(p->s.v.origin, 20, g_globalvars.v_forward, fog_org);
			spawn_tfog(fog_org);

			play_teleport(p);

			p->spawn_effect_queued = false;
		}
	}
}

void SM_PrepareShowscores(void)
{
	gedict_t *p;
	char *team1 = "";
	char *team2 = "";
	char *team3 = "";

	if (k_matchLess && !isCTF()) // skip this in matchLess mode, unless CTF matchless (otherwise causes unlimited overtime because "sc" is always = 0 in CheckOvertime())
	{
		return;
	}

	if (((!isTeam() && !isCTF())
			|| ((CountRTeams() != 2) && (CountRTeams() != 3)))
			&& !(isCTF() && k_matchLess)) // we need 2 ready teams, unless CTF matchless
	{
		return;
	}

	if ((p = find_plr(world)))
	{
		team1 = getteam(p);
	}

	if (strnull(team1))
	{
		return;
	}

	while ((p = find_plr(p)))
	{
		team2 = getteam(p);

		if (strneq(team1, team2))
		{
			break;
		}
	}

	if (strnull(team2) || streq(team1, team2))
	{
		return;
	}

	k_showscores = 1;

	cvar_set("_k_team1", team1);
	cvar_set("_k_team2", team2);
	if ((current_umode >= um2on2on2) && (current_umode <= um4on4on4))
	{
		// let's see if there is a player with a 3rd team
		while ((p = find_plr(p)))
		{
			team3 = getteam(p);

			if (strneq(team1, team3) && strneq(team2, team3))
			{
				break;
			}
		}

		if (strnull(team3) || streq(team1, team3) || streq(team2, team3))
		{
			return;
		}

		cvar_set("_k_team3", team3);
	}
}

void SM_PrepareHostname(void)
{
	char *team1 = cvar_string("_k_team1");
	char *team2 = cvar_string("_k_team2");

	cvar_set("_k_host", cvar_string("hostname")); // save host name at match start

	if (k_showscores && !strnull(team1) && !strnull(team2))
	{
		if ((current_umode < um2on2on2) || (current_umode > um4on4on4))
		{
			cvar_set("hostname", va("%s (%.4s vs. %.4s)\207", cvar_string("hostname"), team1, team2));
		}
		else
		{
			char *team3 = cvar_string("_k_team3");

			cvar_set("hostname", va("%s (%.4s vs. %.4s vs. %.4s)\207", cvar_string("hostname"), team1, team2, team3));
		}
	}
	else
	{
		cvar_set("hostname", va("%s\207", cvar_string("hostname")));
	}
}

void SM_on_MatchStart(void)
{
	gedict_t *p;

	for (p = world; (p = find_client(p));)
	{
		on_match_start(p);
	}
}

// Helpers for qwleague matchmade-server auto-start.
qbool is_matchmade_server(void)
{
	return cvar_string("k_allowed_tokens")[0] != 0;
}

// Publish the brain-stated match tag (k_match_tag, e.g. "4on4 solo" or
// "2on2 team official") as serverinfo matchtag -- the key QTV, the hub and
// the server browsers show as what kind of match this is. The agent's match
// config sets the serverinfo key too, but that only covers the boot: KTX's
// rules reset (last player leaving, _reset_settings) clears matchtag, so it
// is re-applied from the cvar at every map load and after every reset.
// Empty means the brain stated nothing, which leaves matchtag alone.
void mm_apply_match_tag(void)
{
	char tag[32];

	if (!is_matchmade_server())
	{
		return;
	}

	strlcpy(tag, cvar_string("k_match_tag"), sizeof(tag));
	if (!tag[0])
	{
		return;
	}

	localcmd("serverinfo matchtag \"%s\"\n", clean_string(tag));
}

// True if `token` matches an entry in the space/tab/comma-separated
// k_allowed_tokens list. Shared by the player connect gate (CanConnect) and the
// spectator auto-promote (SpectatorConnect), so both agree on what counts as an
// expected match player.
qbool mm_token_allowed(const char *token)
{
	const char *p = cvar_string("k_allowed_tokens");
	char tok[64];
	size_t i;

	if (!token || !token[0])
	{
		return false;
	}

	while (*p)
	{
		while (*p == ' ' || *p == '\t' || *p == ',')
		{
			p++;
		}
		i = 0;
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && i < sizeof(tok) - 1)
		{
			tok[i++] = *p++;
		}
		tok[i] = 0;
		if (i > 0 && streq(tok, token))
		{
			return true;
		}
	}

	return false;
}

// Authoritative match token for a player: prefer *mtoken, pinned as a
// protected userinfo key at connect after validation (clients cannot change
// *-keys), over the mutable client-set "qwleague_token" — a stray setinfo or
// reconnect resend once mislabeled a row mid-series (match 336), and a leaver
// could blank/forge the mutable key to dodge or transfer forfeit blame.
static const char *mm_player_token(gedict_t *p)
{
	const char *tok = ezinfokey(p, "*mtoken");

	if (!tok[0])
	{
		tok = ezinfokey(p, "qwleague_token");
	}
	return tok;
}

// Look up the locked display name for player `p` from the agent-supplied
// `k_token_names` map, formatted as "<token>|<name>|<token>|<name>|...".
// Pipe-delimited (not space) so handles containing spaces survive. Returns
// true and fills `out` (NUL-terminated) when this player's match token has a
// registered name. Used to stop fakenicking on matchmade servers.
qbool mm_forced_name(gedict_t *p, char *out, int out_size)
{
	const char *my_token = mm_player_token(p);
	char *s = cvar_string("k_token_names");
	char tok[64];
	int i;

	if (out_size <= 0 || !my_token[0] || !s[0])
	{
		return false;
	}

	while (*s)
	{
		// token field
		i = 0;
		while (*s && *s != '|' && i < (int) sizeof(tok) - 1)
		{
			tok[i++] = *s++;
		}
		tok[i] = 0;
		if (*s == '|')
		{
			s++;
		}

		// name field (everything up to the next '|') — may contain spaces
		i = 0;
		while (*s && *s != '|' && i < out_size - 1)
		{
			out[i++] = *s++;
		}
		out[i] = 0;
		if (*s == '|')
		{
			s++;
		}

		if (tok[0] && out[0] && streq(tok, my_token))
		{
			return true;
		}
	}

	out[0] = 0;
	return false;
}

// Pull the next space/tab/comma separated token out of *pp, same separators
// k_token_teams uses. Returns false once the string is exhausted.
static qbool mm_next_tok(const char **pp, char *out, size_t size)
{
	const char *p = *pp;
	size_t i = 0;

	while ((*p == ' ') || (*p == '\t') || (*p == ','))
	{
		p++;
	}

	while (*p && (*p != ' ') && (*p != '\t') && (*p != ',') && (i < size - 1))
	{
		out[i++] = *p++;
	}

	out[i] = 0;
	*pp = p;

	return i > 0;
}

// The locked shirt/pants colors for a matchmade player's assigned team.
//
// A tournament fixture carries the two clans' own colors in k_team_colors as
// "<team> <top> <bottom> ...", keyed by the same team string k_token_teams
// assigns (a clan's tag, e.g. "SR") — so each clan plays in its own kit. A
// queue match names no colors and falls back to the classic red = 4 /
// blue = 13.
//
// Returns false when no forcing applies (not a matchmade server, spectator,
// bot, or a team that is neither named in k_team_colors nor red/blue — forcing
// nothing beats dumping both clans on red). Used at connect (initial force)
// and in ClientUserInfoChanged (reject client color changes).
qbool mm_forced_colors(gedict_t *p, int *top, int *bottom)
{
	char *team;
	const char *s;
	char tm[32], tc[8], bc[8];

	if (!is_matchmade_server() || p->isBot || p->ct == ctSpec)
	{
		return false;
	}

	team = getteam(p);
	if (!team[0])
	{
		return false;
	}

	s = cvar_string("k_team_colors");
	while (mm_next_tok(&s, tm, sizeof(tm)) && mm_next_tok(&s, tc, sizeof(tc))
			&& mm_next_tok(&s, bc, sizeof(bc)))
	{
		if (streq(tm, team))
		{
			// 0-16, ezQuake's range, not vanilla's 0-13: 14/15/16 (Orange,
			// Bright red, Black) are real picks a clan can configure, and
			// ezQuake bounds to 16 in both its "color" command and its display
			// forcing. Clamping to 13 here would quietly turn a Black kit into
			// Blue. Its own scoreboard renders 16 as black; only the 3D model
			// falls back to 13 — that is the client's call to make, not ours.
			*top = bound(0, atoi(tc), 16);
			*bottom = bound(0, atoi(bc), 16);

			return true;
		}
	}

	if (streq(team, "red") || streq(team, "blue"))
	{
		*top = *bottom = (streq(team, "red") ? 4 : 13);

		return true;
	}

	return false;
}

// Put the forced colors on the player: the authoritative userinfo (what the
// scoreboard, other clients and QTV read) plus a stuffcmd so the client's own
// cvars agree. It has to be "color <top> <bottom>" — clients have no
// topcolor/bottomcolor command, so that is the only form that takes.
void mm_force_colors(gedict_t *p)
{
	int top = 0, bottom = 0;

	if (!mm_forced_colors(p, &top, &bottom))
	{
		return;
	}

	SetUserInfo(p, "topcolor", va("%d", top), 0);
	SetUserInfo(p, "bottomcolor", va("%d", bottom), 0);
	stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO, "color %d %d\n", top, bottom);
}

// ─── Matchmade ruleset gate (best-effort, spoofable) ────────────────────────
// With k_mm_require_ruleset set, each player is probed at connect for their
// client ruleset and kicked if it isn't the required one. The probe uses the
// standard f_ruleset mechanism: we stuff the client to emit it and intercept
// the reply (mm_capture_ruleset_reply) so it never reaches public chat. The
// reply is client-cooperative, hence spoofable — see the cvar comment.
#define MM_RULESET_WINDOW_SEC 6

// Lowercase a bounded copy of src into dst for case-insensitive matching.
static void mm_lower(const char *src, char *dst, int dst_sz)
{
	int i;
	for (i = 0; src && src[i] && i < dst_sz - 1; i++)
	{
		char c = src[i];
		dst[i] = (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c;
	}
	dst[i] = 0;
}

void mm_arm_ruleset_check(gedict_t *p)
{
	if (!is_matchmade_server() || p->isBot)
	{
		return;
	}
	if (!cvar_string("k_mm_require_ruleset")[0])
	{
		return;  // disabled
	}
	if (p->ct != ctPlayer && p->ct != ctSpec)
	{
		return;
	}
	if (p->f_checkbuf)
	{
		p->f_checkbuf[0] = 0;
	}
	p->mm_rs_deadline = g_globalvars.time + MM_RULESET_WINDOW_SEC;
	// Trigger the client's f_ruleset response; its reply arrives as a say we
	// capture. IGNOREINDEMO so it doesn't pollute recorded demos.
	stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO, "say f_ruleset\n");
}

// Intercept a say from a player whose ruleset probe is pending. Returns true if
// the line was the f_ruleset reply (and was therefore consumed/suppressed).
// Only lines that look like an f_ruleset reply (contain "ruleset") are treated
// as the answer, so ordinary chat in the window isn't mistaken for it.
qbool mm_capture_ruleset_reply(gedict_t *p, const char *say_text)
{
	char lc[256], want[64];

	if (!p || p->mm_rs_deadline <= 0 || !is_matchmade_server())
	{
		return false;
	}
	mm_lower(cvar_string("k_mm_require_ruleset"), want, sizeof(want));
	if (!want[0] || !say_text)
	{
		return false;
	}

	mm_lower(say_text, lc, sizeof(lc));
	if (!strstr(lc, "ruleset"))
	{
		return false;  // not an f_ruleset reply — let normal chat through
	}

	p->mm_rs_deadline = 0;  // resolved either way
	if (strstr(lc, want))
	{
		G_sprint(p, 2, "%s\n", redtext(va("Ruleset verified: %s.", want)));
	}
	else
	{
		G_sprint(p, 2,
				"%s\n"
				"This matchmade game requires \"ruleset %s\".\n"
				"Set it in your client (/ruleset %s) and reconnect.\n",
				redtext("wrong ruleset"), want, want);
		stuffcmd(p, "disconnect\n");
	}
	return true;  // consumed — keep it out of public chat
}

// Frame tick: handle players whose ruleset-probe window expired with no
// parseable reply. Fail-open unless k_mm_ruleset_strict is set (then kick), so
// a quirk in client replies can't lock an entire match's roster out.
void mm_check_ruleset_deadlines(void)
{
	gedict_t *p;
	char want[64];

	if (!is_matchmade_server())
	{
		return;
	}
	mm_lower(cvar_string("k_mm_require_ruleset"), want, sizeof(want));
	if (!want[0])
	{
		return;
	}

	for (p = world; (p = find_client(p));)
	{
		if (p->mm_rs_deadline <= 0 || g_globalvars.time < p->mm_rs_deadline)
		{
			continue;
		}
		p->mm_rs_deadline = 0;
		if ((int) cvar("k_mm_ruleset_strict"))
		{
			G_sprint(p, 2,
					"%s\n"
					"Could not verify your client ruleset. This game requires\n"
					"\"ruleset %s\" - set it (/ruleset %s) and reconnect.\n",
					redtext("ruleset check failed"), want, want);
			stuffcmd(p, "disconnect\n");
		}
		else
		{
			G_sprint(p, 2, "%s\n",
					redtext(va("Could not verify ruleset (expected %s) - continuing.",
							want)));
		}
	}
}

// Returns true if exactly two non-bot players are in the game.
// Fill empty slots with bots up to k_mm_players. Dev/testing only — gated by
// k_mm_bots (0 in production, so this is a no-op there). Lets a single human
// exercise a full team match. Bots auto-balance onto the smaller team.
void mm_fill_bots(void)
{
#ifdef BOT_SUPPORT
	extern void FrogbotsAddbot(int skill_level, const char *specificteam,
			qbool error_messages);
	int want = (int) cvar("k_mm_bots");
	int needed = (int) cvar("k_mm_players");
	int skill = (int) cvar("k_mm_bot_skill");
	int total = 0, guard = 0;
	gedict_t *p;

	if (want <= 0 || !is_matchmade_server())
	{
		return;
	}
	if (match_in_progress || match_over)
	{
		return;
	}
	if (needed < 2)
	{
		needed = 2;
	}
	if (skill <= 0)
	{
		skill = 14;
	}

	for (p = world; (p = find_plr(p));)
	{
		total++;
	}
	// Only fill once at least one human is present, so bots balance around the
	// real player rather than forming lopsided teams before they arrive.
	while (total < needed && guard++ < 8)
	{
		FrogbotsAddbot(skill, "", false);  // "" => auto-balance team
		total++;
	}
#endif
}

static qbool mm_has_both_players(void)
{
	int human_players = 0;
	int total_players = 0;
	int needed = (int) cvar("k_mm_players");
	gedict_t *p;
	// k_mm_players is set by the agent (2 for 1on1, 4 for 2on2). Default to 2
	// for older configs that don't set it.
	if (needed < 2)
	{
		needed = 2;
	}
	for (p = world; (p = find_plr(p));)
	{
		total_players++;
		if (!p->isBot)
		{
			human_players++;
		}
	}
	// Production (no bots): total == humans, so this is the original check.
	// With bot-fill: start once the roster is full and at least one human is
	// present to watch/play.
	return human_players >= 1 && total_players >= needed;
}

static int mm_human_count(void)
{
	int humans = 0;
	gedict_t *p;
	for (p = world; (p = find_plr(p));)
	{
		if (!p->isBot)
		{
			humans++;
		}
	}
	return humans;
}

// Arm the abandonment watchdog if a matchmade server has dropped below the
// needed player count BEFORE the match started. No-op if a match is running, if
// one is already armed, or if the initial join-deadline ticker is still up
// (that one handles "not everyone has arrived yet").
static void mm_arm_abandon(void)
{
	gedict_t *w;
	int left;

	if (!is_matchmade_server() || match_in_progress || match_over)
	{
		return;
	}
	if (find(world, FOFCLSN, "mm_abandon") || find(world, FOFCLSN, "mm_join_deadline"))
	{
		return;
	}
	w = spawn();
	w->classname = "mm_abandon";
	w->think = (func_t) mm_abandon_think;
	w->s.v.nextthink = g_globalvars.time + 1;
	// Resumes the map's shared waiting budget — NOT a fresh countdown. If the
	// budget is already spent the watchdog is still armed: it resolves the
	// session (forfeit/void + shutdown) on its next tick, so the server never
	// lingers waiting for the multi-hour orphan reaper.
	left = mm_wait_left;
	if (left <= 0)
	{
		G_bprint(PRINT_HIGH, "%s\n",
				redtext("A player left during warmup - the wait time for this map "
						"is used up, resolving now."));
	}
	else if ((int) cvar("k_series_index") > 0)
	{
		// Mid-series: a map is already decided, so the stakes are a forfeit.
		G_bprint(PRINT_HIGH, "%s\n",
				redtext(va("A player left during warmup - %d:%02d to reconnect "
						"or they forfeit the series.", left / 60, left % 60)));
	}
	else
	{
		// Map 1, nothing decided: leaving and re-entering during warmup is free.
		// Say so — the old wording ("60 seconds or the match is aborted") read
		// as a punishment for using the break window as intended.
		G_bprint(PRINT_HIGH, "%s\n",
				redtext(va("A player left during warmup - %d:%02d to reconnect. "
						"Warmup resumes where it left off if they return.",
						left / 60, left % 60)));
	}
}

// Warmup ticker. Spawned once everyone's connected. Players are unreadied (so
// they can move/shoot/prewar) until the map's warmup budget (mm_warmup_left) runs
// out, then we force-ready and start the real countdown. Cancels itself if the
// roster drops — the budget stops draining and the abandon watchdog decides
// whether to wait for a return or shut down, so warmup RESUMES on a rejoin.
void mm_prewar_think(void)
{
	int needed = (int) cvar("k_mm_players");
	int delay;
	gedict_t *p;

	if (needed < 2)
	{
		needed = 2;
	}
	if (match_in_progress || match_over || intermission_running)
	{
		ent_remove(self);
		return;
	}
	if (mm_human_count() < needed)
	{
		ent_remove(self);  // someone left during warmup — abandon watchdog handles it
		return;
	}
	// Ready-gate: start the moment every present human is ready (don't wait out
	// the clock); otherwise keep counting down and auto-start at expiry. The
	// warmup doubles as the (re)connect + break window — not readying is a break,
	// bounded by the warmup length.
	{
		int total = 0, rdy = 0;
		for (p = world; (p = find_plr(p));)
		{
			if (p->isBot)
			{
				continue;
			}
			total++;
			if (p->ready)
			{
				rdy++;
			}
		}
		if (!(total > 0 && rdy >= total) && mm_warmup_left > 0)
		{
			int cs = mm_warmup_left;
			G_cp2all("WARMUP - type \"ready\"\n\n%d / %d ready\n\nstarts in %d:%02d",
					rdy, total, cs / 60, cs % 60);
			mm_warmup_left -= 1;
			self->s.v.nextthink = g_globalvars.time + 1;
			return;
		}
	}

	// All ready (or warmup expired) — ready everyone and start the real countdown.
	G_cp2all(" ");
	for (p = world; (p = find_plr(p));)
	{
		if (!p->isBot)
		{
			p->ready = 1;
		}
	}
	delay = (int) bound(3, cvar("k_match_start_delay"), 60);
	cvar_fset("k_count", delay);
	G_bprint(PRINT_HIGH, "%s\n",
			redtext(va("Warmup over - match starts in %d seconds.", delay)));
	StartTimer();
	ent_remove(self);
}

// (mm_forfeit_outcome / mm_set_forfeit_team_from_token / mm_notify_forfeit are
// forward-declared earlier, near mm_join_deadline_think.)

// Abandonment watchdog (see mm_arm_abandon). If players don't come back to the
// needed count before the warmup/countdown grace expires, end the session and
// tell the backend so the players are freed right away (rather than lingering
// until the multi-hour orphan reaper), then shut the server down.
//
// Series-aware: voiding with no Elo is only correct when NOTHING has been
// decided yet (map 1 / 0-0) or both sides are gone (no winner). If we are in a
// LATER map's warmup (a map already decided) and exactly ONE side is missing,
// voiding would throw away the decided maps AND let a player drop during warmup
// to dodge a likely loss — so instead we forfeit the remaining series to the
// present team (they get the win + Elo; the leaver gets the abandon cooldown).
void mm_abandon_think(void)
{
	int needed = (int) cvar("k_mm_players");
	gedict_t *sd;
	char loser[64] = "";

	if (needed < 2)
	{
		needed = 2;
	}
	if (match_in_progress || match_over)
	{
		ent_remove(self);
		return;
	}
	if (mm_human_count() >= needed)
	{
		ent_remove(self);  // everyone's back; mm_maybe_auto_start resumed warmup
		return;
	}
	if (mm_wait_left > 0)
	{
		int left = mm_wait_left;

		// Keep the present player informed — the wait must never look like a
		// hang, and the consequence must be stated (session 414 fallout: the
		// grace was invisible while the website still showed 5:00).
		if ((left % 60 == 0) || (left == 30) || (left == 10))
		{
			if ((int) cvar("k_series_index") > 0)
			{
				G_bprint(PRINT_HIGH, "%s\n",
						redtext(va("Waiting for missing player - %d:%02d until "
								"the series is forfeited to the present team.",
								left / 60, left % 60)));
			}
			else
			{
				G_bprint(PRINT_HIGH, "%s\n",
						redtext(va("Waiting for missing player - %d:%02d to "
								"reconnect, then the match is voided (no Elo).",
								left / 60, left % 60)));
			}
		}
		// Nudge a rostered player idling in the spectator slots (every ~15s).
		if (left % 15 == 0)
		{
			mm_warn_spectating_missing();
		}
		mm_wait_left -= 1;
		self->s.v.nextthink = g_globalvars.time + 1;
		return;
	}

	{
		int outcome = mm_forfeit_outcome(loser, sizeof(loser));

		if ((int) cvar("k_series_index") > 0 && outcome == 1)
		{
			// Mid-series, one side missing → forfeit the rest of the series to
			// the present team instead of voiding it.
			mm_set_forfeit_team_from_token(loser);
			cvar_set("k_match_forfeit_loser", loser);
			G_bprint(PRINT_HIGH, "%s\n",
					redtext("A player abandoned mid-series - forfeiting the remaining maps "
							"to the present team."));
			mm_notify_forfeit();
		}
		else
		{
			// Map-1 warmup (nothing decided) or both sides gone → no-Elo void.
			// Still NAME the abandoner when one side is missing: the backend
			// applies the escalating abandon cooldown to them and re-queues
			// only the innocent players. Without this, a griefer could ready,
			// connect, idle out warmup and re-queue with full priority forever
			// while each opponent pays the wait.
			if (outcome == 1 && loser[0])
			{
				cvar_set("k_match_forfeit_loser", loser);
			}
			else
			{
				// Both sides gone / unresolvable: make sure a STALE loser from
				// an earlier map's forfeit (cvars persist across map loads and
				// StartMatch hasn't run yet during warmup) can't leak into the
				// abort payload and cooldown the wrong player.
				cvar_set("k_match_forfeit_loser", "");
			}
			G_bprint(PRINT_HIGH, "%s\n",
					redtext("Players left before the match started - aborting, no Elo change."));
			cvar_fset("k_match_aborted", 1);
			mm_notify_aborted();
		}
	}

	sd = spawn();
	sd->classname = "mm_shutdown";
	sd->think = (func_t) mm_shutdown_think;
	sd->s.v.nextthink = g_globalvars.time + 1;
	sd->cnt2 = 15;  // sv_web_postfile is async — same flush window as the
	                // map-end stats upload. 5s lost a series forfeit to the
	                // quit race (session 414): the payload file was written
	                // but the POST never left the box.
	ent_remove(self);
}

// Called from PutClientInServer when a player on a matchmade server is fully in
// the game. Once everyone's present, kick off the warmup window (which then
// readies players and starts the countdown). Guards against re-arming.
void mm_maybe_auto_start(void)
{
	if (!is_matchmade_server())
	{
		return;
	}
	if (match_in_progress || match_over || intermission_running)
	{
		return;
	}
	// Dev bot-fill: top up empty slots with bots (no-op when k_mm_bots == 0).
	mm_fill_bots();
	if (!mm_has_both_players())
	{
		return;
	}
	// Already in warmup? leave it running.
	if (find(world, FOFCLSN, "mm_prewar"))
	{
		return;
	}

	// Start (or RESUME) the warmup window. Players stay unreadied (free to
	// move/shoot) until the map's warmup budget elapses, then mm_prewar_think
	// readies everyone and starts the countdown. After a leave/rejoin this picks
	// up the remaining budget — it does not hand out a fresh full-length warmup.
	{
		int warm = mm_warmup_left;
		gedict_t *pw = spawn();
		pw->classname = "mm_prewar";
		pw->think = (func_t) mm_prewar_think;
		pw->s.v.nextthink = g_globalvars.time + 1;
		G_bprint(PRINT_HIGH, "%s\n",
				redtext(va("All players connected - warmup. Type \"ready\"; the map starts "
						"when everyone is ready, or automatically in %d:%02d.",
						warm / 60, warm % 60)));
	}
}

// Forfeit timer state (real-time, runs during engine pause). Set when a
// player disconnects mid-match; cleared on resolve.
static qbool mm_forfeit_active = false;
static int mm_forfeit_deadline_ms = 0;
static int mm_forfeit_announce_sec = 0;  // last announced 'seconds remaining'
// Pause-clock value (PausedTic's duration_ms) at the moment this window armed.
// The engine's pause clock is TOTAL time paused, not window time — a window
// armed during an existing manual pause would otherwise have its return time
// silently eaten by however long the pause had already run (80s of manual
// pause + a drop = a 10s "90s window"), and the correlated-drop gap would be
// measured from the pause start instead of the window start.
static int mm_forfeit_start_ms = 0;
// How many disconnect technical-timeouts each PLAYER (by token) has been
// granted this map. Each disconnect pauses the match and gives a return
// window; reconnecting before the deadline resumes. Without a cap, a losing
// player could disconnect/return in a loop forever, freezing the clock so the
// match never ends (then dodge the loss via the backend's orphan-abort). After
// this many cycles, a further disconnect by THAT player forfeits them
// immediately instead of granting a window. Tracked per token — a global
// counter would let one flaky player consume the budget and get the NEXT
// (first-time) dropper insta-forfeited for someone else's disconnects.
#define MM_MAX_FORFEIT_CYCLES 3
static char mm_cycle_tok[16][64];
static int mm_cycle_cnt[16];
static int mm_cycle_n = 0;

static int mm_forfeit_cycles_of(const char *tok)
{
	int i;

	for (i = 0; i < mm_cycle_n; i++)
	{
		if (streq(mm_cycle_tok[i], tok))
		{
			return mm_cycle_cnt[i];
		}
	}
	return 0;
}

static void mm_forfeit_cycles_bump(const char *tok)
{
	int i;

	if (!tok[0])
	{
		return;
	}
	for (i = 0; i < mm_cycle_n; i++)
	{
		if (streq(mm_cycle_tok[i], tok))
		{
			mm_cycle_cnt[i]++;
			return;
		}
	}
	if (mm_cycle_n < (int) (sizeof(mm_cycle_cnt) / sizeof(mm_cycle_cnt[0])))
	{
		strlcpy(mm_cycle_tok[mm_cycle_n], tok, sizeof(mm_cycle_tok[0]));
		mm_cycle_cnt[mm_cycle_n] = 1;
		mm_cycle_n++;
	}
}

static void mm_forfeit_cycles_reset(void)
{
	mm_cycle_n = 0;
}

// qwleague: token of the FIRST player to abandon the current live map. In a
// 1on1 this is who loses if the match later ends with the roster unresolvable
// — the abandoner must not be able to convert a decided-by-abandonment result
// into a no-Elo abort by having the (innocent) opponent also leave the frozen
// match. Cleared when the roster is fully restored (blip forgiven) and each
// map in StartMatch.
static char mm_first_leaver[64] = "";

// Correlated-drop detection. When the SECOND player drops within 5s (real
// time) of the first, the outage almost certainly hit both (server box /
// upstream blip), so the both-missing case stays a no-fault abort instead of
// blaming whichever client the engine happened to sweep first. Real time is
// tracked via mm_paused_tic's duration_ms — game time is frozen during the
// pause, so it can't measure this gap.
#define MM_CORRELATED_DROP_MS 5000
static qbool mm_drop_correlated = false;
static int mm_last_pause_ms = 0;  // duration_ms as of the last PausedTic
// Last technical-timeout centerprint we actually sent, so PausedTic (every
// frame) only re-sends when the text changes — see mm_paused_tic.
static char mm_paused_cp_last[192] = "";

// Accessors so the pause/extend command (commands.c) can see and extend the
// disconnect technical-timeout without exposing the statics.
qbool mm_forfeit_is_active(void)
{
	return mm_forfeit_active;
}

void mm_extend_forfeit_deadline(int ms)
{
	if (mm_forfeit_active)
	{
		mm_forfeit_deadline_ms += ms;
		// Re-arm milestone announcements so the new time is shown.
		mm_forfeit_announce_sec = 0;
  mm_paused_cp_last[0] = 0;  // let the next window print immediately
	}
}

static void mm_token_team(const char *want, char *out, int out_sz);

// ── Per-map LINEUP (qwleague) ──────────────────────────────────────────────
// k_allowed_tokens is the CONNECT gate, NOT the roster in play. For a queue
// match the two are the same list, but an arranged session (clan tournament
// fixture or prac, docs/adr/0006) is spawned with each clan's WHOLE roster —
// fixture 161 on 2026-09-16 carried 15 allowed tokens for an 8-player 4on4 —
// because any rostered member may play or substitute. Judging presence against
// that list counts every benched clan member as a missing player, so a
// technical timeout in a tournament match can never resolve to "everyone's
// back": the dropped player rejoined, all 8 were on the server, and KTX still
// demanded a "proceed" vote and was heading for a no-Elo abort.
//
// So snapshot the players actually in the game when the map goes live and judge
// presence against THAT, per side and by HEADCOUNT: a side is short only while
// it has fewer players on the server than it started the map with. Counting
// heads rather than matching token identity is what makes a substitution work —
// a bench player who takes the dropped player's slot fills the deficit.
#define MM_MAX_LINEUP 16
static char mm_lineup[MM_MAX_LINEUP][64];
static int mm_lineup_n = 0;

static void mm_lineup_reset(void)
{
	mm_lineup_n = 0;
}

// Called from StartMatch: this map's starting lineup.
static void mm_lineup_snapshot(void)
{
	gedict_t *p;

	mm_lineup_n = 0;

	if (!is_matchmade_server())
	{
		return;
	}

	for (p = world; (p = find_plr(p)) && mm_lineup_n < MM_MAX_LINEUP;)
	{
		const char *tok = mm_player_token(p);

		if (p->isBot || !tok[0])
		{
			continue;  // bots carry no match token, and never had a slot to miss
		}
		strlcpy(mm_lineup[mm_lineup_n++], tok, 64);
	}
}

// Is any connected player holding this token right now?
static qbool mm_token_connected(const char *tok)
{
	gedict_t *p;

	if (!tok[0])
	{
		return false;
	}
	for (p = world; (p = find_plr(p));)
	{
		if (streq(mm_player_token(p), tok))
		{
			return true;
		}
	}
	return false;
}

// The roster to judge presence against: this map's lineup once the map is
// live, else (pre-match) the allowed list, which is all we have to name people
// by before anyone has taken a slot.
static int mm_roster_tokens(char out[][64], int max_out)
{
	const char *p;
	char tok[64];
	size_t i;
	int n = 0;

	if (mm_lineup_n > 0)
	{
		for (n = 0; n < mm_lineup_n && n < max_out; n++)
		{
			strlcpy(out[n], mm_lineup[n], 64);
		}
		return n;
	}

	p = cvar_string("k_allowed_tokens");
	while (*p && n < max_out)
	{
		while (*p == ' ' || *p == '\t' || *p == ',')
		{
			p++;
		}
		i = 0;
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && i < sizeof(tok) - 1)
		{
			tok[i++] = *p++;
		}
		tok[i] = 0;
		if (i == 0)
		{
			continue;
		}
		strlcpy(out[n++], tok, 64);
	}
	return n;
}

// Which side a token plays for, "" when there is no team map at all. `teamed`
// off means one single bucket: every token belongs to the same (nameless) side.
static qbool mm_side_of(const char *tok, const char *side, qbool teamed)
{
	char team[32];

	if (!teamed)
	{
		return true;
	}
	mm_token_team(tok, team, sizeof(team));
	return streq(team, side);
}

// Returns how many SLOTS are currently unfilled, i.e. for each side, how far
// its headcount has fallen below what it started the map with (before the match
// starts, below the mode's per-side size). Fills `out[i]` with a roster token
// for each unfilled slot — someone on that side who is genuinely not on the
// server — so callers can name and blame the absent player. The first leaver is
// listed first, so forfeit attribution names whoever actually walked out rather
// than an arbitrary team-mate.
static int mm_missing_tokens(char out[][64], int max_out)
{
	char roster[MM_MAX_LINEUP][64];
	int rn = mm_roster_tokens(roster, MM_MAX_LINEUP);
	qbool teamed = (cvar_string("k_token_teams")[0] != 0);
	const char *sides[2];
	int nsides = (teamed ? 2 : 1);
	int missing = 0;
	int si, i;

	sides[0] = (teamed ? mm_series_team1() : "");
	sides[1] = (teamed ? mm_series_team2() : "");

	for (si = 0; si < nsides && missing < max_out; si++)
	{
		int expected = 0;
		int present = 0;
		int deficit;
		qbool leaver_listed = false;
		gedict_t *p;

		if (mm_lineup_n > 0)
		{
			for (i = 0; i < rn; i++)
			{
				if (mm_side_of(roster[i], sides[si], teamed))
				{
					expected++;
				}
			}
		}
		else
		{
			// No lineup yet (waiting / warmup): the mode's own size is what this
			// side owes.
			int need = (int) cvar("k_mm_players");

			if (need < 2)
			{
				need = 2;
			}
			expected = (teamed ? need / 2 : need);
		}

		// Heads on the server for this side. ANY allowed token counts, not just
		// the ones in the lineup, so a rostered substitute fills the slot of the
		// player they came on for.
		for (p = world; (p = find_plr(p));)
		{
			if (p->isBot)
			{
				continue;
			}
			if (mm_side_of(mm_player_token(p), sides[si], teamed))
			{
				present++;
			}
		}

		deficit = expected - present;

		if ((deficit > 0) && mm_first_leaver[0] && missing < max_out
				&& mm_side_of(mm_first_leaver, sides[si], teamed)
				&& !mm_token_connected(mm_first_leaver))
		{
			strlcpy(out[missing++], mm_first_leaver, 64);
			leaver_listed = true;
			deficit--;
		}

		for (i = 0; (i < rn) && (deficit > 0) && (missing < max_out); i++)
		{
			if (leaver_listed && streq(roster[i], mm_first_leaver))
			{
				continue;
			}
			if (!mm_side_of(roster[i], sides[si], teamed))
			{
				continue;
			}
			if (mm_token_connected(roster[i]))
			{
				continue;
			}
			strlcpy(out[missing++], roster[i], 64);
			deficit--;
		}
	}

	return missing;
}

// Look up the forced team ("red"/"blue") for a token from k_token_teams — the
// same "<token> <team> <token> <team> ..." list CanConnect uses to place
// players. Writes "" if the token has no entry (e.g. 1on1, where the cvar is
// empty).
static void mm_token_team(const char *want, char *out, int out_sz)
{
	const char *p = cvar_string("k_token_teams");
	char tok[64], team[32];
	size_t i;

	out[0] = 0;
	while (*p)
	{
		while (*p == ' ' || *p == '\t' || *p == ',')
		{
			p++;
		}
		i = 0;
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && i < sizeof(tok) - 1)
		{
			tok[i++] = *p++;
		}
		tok[i] = 0;
		while (*p == ' ' || *p == '\t' || *p == ',')
		{
			p++;
		}
		i = 0;
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && i < sizeof(team) - 1)
		{
			team[i++] = *p++;
		}
		team[i] = 0;
		if (tok[0] && team[0] && streq(tok, want))
		{
			strlcpy(out, team, out_sz);
			return;
		}
	}
}

// Decide the pending forfeit outcome from who is currently missing:
//   0 = everyone present (resume)
//   1 = forfeit  (one side is incomplete; *loser set to a token on that side)
//   2 = abort    (both sides incomplete, or unresolvable — no Elo change)
// Team-aware for 2on2/4on4: a side loses ONLY if THAT side is missing a player.
// This closes the 1on1-shaped loophole where any two disconnects aborted the
// match with no Elo — a losing team can no longer dodge a loss by having two of
// its own players quit (only their side is missing → they forfeit). Falls back
// to the 1on1 rule (1 missing = forfeit, 2 = abort) when there is no team map.
static int mm_forfeit_outcome(char *loser, int loser_sz)
{
	char missing[16][64];
	int n;
	int i;
	qbool red = false, blue = false;
	char red_tok[64] = "", blue_tok[64] = "";
	char team[32];

	n = mm_missing_tokens(missing, 16);
	if (n == 0)
	{
		return 0;
	}

	if (cvar_string("k_token_teams")[0])
	{
		// red/blue here mean backend team 1/2 — resolved via k_series_team1/2
		// (clan matches carry clan tags as the team strings; the old literal
		// "red"/"blue" comparisons resolved NO team there, so every clan-match
		// disconnect fell through to the "abort safely" outcome and the leaver
		// dodged the forfeit).
		for (i = 0; i < n; i++)
		{
			mm_token_team(missing[i], team, sizeof(team));
			if (streq(team, mm_series_team1()))
			{
				red = true;
				if (!red_tok[0])
				{
					strlcpy(red_tok, missing[i], sizeof(red_tok));
				}
			}
			else if (streq(team, mm_series_team2()))
			{
				blue = true;
				if (!blue_tok[0])
				{
					strlcpy(blue_tok, missing[i], sizeof(blue_tok));
				}
			}
		}

		if (red && blue)
		{
			// Both sides incomplete. NOTE: production duels take THIS branch,
			// not the no-team-map fallback below — the agent renders
			// k_token_teams for every mode (spawn.py assigns team 1/2 to duel
			// players too). Blindly aborting here was the loophole where a
			// losing duellist quits and the opponent, staring at a frozen
			// technical timeout, also leaves — voiding the match with no Elo.
			// If we recorded who abandoned FIRST (and it wasn't a correlated
			// infra drop), their side forfeits. Guarded on the first leaver
			// actually being among the missing so a returned player is never
			// blamed for others' absence.
			if (mm_first_leaver[0] && !mm_drop_correlated)
			{
				for (i = 0; i < n; i++)
				{
					if (streq(missing[i], mm_first_leaver))
					{
						if (loser)
						{
							strlcpy(loser, mm_first_leaver, loser_sz);
						}
						return 1;
					}
				}
			}
			return 2;  // no liable first leaver → abort
		}
		if (red)
		{
			if (loser)
			{
				strlcpy(loser, red_tok, loser_sz);
			}
			return 1;
		}
		if (blue)
		{
			if (loser)
			{
				strlcpy(loser, blue_tok, loser_sz);
			}
			return 1;
		}
		return 2;  // missing tokens had no resolvable team → abort safely
	}

	// 1on1: no team mapping. One missing = forfeit the missing player — a
	// PRESENT player is never blamed, even when they abandoned earlier and
	// returned (blaming someone standing on the server for a map their
	// opponent is absent from is indefensible, and it would punish good-faith
	// reconnects). The residual gambit — B quits, waits for A to leave the
	// frozen match, then reconnects so A is the one missing — requires A to
	// ignore the explicit "stay connected or YOU forfeit" warning printed
	// during every technical timeout, and each such quit burns one of B's
	// MM_MAX_FORFEIT_CYCLES.
	if (n == 1)
	{
		if (loser)
		{
			strlcpy(loser, missing[0], loser_sz);
		}
		return 1;
	}

	// Both missing (n >= 2). Previously this aborted with no Elo — the loophole
	// where a losing duellist quits and then the opponent, staring at a frozen
	// technical-timeout, also leaves, voiding the match. If we recorded who
	// abandoned FIRST, they forfeit; the opponent leaving a match that was
	// already lost-by-abandonment cannot launder it into a no-fault abort.
	// Exception: both dropped within seconds of each other — that's a shared
	// outage, not an abandonment; keep the no-fault abort.
	if (mm_first_leaver[0] && !mm_drop_correlated)
	{
		if (loser)
		{
			strlcpy(loser, mm_first_leaver, loser_sz);
		}
		return 1;
	}
	return 2;
}

// ── No-fault early abort + vote-to-proceed (qwleague) ──────────────────────
// True only on MAP 1, within k_mm_abort_window seconds of the match starting —
// the grace where an unrecovered disconnect VOIDS the match (no Elo) instead of
// forfeiting a map, because nothing has been decided yet.
static qbool mm_in_abort_window(void)
{
	int win = (int) bound(0, cvar("k_mm_abort_window"), 1800);
	if ((int) cvar("k_series_index") != 0)
	{
		return false;  // only the very first map
	}
	if (match_start_time <= 0)
	{
		return false;
	}
	return (g_globalvars.time - match_start_time) < (float) win;
}

// Record which team forfeited (1/2 in the brain's numbering, resolved via
// k_series_team1/2 — clan matches carry clan tags, not "red"/"blue") from a
// player token, so EndMatch can credit the OTHER team with this map's win.
static void mm_set_forfeit_team_from_token(const char *tok)
{
	char team[32];
	int t = 0;
	mm_token_team((char *) tok, team, sizeof(team));
	if (streq(team, mm_series_team1()))
	{
		t = 1;
	}
	else if (streq(team, mm_series_team2()))
	{
		t = 2;
	}
	cvar_fset("k_match_forfeit_team", t);
}

// qwleague: resolve a still-pending disconnect forfeit when EndMatch fires
// without mm_paused_tic having run — the empty-server teardown ends the match
// as soon as the last player leaves, before the technical-timeout deadline. If
// the outcome is already decided (mm_paused_tic, the cycle cap, admin abort, or
// a series abandon set the cvars) we leave it untouched. Otherwise we classify
// the current roster exactly like mm_paused_tic's deadline branch so the stats
// POST carries the forfeit/abort — a match that emptied out mid-play can no
// longer post a resultless upload that dodges Elo.
static void mm_resolve_pending_forfeit_on_end(void)
{
	char loser[64] = "";
	int outcome;

	// ONLY the empty-server teardown path. With anyone still connected this
	// EndMatch is either a natural end (possibly after a unanimous "proceed"
	// vote played the map out short-handed — stamping the absent player would
	// override a legitimately earned result) or an operator shutdown
	// (G_ShutDown → EndMatch), which must stay resultless rather than blame
	// whoever happened to be mid-reconnect.
	if (CountPlayers() != 0)
	{
		return;
	}

	if (cvar("k_match_aborted") || cvar_string("k_match_forfeit_loser")[0])
	{
		return;  // already resolved elsewhere — don't override
	}

	outcome = mm_forfeit_outcome(loser, sizeof(loser));
	if (outcome == 0)
	{
		return;  // everyone present — a natural (frag/time) end, nothing to forfeit
	}

	if (outcome == 1)
	{
		cvar_set("k_match_forfeit_loser", loser);
		if (mm_in_abort_window())
		{
			// Map 1, within the no-fault grace: void with no Elo, but the named
			// leaver still takes the backend abandon cooldown.
			cvar_fset("k_match_aborted", 1);
		}
		else
		{
			mm_set_forfeit_team_from_token(loser);
		}
	}
	else
	{
		cvar_fset("k_match_aborted", 1);  // unresolvable — abort, no Elo
	}
}

// Vote-to-proceed: present members of the short-handed team may resume the map a
// man down rather than wait out the technical timeout. Votes (by token) are
// collected per timeout and cleared when a new one starts.
static char mm_proceed_voters[16][64];
static int mm_proceed_n = 0;

static void mm_proceed_reset(void)
{
	mm_proceed_n = 0;
}

static qbool mm_has_voted(const char *tok)
{
	int i;
	for (i = 0; i < mm_proceed_n; i++)
	{
		if (streq(mm_proceed_voters[i], tok))
		{
			return true;
		}
	}
	return false;
}

// Called from PausedTic each frame during engine pause. Runs in REAL time
// (via the `duration_ms` argument the engine provides). Resolves the matchmade
// forfeit via mm_forfeit_outcome (team-aware for 2on2/4on4):
//   0 → everyone back, unpause (existing 3s engine grace handles resume)
//   1 → forfeit at deadline, the incomplete side loses Elo
//   2 → abort at deadline, no Elo change
void mm_paused_tic(int duration_ms)
{
	int left_ms;
	int left_sec;
	int outcome;
	char loser[64] = "";
	extern int when_to_unpause;
	extern int pauseduration;
	extern int mm_extends_used;

	if (!mm_forfeit_active)
	{
		return;
	}
	if (match_over || !match_in_progress)
	{
		mm_forfeit_active = false;
  mm_paused_cp_last[0] = 0;  // let the next window print immediately
		return;
	}

	mm_last_pause_ms = duration_ms;  // real time elapsed in this pause

	left_ms = mm_forfeit_deadline_ms - duration_ms;
	left_sec = left_ms > 0 ? (left_ms + 999) / 1000 : 0;

	outcome = mm_forfeit_outcome(loser, sizeof(loser));

	if (outcome == 0)
	{
		// Everyone returned — schedule the engine's standard 3s unpause grace.
		// The roster is complete again, so the recovered blip is forgiven:
		// clear the first-leaver record. Without this, a player whose wifi
		// blipped at 2:00 would remain "first leaver" for the whole map and
		// eat the blame if the OPPONENT later abandons and both end up gone.
		mm_forfeit_active = false;
  mm_paused_cp_last[0] = 0;  // let the next window print immediately
		mm_first_leaver[0] = 0;
		mm_drop_correlated = false;
		G_bprint(PRINT_HIGH, "%s\n",
				redtext("All players present - resuming in 3 seconds."));
		when_to_unpause = pauseduration + 3000;
		return;
	}

	if (left_ms <= 0)
	{
		// Deadline expired. Resolve based on who's missing. Leave the engine
		// paused — EndMatch fires now anyway and stats POST is unaffected.
		mm_forfeit_active = false;
  mm_paused_cp_last[0] = 0;  // let the next window print immediately
		if (outcome == 1)
		{
			if (mm_in_abort_window())
			{
				// No-fault early abort (map 1): void with no Elo, but record the
				// leaver so the backend applies their abandon cooldown.
				G_bprint(PRINT_HIGH, "%s\n",
						redtext("Early disconnect - match aborted, no Elo change."));
				cvar_set("k_match_forfeit_loser", loser);
				cvar_fset("k_match_aborted", 1);
			}
			else
			{
				// Past the early window / later map: forfeit only THIS map; the
				// series continues (next map's warmup is the return window).
				G_bprint(PRINT_HIGH, "%s\n",
						redtext("Technical timeout expired - map forfeited."));
				cvar_set("k_match_forfeit_loser", loser);
				mm_set_forfeit_team_from_token(loser);
			}
		}
		else
		{
			G_bprint(PRINT_HIGH, "%s\n",
					redtext("Teams incomplete - match aborted, no Elo change."));
			cvar_fset("k_match_aborted", 1);
		}
		// Engine's pause stops EndMatch's nextthink usage, but EndMatch
		// itself can run synchronously here. Unpause briefly so subsequent
		// logic (timer cleanup, stats POST) ticks normally.
		trap_setpause(0);
		EndMatch(0);
		return;
	}

	// Centerprint a live countdown to all players still in the game, with the
	// recovery commands they can type. `extend` (+2:00, to wait for the dropped
	// player) is offered FIRST while the match still has extends left — without
	// advertising it, players don't know they can wait and the timeout silently
	// auto-resolves (the original bug: nobody knew to type it).
	//
	// ONLY when the text actually changes (i.e. once a second). A centerprint is a
	// RELIABLE message and PausedTic runs EVERY FRAME — at maxfps 77 the old
	// unconditional print queued ~77 reliable messages per second per client.
	// mvdsv's netchan cannot flush that, its back buffers fill, and the ENGINE
	// DROPS the client: "WARNING: MAX_BACK_BUFFERS for X" / "X overflowed". That
	// is what happened in match 589 (2026-07-27) — 11s into a technical timeout
	// the server dropped squeeze and then G-Flip, and mm_handle_disconnect then
	// processed them as players who "also disconnected", feeding correlated-drop
	// and forfeit attribution. The server was blaming players for its own flood.
	// (Vanilla PausedTic in commands.c has always throttled with `prevtime`.)
	{
		char cp[192];

		if (outcome == 1)
		{
			qbool team = (int) cvar("k_mm_players") > 2;  // proceed only in team modes
			qbool can_extend = (mm_extends_used < MAX_MATCH_EXTENDS);
			qbool can_abort = mm_in_abort_window();
			const char *label = can_abort ? "auto-abort" : "map forfeit";
			char hint[96];

			hint[0] = 0;
			if (can_extend)
			{
				strlcat(hint, "extend", sizeof(hint));
			}
			if (team)
			{
				if (hint[0]) { strlcat(hint, " / ", sizeof(hint)); }
				strlcat(hint, "proceed", sizeof(hint));
			}
			if (can_abort)
			{
				if (hint[0]) { strlcat(hint, " / ", sizeof(hint)); }
				strlcat(hint, "abort", sizeof(hint));
			}

			if (hint[0])
			{
				snprintf(cp, sizeof(cp), "TECHNICAL TIMEOUT\n\n%s in %d\n\ntype: %s",
						label, left_sec, hint);
			}
			else
			{
				snprintf(cp, sizeof(cp), "TECHNICAL TIMEOUT\n\n%s in %d",
						label, left_sec);
			}
		}
		else
		{
			snprintf(cp, sizeof(cp), "TECHNICAL TIMEOUT\n\nIncomplete - abort in %d",
					left_sec);
		}

		if (!streq(cp, mm_paused_cp_last))
		{
			G_cp2all("%s", cp);
			strlcpy(mm_paused_cp_last, cp, sizeof(mm_paused_cp_last));
		}
	}

	// Print to chat at milestones. Surface `extend` here too so it survives in
	// the scrollback even if a player missed the on-screen prompt.
	if (left_sec != mm_forfeit_announce_sec
		&& (left_sec == 60 || left_sec == 30 || left_sec == 15
		    || (left_sec <= 5 && left_sec > 0)))
	{
		if (outcome == 1)
		{
			if (mm_extends_used < MAX_MATCH_EXTENDS)
			{
				G_bprint(PRINT_HIGH, "%s\n",
						redtext(va("Forfeit in %d second%s - type \"extend\" to wait +2:00 for the missing player (%d left)",
								left_sec, (left_sec == 1 ? "" : "s"),
								MAX_MATCH_EXTENDS - mm_extends_used)));
			}
			else
			{
				G_bprint(PRINT_HIGH, "%s\n",
						redtext(va("Forfeit in %d second%s unless the missing player returns",
								left_sec, (left_sec == 1 ? "" : "s"))));
			}
		}
		else
		{
			G_bprint(PRINT_HIGH, "%s\n",
					redtext(va("Teams incomplete - abort in %d second%s",
							left_sec, (left_sec == 1 ? "" : "s"))));
		}
	}
	mm_forfeit_announce_sec = left_sec;
}

// Called from ClientDisconnect.
//  - Mid-countdown disconnect (state 1): abort the auto-start timer.
//  - Mid-match disconnect (state 2): start the forfeit timer if not already
//    running. A second disconnect during the timer simply lets it tick on —
//    when it expires, the missing-token count decides forfeit vs abort.
void mm_handle_disconnect(void)
{
	extern int mm_extends_used;

	if (!is_matchmade_server())
	{
		return;
	}

	if (match_in_progress == 1)
	{
		G_bprint(PRINT_HIGH, "%s\n",
				redtext("Opponent left during countdown - waiting for them to rejoin."));
		StopTimer(0);
		// Don't let an abandoned pre-match server linger until the orphan reaper.
		mm_arm_abandon();
		return;
	}

	if (match_in_progress == 2 && self->ct == ctPlayer)
	{
		// Remember who abandoned the live map first. This is the loser of record
		// if the match later ends unresolvable (e.g. the opponent also leaves).
		// Cleared when the roster is fully restored, and by StartMatch. Admin
		// kicks don't count — removed is not abandoned (idle kicks do count:
		// they go through stuffcmd-disconnect without the flag, and AFK on a
		// matchmade server IS abandonment).
		if (!mm_first_leaver[0] && !self->k_was_kicked)
		{
			strlcpy(mm_first_leaver, mm_player_token(self),
					sizeof(mm_first_leaver));
		}

		if (mm_forfeit_active)
		{
			// Second player gone within seconds of the first → treat the
			// outage as shared (infra), not as one side abandoning: the
			// both-missing resolution stays a no-fault abort. Measured from
			// the WINDOW start, not the pause start — they differ when the
			// window armed during an existing manual pause.
			if (mm_last_pause_ms - mm_forfeit_start_ms < MM_CORRELATED_DROP_MS)
			{
				mm_drop_correlated = true;
			}
			G_bprint(PRINT_HIGH, "%s %s\n",
					self->netname,
					redtext("also disconnected - technical timeout running."));
			return;
		}

		// Anti-stall: a player who has already used up THEIR allowed number of
		// disconnect windows gets no new one — they forfeit immediately. This
		// stops the disconnect/reconnect loop that would otherwise freeze the
		// clock indefinitely and let a loser dodge the result. Per-player: an
		// opponent's burned windows never count against this player.
		if (mm_forfeit_cycles_of(mm_player_token(self)) >= MM_MAX_FORFEIT_CYCLES)
		{
			char loser[64];
			strlcpy(loser, mm_player_token(self), sizeof(loser));
			if (mm_in_abort_window())
			{
				G_bprint(PRINT_HIGH, "%s %s\n", self->netname,
						redtext("disconnected repeatedly early - match aborted, no Elo change."));
				if (loser[0])
				{
					cvar_set("k_match_forfeit_loser", loser);
				}
				cvar_fset("k_match_aborted", 1);
			}
			else
			{
				G_bprint(PRINT_HIGH, "%s %s\n", self->netname,
						redtext(va("disconnected too many times (>%d) - map forfeited.",
								MM_MAX_FORFEIT_CYCLES)));
				if (loser[0])
				{
					cvar_set("k_match_forfeit_loser", loser);
					mm_set_forfeit_team_from_token(loser);
				}
				else
				{
					cvar_fset("k_match_aborted", 1);
				}
			}
			trap_setpause(0);
			EndMatch(0);
			return;
		}
		mm_forfeit_cycles_bump(mm_player_token(self));
		mm_proceed_reset();

		{
			int window_sec = (int) bound(30, cvar("k_match_forfeit_deadline"), 600);
			extern int when_to_unpause;
			extern int pauseduration;

			// One print, not five. Each G_bprint is a separate RELIABLE message to
			// every client, and this block fires at the exact moment the engine
			// gets paused — i.e. when the netchan is least able to flush. Same
			// text, same "announce the commands at every state" behaviour, a fifth
			// of the reliable pressure. See mm_paused_tic for the overflow this
			// class of traffic caused in match 589.
			{
				char msg[512];

				snprintf(msg, sizeof(msg),
						"%s\n%s\n",
						redtext(va("disconnected. Match paused - %d:%02d for them to return.",
								window_sec / 60, window_sec % 60)),
						redtext("Stay connected - the timeout resolves in your favor. "
								"If you leave too, YOU can be forfeited."));
				if ((int) cvar("k_mm_players") > 2)
				{
					strlcat(msg, redtext("Your team can type \"proceed\" to play on short-handed."),
							sizeof(msg));
					strlcat(msg, "\n", sizeof(msg));
				}
				if (mm_in_abort_window())
				{
					strlcat(msg, redtext("Early game: type \"abort\" to void it (no Elo) - auto-aborts if nobody returns."),
							sizeof(msg));
					strlcat(msg, "\n", sizeof(msg));
				}
				if (mm_extends_used < MAX_MATCH_EXTENDS)
				{
					strlcat(msg, redtext(va("Type \"extend\" to wait +2:00 longer for them (%d available).",
									MAX_MATCH_EXTENDS - mm_extends_used)),
							sizeof(msg));
					strlcat(msg, "\n", sizeof(msg));
				}
				G_bprint(PRINT_HIGH, "%s %s", self->netname, msg);
			}

			// Pause the engine so the match clock stops and the remaining player
			// can't gain frags. The forfeit countdown runs via PausedTic →
			// mm_paused_tic, which the engine calls each frame in real time.
			//
			// The window is anchored at the CURRENT pause clock: if the server
			// was already paused (manual pause) the engine's duration_ms keeps
			// counting from that pause's start, and an unanchored deadline
			// would be partly (or fully) consumed by pause time that predates
			// the disconnect. And a pending 3s resume (from a recovered blip or
			// an unpause request) must be cancelled — if it fired now, the
			// engine would unpause mid-window, PausedTic would stop ticking,
			// and the forfeit deadline would never resolve.
			when_to_unpause = 0;
			mm_forfeit_active = true;
   mm_paused_cp_last[0] = 0;  // let the next window print immediately
			mm_forfeit_start_ms = ((int) cvar("sv_paused") & 1) ? pauseduration : 0;
			mm_forfeit_deadline_ms = mm_forfeit_start_ms + window_sec * 1000;
			mm_forfeit_announce_sec = 0;
   mm_paused_cp_last[0] = 0;  // let the next window print immediately
			mm_last_pause_ms = mm_forfeit_start_ms;  // fresh window for correlated-drop timing
			mm_drop_correlated = false;
			trap_setpause(1);
		}
		return;
	}

	// Pre-match (warmup / waiting) disconnect: if the roster just dropped below
	// the needed count, make sure an abandoned server is reaped promptly rather
	// than lingering until the multi-hour orphan reaper.
	if (!match_in_progress)
	{
		mm_arm_abandon();
	}
}

// "proceed": the short-handed team votes to play the map out a man down instead
// of waiting out the technical timeout. Unanimous among that team's present
// players. Matchmade team modes only. (1on1 can't be played short-handed.)
void PlayerProceed(void)
{
	char missing[16][64];
	char shorthanded[32] = "";
	char myteam[32] = "";
	const char *mytok;
	gedict_t *p;
	int present = 0, voted = 0;
	extern int when_to_unpause;
	extern int pauseduration;

	if (!is_matchmade_server() || !mm_forfeit_active)
	{
		G_sprint(self, PRINT_HIGH, "%s\n", redtext("No technical timeout in progress."));
		return;
	}
	if ((int) cvar("k_mm_players") <= 2)
	{
		G_sprint(self, PRINT_HIGH, "%s\n",
				redtext("\"proceed\" is for team modes only - a duel can't continue a man down."));
		return;
	}
	if (mm_missing_tokens(missing, 16) == 0)
	{
		return;  // everyone's back; mm_paused_tic resumes
	}
	mm_token_team(missing[0], shorthanded, sizeof(shorthanded));
	mytok = mm_player_token(self);
	mm_token_team((char *) mytok, myteam, sizeof(myteam));
	if (!shorthanded[0] || !streq(myteam, shorthanded))
	{
		G_sprint(self, PRINT_HIGH, "%s\n",
				redtext("Only the team missing a player can vote to proceed."));
		return;
	}
	if (!mm_has_voted(mytok) && mm_proceed_n < 16)
	{
		strlcpy(mm_proceed_voters[mm_proceed_n++], mytok, 64);
	}
	for (p = world; (p = find_plr(p));)
	{
		char ptok[64], pteam[32];
		if (p->ct != ctPlayer)
		{
			continue;
		}
		strlcpy(ptok, mm_player_token(p), sizeof(ptok));
		mm_token_team(ptok, pteam, sizeof(pteam));
		if (!streq(pteam, shorthanded))
		{
			continue;
		}
		present++;
		if (mm_has_voted(ptok))
		{
			voted++;
		}
	}
	G_bprint(PRINT_HIGH, "%s %s\n", self->netname,
			redtext(va("voted to proceed short-handed (%d/%d)", voted, present)));
	if (present > 0 && voted >= present)
	{
		mm_forfeit_active = false;
  mm_paused_cp_last[0] = 0;  // let the next window print immediately
		mm_proceed_reset();
		G_bprint(PRINT_HIGH, "%s\n",
				redtext("Team chose to proceed - resuming short-handed in 3 seconds."));
		when_to_unpause = pauseduration + 3000;
	}
}

// "abort": within the map-1 no-fault window, the disadvantaged team can void the
// match immediately (no Elo). Outside the window it's not allowed.
void PlayerMatchAbort(void)
{
	char loser[64] = "";

	if (!is_matchmade_server() || !mm_forfeit_active)
	{
		G_sprint(self, PRINT_HIGH, "%s\n", redtext("No technical timeout in progress."));
		return;
	}
	if (!mm_in_abort_window())
	{
		G_sprint(self, PRINT_HIGH, "%s\n",
				redtext("Too late to abort - the map will be forfeited instead."));
		return;
	}
	mm_forfeit_active = false;
 mm_paused_cp_last[0] = 0;  // let the next window print immediately
	mm_forfeit_outcome(loser, sizeof(loser));  // capture the leaver (for cooldown)
	G_bprint(PRINT_HIGH, "%s\n",
			redtext("Match aborted - no result, no Elo change."));
	if (loser[0])
	{
		cvar_set("k_match_forfeit_loser", loser);
	}
	cvar_fset("k_match_aborted", 1);
	trap_setpause(0);
	EndMatch(0);
}

// Notify qwleague that a specific player connected to this matchmade server.
// Writes a tiny JSON file in the gamedir, then POSTs it to
// /ServerApi/PlayerConnect via sv_web_postfile. The backend uses this to know
// which player actually showed up, so only true no-shows get penalized.
void mm_notify_connect(void)
{
	const char *token;
	const char *match_id;
	const char *callback;
	char filename[80];
	char body[256];
	fileHandle_t fh = 0;
	size_t i;

	if (!is_matchmade_server())
	{
		return;
	}
	if (self->isBot)
	{
		return;
	}

	token = mm_player_token(self);
	if (!token[0])
	{
		return;
	}
	match_id = cvar_string("k_match_id");
	if (!match_id[0])
	{
		return;
	}
	callback = cvar_string("sv_www_address");
	if (!callback[0])
	{
		return;
	}

	// Filename is unique per token so simultaneous connects don't clobber.
	// Keep only safe chars (alphanumeric + -_) so MVDSV's safety check passes.
	snprintf(filename, sizeof(filename), "connect_");
	{
		size_t base = strlen(filename);
		size_t tlen = strlen(token);
		size_t copy = tlen < (sizeof(filename) - base - 6) ? tlen : (sizeof(filename) - base - 6);
		for (i = 0; i < copy; i++)
		{
			char c = token[i];
			filename[base + i] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
					|| (c >= '0' && c <= '9') || c == '-' || c == '_'
					? c : '_';
		}
		filename[base + copy] = 0;
		strlcat(filename, ".json", sizeof(filename));
	}

	snprintf(body, sizeof(body),
			"{\"token\":\"%s\",\"match_id\":\"%s\"}", token, match_id);

	if (trap_FS_OpenFile(filename, &fh, FS_WRITE_BIN) < 0)
	{
		return;
	}
	trap_FS_WriteFile(body, strlen(body), fh);
	trap_FS_CloseFile(fh);

	localcmd("\nsv_web_postfile ServerApi/PlayerConnect \"\" \"%s\" *internal authinfo\n",
			filename);
	trap_executecmd();
}

// Tell qwleague a matchmade session was aborted before it ever produced a
// result (e.g. everyone left during warmup/countdown). POSTs a minimal
// {match_id, aborted} to /ServerApi/UploadGameStats so the backend marks the
// session aborted and frees the players immediately, instead of waiting for its
// multi-hour orphan safety net. Mirrors mm_notify_connect's file+post pattern.
void mm_notify_aborted(void)
{
	const char *match_id = cvar_string("k_match_id");
	const char *callback = cvar_string("sv_www_address");
	const char *secret = cvar_string("k_qwleague_secret");
	const char *loser = cvar_string("k_match_forfeit_loser");
	char body[256];
	fileHandle_t fh = 0;

	if (!is_matchmade_server() || !match_id[0] || !callback[0])
	{
		return;
	}

	// Name the abandoner when known (k_match_forfeit_loser set by the caller):
	// the backend then applies the escalating abandon cooldown to that player
	// and re-queues only the innocent ones. Still a no-Elo void either way.
	if (loser[0])
	{
		snprintf(body, sizeof(body),
				"{\"match_id\":\"%s\",\"aborted\":1,\"forfeit_loser\":\"%s\","
				"\"secret\":\"%s\"}",
				match_id, loser, secret);
	}
	else
	{
		snprintf(body, sizeof(body),
				"{\"match_id\":\"%s\",\"aborted\":1,\"secret\":\"%s\"}",
				match_id, secret);
	}

	if (trap_FS_OpenFile("mm_abort.json", &fh, FS_WRITE_BIN) < 0)
	{
		return;
	}
	trap_FS_WriteFile(body, strlen(body), fh);
	trap_FS_CloseFile(fh);

	localcmd("\nsv_web_postfile ServerApi/UploadGameStats \"\" \"%s\" *internal authinfo\n",
			"mm_abort.json");
	trap_executecmd();
}

// Posted when a player abandons a series mid-flow (during a LATER map's warmup)
// and can't return, so the present team is awarded the remaining maps. Unlike
// mm_notify_aborted this is NOT a no-Elo void: the backend credits the present
// team the series win and applies the abandon cooldown to the leaver
// (k_match_forfeit_loser). Mirrors mm_notify_aborted's file+post pattern.
void mm_notify_forfeit(void)
{
	const char *match_id = cvar_string("k_match_id");
	const char *callback = cvar_string("sv_www_address");
	const char *secret = cvar_string("k_qwleague_secret");
	const char *loser = cvar_string("k_match_forfeit_loser");
	char body[256];
	fileHandle_t fh = 0;

	if (!is_matchmade_server() || !match_id[0] || !callback[0])
	{
		return;
	}

	snprintf(body, sizeof(body),
			"{\"match_id\":\"%s\",\"forfeit_loser\":\"%s\",\"series_forfeit\":1,"
			"\"aborted\":0,\"secret\":\"%s\"}",
			match_id, loser, secret);

	if (trap_FS_OpenFile("mm_forfeit.json", &fh, FS_WRITE_BIN) < 0)
	{
		return;
	}
	trap_FS_WriteFile(body, strlen(body), fh);
	trap_FS_CloseFile(fh);

	localcmd("\nsv_web_postfile ServerApi/UploadGameStats \"\" \"%s\" *internal authinfo\n",
			"mm_forfeit.json");
	trap_executecmd();
}

// Reset player frags and start the timer.
void HideSpawnPoints(void);

void StartMatch(void)
{
	char date[64];
	extern int mm_extends_used;

	// reset bloodfest vars.
	bloodfest_reset();

	mm_extends_used = 0; // fresh /extend budget for this match

	// qwleague: each map starts with a clean disconnect/forfeit slate so a
	// previous map's forfeit/abort state never carries into this one. (The
	// per-map stats POST for the prior map already happened in EndMatch.)
	if (is_matchmade_server())
	{
		cvar_set("k_match_forfeit_loser", "");
		cvar_fset("k_match_forfeit_team", 0);
		cvar_fset("k_match_aborted", 0);
		mm_forfeit_cycles_reset();
		mm_first_leaver[0] = 0;
		mm_forfeit_active = false;   // never carry a stale pause into a new map
  mm_paused_cp_last[0] = 0;  // let the next window print immediately
		mm_forfeit_start_ms = 0;
		mm_drop_correlated = false;
		mm_proceed_reset();
		// Who is actually playing this map — the roster every presence check is
		// judged against from here on (see mm_missing_tokens).
		mm_lineup_snapshot();
	}

	k_nochange = 0;
	k_showscores = 0;
	k_standby = 0;
	k_checkx = 0;

	k_userid = 1;
	localcmd("localinfo 1 \"\"\n");
	trap_executecmd(); // <- this really needed

	first_rl_taken = false; // no one took rl yet

	SM_PrepareMap(); // remove/add some items from map regardind with dmm and game mode

	if (SpawnShowStatus() != SPAWN_SHOW_MATCH)
	{
		HideSpawnPoints();
	}

	if (SpawnicideStatus() == SPAWNICIDE_MATCH)
	{
		SpawnicideEnable();
	}
	else
	{
		SpawnicideDisable();
	}

	match_start_time = g_globalvars.time;
	g_matchstarttime = (int)(g_globalvars.time * 1000);
	match_in_progress = 2;

	// Disable berzerk at start
	k_berzerk = 0;
	// Get berzerk mode time
	// qwleague: berzerk hands every living player QUAD + PENT for the last
	// k_btime seconds of the map, which would wreck both the result and the
	// stats we report. It can never arm on a matchmade server, whatever k_bzk
	// says — the `berzerk` command is already CF_RULES-blocked, this covers any
	// other way k_bzk could end up set (stale server config, agent cfg drift).
	if (cvar("k_bzk") && !is_matchmade_server())
	{
		k_berzerktime = cvar("k_btime");
	}
	else
	{
		k_berzerktime = 0;
	}

	lastTeamLocationTime = -TEAM_LOCATION_UPDATE_TIME; // update on next frame

	remove_specs_wizards(); // remove wizards

	SM_PrepareClients(); // put clients in server and reset some params

	if (!QVMstrftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S %Z", 0))
	{
		date[0] = 0;
	}

	if (deathmatch && (!isHoonyModeAny() || (HM_current_point() == 0)))
	{
		if (date[0])
		{
			G_bprint(2, "matchdate: %s\n", date);
		}

		if (!k_matchLess || cvar("k_matchless_countdown"))
		{
			G_bprint(2, "%s\n", redtext("The match has begun!"));
		}
	}

// spec silence
	{
		int fpd = iKey(world, "fpd");
		int k_spectalk = (coop ? 1 : bound(0, cvar("k_spectalk"), 1));
		cvar_fset("sv_spectalk", k_spectalk);

		fpd = (k_spectalk) ? (fpd & ~64) : (fpd | 64);

		localcmd("serverinfo fpd %d\n", fpd);
	}

	self->k_teamnum = g_globalvars.time + 3; //dirty i know, but why waste space?
											 // FIXME: waste space, but be clean

	if (isHoonyModeAny() && HM_timelimit() > 0)
	{
		int minutes = bound(0, HM_timelimit() / 60, 9999);
		int seconds = HM_timelimit() % 60;

		if (seconds)
		{
			++minutes;
		}
		else
		{
			seconds = 60;
		}

		self->cnt = minutes;
		self->cnt2 = seconds;
		localcmd("serverinfo status \"%d min left\"\n", minutes);
		match_end_time = match_start_time + (minutes - 1) * 60 + seconds;
	}
	else
	{
		self->cnt = bound(0, timelimit, 9999);
		self->cnt2 = 60;
		localcmd("serverinfo status \"%d min left\"\n", (int)timelimit);
		match_end_time = match_start_time + self->cnt * 60;
	}

	self->think = (func_t) TimerThink;
	self->s.v.nextthink = g_globalvars.time + 1;

	SM_PrepareShowscores();

	SM_PrepareHostname();

	SM_PrepareTeamsStats();

	SM_PrepareCA();

	SM_on_MatchStart();

	ClearDemoMarkers();

	StartLogs();

#ifdef BOT_SUPPORT
	BotsMatchStart();
#endif

	if (!self->cnt)
	{
		ent_remove(self); // timelimit == 0, so match will end no due to timelimit but due to fraglimit or something
	}

	if (isRACE())
	{
		race_match_start();
	}

	stuffcmd_flags(world, STUFFCMD_DEMOONLY, "//ktx matchstart\n");
	cvar_fset("qtv_sayenabled", 0);
}

// just check if someone using handicap
static qbool handicap_in_use(void)
{
	gedict_t *p;
	int from;

	for (from = 0, p = world; (p = find_plrghst(p, &from));)
	{
		if (GetHandicap(p) != 100)
		{
			return true;
		}
	}

	return false;
}

void PersonalisedCountdown(char *baseText)
{
	char text[1024];
	gedict_t *p;

	for (p = world; (p = find_plr(p)); /**/)
	{
		strlcpy(text, baseText, sizeof(text));

		if (HM_current_point_type() == HM_PT_SET)
		{
			strlcat(text, redtext("* Set Point *"), sizeof(text));
			strlcat(text, "\n\n", sizeof(text));
		}

		if (p->k_hoony_new_spawn && !strnull(p->k_hoony_new_spawn->targetname))
		{
			strlcat(text, va("Next %8.8s\n", redtext(p->k_hoony_new_spawn->targetname)),
					sizeof(text));

			if (timelimit)
			{
				strlcat(text, va("%s %3s\n", "Timelimit", dig3(timelimit)), sizeof(text));
			}
			else if (isHoonyModeDuel() && world->hoony_timelimit)
			{
				int minutes = world->hoony_timelimit / 60;
				int seconds = world->hoony_timelimit % 60;

				if (minutes == 0)
				{
					strlcat(text, va("%s %3ss\n", "Duration", dig3(seconds)), sizeof(text));
				}
				else if (seconds == 0)
				{
					strlcat(text, va("%s %3sm\n", "Duration", dig3(minutes)), sizeof(text));
				}
				else
				{
					strlcat(text, va("%s %1s:%2s\n", "Duration", dig3(minutes), dig3(seconds)),
							sizeof(text));
				}
			}

			if (!strnull(world->hoony_defaultwinner))
			{
				if (streq(p->k_hoony_new_spawn->targetname, world->hoony_defaultwinner))
				{
					strlcat(text, va("Draw %s\n", redtext(" you win")), sizeof(text));
				}
				else
				{
					strlcat(text, va("Draw %s\n", redtext("you lose")), sizeof(text));
				}
			}
		}

		G_centerprint(p, "%s", text);
	}
}

void PrintCountdown(int seconds)
{
// Countdown: seconds
//
//
// EQL semifinal
//
// Deathmatch	x
// Mode			D u e l | T e a m | F F A | C O O P | BLOODFST | C T F | RA | CA
// Spawnmodel	KTX | bla bla bla // optional
// Antilag		On|Off
// NoItems		On // optional
// Midair		On // optional
// Instagib		On // optional
// Yawnmode		On // optional
// Airstep		On // optional
// TmOverlay	On // optional
// Teamplay		x
// Timelimit	xx
// Fraglimit	xxx
// Overtime		xx		Overtime printout, supports sudden death display
// Powerups		On|Off|QPRS
// Dmgfrags		On // optional
// Noweapon

// Handicap in use // optional

	char text[1024] =
		{ 0 };
	char *mode = "";
	char *ot = "";
	char *nowp = "";
	char *matchtag = redtext(ezinfokey(world, "matchtag"));
	int k_socd = cvar("k_socd");

	strlcat(text, va("%s: %2s\n\n\n", redtext("Countdown"), dig3(seconds)), sizeof(text));

//	if (matchtag[0]) {
//		strlcat(text, va("matchtag %s\n\n\n", matchtag), sizeof(text));
//	}
//	else {
//		strlcat(text, "no matchtag\n\n\n", sizeof(text));
//	}

	if (isHoonyModeDuel() && seconds <= 3)
	{
		PersonalisedCountdown(text);

		return;
	}

	// useless in RA
	if (!isRA() && !coop && !isRACE())
	{
		strlcat(text, va("%s %2s\n", "Deathmatch", dig3(deathmatch)), sizeof(text));
	}

	if (k_bloodfest)
	{
		mode = redtext("BLOODFST");
	}
	else if (coop)
	{
		mode = redtext("C O O P");
	}
	else if (isRA())
	{
		mode = redtext("RA");
	}
	else if (isCA())
	{
		if (cvar("k_clan_arena") == 2)
		{
			mode = redtext("Wipeout");	
		}
		else 
		{
			mode = redtext("CA");
		}
	}
	else if (isHoonyModeDuel())
	{
		mode = redtext("Hoony");
	}
	else if (lgc_enabled())
	{
		mode = redtext("LGC");
	}
	else if (isHoonyModeTDM())
	{
		mode = redtext("BlitzTDM");
	}
	else if (isRACE())
	{
		mode = redtext("R A C E");
	}
	else if (isDuel())
	{
		mode = redtext("D u e l");
	}
	else if (isTeam())
	{
		mode = redtext("T e a m");
	}
	else if (isFFA())
	{
		mode = redtext("F F A");
	}
	else if (isCTF())
	{
		mode = redtext("C T F");
	}
	else
	{
		mode = redtext("Unknown");
	}

	strlcat(text, va("%s %8s\n", "Mode", mode), sizeof(text));

	if (isCA())
	{
		strlcat(text, va("%s %3s\n", "RoundWins", dig3(CA_wins_required())), sizeof(text));
	}
	else if (isHoonyModeTDM() && HM_current_point())
	{
		strlcat(text, "\n", sizeof(text));
		strlcat(text, (char*) HM_round_explanation(), sizeof(text));
		strlcat(text, (char*) HM_series_explanation(), sizeof(text));
		strlcat(text, "\n", sizeof(text));
	}
	else if (isRACE())
	{
		if (race.round_number >= race.rounds)
		{
			strlcat(text, va("Round %7s\n", redtext("final")), sizeof(text));
		}

		strlcat(text, va("%s %9s\n", "Pts", race_scoring_system_name()), sizeof(text));
	}

	if (!(isHoonyModeTDM() && HM_current_point()))
	{
		//	if ( cvar( "k_spw" ) != 3 )
		if (!isRACE())
		{
			strlcat(text, va("%s %4s\n", "Respawns", respawn_model_name_short(cvar("k_spw"))),
					sizeof(text));
		}

		if (cvar("sv_antilag"))
		{
			strlcat(text, va("%s %5s\n", "Antilag", dig3((int)cvar("sv_antilag"))), sizeof(text));
		}

		if (cvar("k_drp"))
		{
			strlcat(text, va("%s %5s\n", "DropMsg", dig3((int)cvar("k_drp"))), sizeof(text));
		}

		if (cvar("k_noitems") && !isRACE())
		{
			strlcat(text, va("%s %5s\n", "NoItems", redtext("on")), sizeof(text));
		}

		if (cvar("k_midair"))
		{
			strlcat(text, va("%s %6s\n", "Midair", redtext("on")), sizeof(text));
		}

		if (cvar("k_instagib"))
		{
			strlcat(text, va("%s %4s\n", "Instagib", redtext("on")), sizeof(text));
		}

		if (k_yawnmode)
		{
			strlcat(text, va("%s %4s\n", "Yawnmode", redtext("on")), sizeof(text));
		}

		if (cvar("pm_airstep"))
		{
			strlcat(text, va("%s %5s\n", "Airstep", redtext("on")), sizeof(text));
		}

		vw_enabled = vw_available && cvar("k_allow_vwep") && cvar("k_vwep");
		if (vw_enabled && !isRACE())
		{
			strlcat(text, va("%s %8s\n", "VWep", redtext("on")), sizeof(text));
		}

		if (cvar("k_teamoverlay") && tp_num() && !isDuel() && !isRACE())
		{
			strlcat(text, va("%s %3s\n", "TmOverlay", redtext("on")), sizeof(text));
		}

		if (!isRA() && (isTeam() || isCTF()))
		{
			strlcat(text, va("%s %4s\n", "Teamplay", dig3(teamplay)), sizeof(text));
		}
	}

	if (isHoonyModeAny())
	{
		int hm_timelimit = HM_timelimit();

		if (hm_timelimit)
		{
			int minutes = hm_timelimit / 60;
			int seconds = hm_timelimit % 60;

			if (minutes == 0)
			{
				strlcat(text, va("%s %3ss\n", "Duration", dig3(seconds)), sizeof(text));
			}
			else if (seconds == 0)
			{
				strlcat(text, va("%s %3sm\n", "Duration", dig3(minutes)), sizeof(text));
			}
			else
			{
				strlcat(text, va("%s %1s:%2s\n", "Duration", dig3(minutes), dig3(seconds)),
						sizeof(text));
			}
		}

		if (HM_rounds())
		{
			strlcat(text,
					va(" %s     %2s/%2s\n", "Round", dig3(HM_current_point() + 1),
						dig3(HM_rounds())),
					sizeof(text));
		}
	}
	else if (timelimit)
	{
		strlcat(text, va("%s %3s\n", "Timelimit", dig3(timelimit)), sizeof(text));
	}

	if (!isHoonyModeAny() && fraglimit)
	{
		strlcat(text, va("%s %3s\n", "Fraglimit", dig3(fraglimit)), sizeof(text));
	}

	switch ((int)cvar("k_overtime"))
	{
		case 0:
			ot = redtext("off");
			break;

		case 1:
			ot = dig3(cvar("k_exttime"));
			break;

		case 2:
			ot = redtext("sd");
			break;

		case 3:
			ot = va("%s %s", dig3(tiecount()), redtext("tb"));
			break;

		case SD_GOLDEN_FRAG:
			ot = va("gold");
			break;

		default:
			ot = redtext("Unkn");
			break;
	}

	if (!isHoonyModeAny())
	{
		if (timelimit && cvar("k_overtime"))
		{
			strlcat(text, va("%s %4s\n", "Overtime", ot), sizeof(text));
		}
	}

	if (!isRA() && Get_Powerups() && strneq("off", Get_PowerupsStr()))
	{
		strlcat(text, va("%s %4s\n", "Powerups", redtext(Get_PowerupsStr())), sizeof(text));
	}

	if (SpawnicideStatus() == SPAWNICIDE_MATCH)
	{
		strlcat(text, va("Spawnicide %s\n", redtext("on")), sizeof(text));
	}

	strlcat(text, va("%s %6s\n", SOCD_DETECTION_VERSION,
		k_socd == SOCD_ALLOW ? redtext("allow")
		: k_socd == SOCD_STATS ? redtext("stats")
		: k_socd == SOCD_WARN ? redtext("warn")
		: redtext("kick")), sizeof(text));

	if (cvar("k_dmgfrags"))
	{
		strlcat(text, va("%s %4s\n", "Dmgfrags", redtext("on")), sizeof(text));
	}

	if (cvar("k_freshteams"))
	{
		strlcat(text, va("%s %2s\n", "&c07fFreshTeams&r", redtext("on")), sizeof(text));
	}

	if (cvar("k_nosweep"))
	{
		strlcat(text, va("%s %5s\n", "NoSweep", redtext("on")), sizeof(text));
	}

	if ((deathmatch == 4) && !cvar("k_midair") && !cvar("k_instagib")
			&& !strnull(nowp = str_noweapon((int)cvar("k_disallow_weapons") & DA_WPNS)))
	{
		strlcat(text, va("\n%s %4s\n", "Noweapon", redtext(nowp[0] == 32 ? (nowp + 1) : nowp)),
				sizeof(text));
	}

	if (handicap_in_use())
	{
		strlcat(text, "\n"
				"Handicap in use\n",
				sizeof(text));
	}

	if (isHoonyModeAny())
	{
		if (((HM_current_point() % 2) == 0))
		{
			strlcat(text, va("\n%-13s\n", redtext("New spawns")), sizeof(text));
		}
		else
		{
			strlcat(text, va("\n%-13s\n", redtext("Switch spawns")), sizeof(text));
		}
	}

	if (tot_mode_enabled())
	{
		int weapon = FrogbotWeapon();
		strlcat(text, va("\nTribe of Tjernobyl mode %2s\n", redtext("on")), sizeof(text));
		strlcat(text, va("Break on death %11s\n",
			FrogbotBreakOnDeath() ? redtext("on") : redtext("off")),
			sizeof(text));
		strlcat(text, va("Bot weapon %15s\n", redtext(weapon ? WpName(weapon) : "random")), sizeof(text));
		strlcat(text, va("Bot health %15s\n", dig3(FrogbotHealth())), sizeof(text));
		strlcat(text, va("Bot skill %16s\n", dig3(FrogbotSkillLevel())), sizeof(text));
		strlcat(text, va("Quad damage multiplier %3s\n", dig3(FrogbotQuadMultiplier())), sizeof(text));
		strlcat(text, va("Item Pickup Bonus %8s\n",
			redtext(FrogbotItemPickupBonus() ? "on": "off")), sizeof(text));

	}

	if (CountBots() >= 1)
	{
		strlcat(text, va("\nBot Skill Mode %s\n",
				redtext(FrogbotEasySkillMode() ? "easy" : "default")), sizeof(text));
	}

	if (matchtag[0])
	{
		strlcat(text, va("\nmatchtag %s\n\n\n", matchtag), sizeof(text));
	}
	else
	{
		strlcat(text, "\nno matchtag\n\n\n", sizeof(text));
	}

	G_cp2all("%s", text);
}

qbool isCanStart(gedict_t *s, qbool forceMembersWarn)
{
	int k_lockmin = (isCA() || isRACE()) ? 2 : cvar("k_lockmin");
	int k_lockmax = (isCA() || isRACE()) ? 2 : cvar("k_lockmax");
	int k_membercount = cvar("k_membercount");
	int i = CountRTeams();
	int sub;
	int nready;
	char *txt = "";
	gedict_t *p;

	// no limits in RA
	if (isRA())
	{
		return true;
	}

	// some limits in duel...
	if (isDuel())
	{
		sub = CountPlayers() - 2;

		if (sub > 0) // we need two players in duel...
		{
			txt = va("Get rid of %d player%s!\n", sub, count_s(sub));

			if (s)
			{
				G_sprint(s, 2, "%s", txt);
			}
			else
			{
				G_bprint(2, "%s", txt);
			}

			return false;
		}
	}

	// no team/members rules limitation in non team game
	if (!isTeam() && !isCTF())
	{
		return true;
	}

	// below Team or CTF game modes and limits...

	for (p = world; (p = find_plr(p));)
	{
		if (strnull(getteam(p)))
		{
			txt = "Get rid of players with empty team\n";

			if (s)
			{
				G_sprint(s, 2, "%s", txt);
			}
			else
			{
				G_bprint(2, "%s", txt);
			}

			return false;
		}
	}

	if (i < k_lockmin)
	{
		sub = k_lockmin - i;
		txt = va("%d more team%s required!\n", sub, count_s(sub));

		if (s)
		{
			G_sprint(s, 2, "%s", txt);
		}
		else
		{
			G_bprint(2, "%s", txt);
		}

		return false;
	}

	if (i > k_lockmax)
	{
		sub = i - k_lockmax;
		txt = va("Get rid of %d team%s!\n", sub, count_s(sub));

		if (s)
		{
			G_sprint(s, 2, "%s", txt);
		}
		else
		{
			G_bprint(2, "%s", txt);
		}

		return false;
	}

	nready = 0;
	for (p = world; (p = find_plr(p));)
	{
		if (p->ready)
		{
			nready++;
		}
	}

	if (!CheckMembers(k_membercount))
	{
		if (!forceMembersWarn) // warn anyway if we want
		{
			if (nready != k_attendees && !s)
			{
				return false; // inform not in all cases, less annoying
			}
		}

		txt = va("%s %d %s\n"
					"%s\n",
					redtext("Server wants at least"), k_membercount,
					redtext("players in each team"), redtext("Waiting..."));

		if (s)
		{
			G_sprint(s, 2, "%s", txt);
		}
		else
		{
			G_bprint(2, "%s", txt);
		}

		return false;
	}

	if (isCTF() && !k_matchLess) // In matchless CTF, the "this map does not support CTF mode" would get spammed constantly in an unsupported map. So, don't bother returning false.
	{
		// can't really play ctf if map doesn't have flags
		gedict_t *rflag = find(world, FOFCLSN, "item_flag_team1");
		gedict_t *bflag = find(world, FOFCLSN, "item_flag_team2");

		if (!rflag || !bflag)
		{
			txt = "This map does not support CTF mode\n";

			if (s)
			{
				G_sprint(s, 2, "%s", txt);
			}
			else
			{
				G_bprint(2, "%s", txt);
			}

			return false;
		}
	}

	return true;
}

void standby_think(void)
{
	gedict_t *p;

	if (match_in_progress == 1 && !isRA())
	{

		k_standby = 1;

		for (p = world; (p = find_plr(p));)
		{
			if (!strnull(p->netname))
			{
				//set to ghost, 0.2 second before matchstart
				if (isHoonyModeDuel() && p->k_hoony_new_spawn)
				{
					// move viewpoint to selected spawn
					VectorCopy(p->k_hoony_new_spawn->s.v.origin, p->s.v.origin);
					p->s.v.origin[2] += 1;
					VectorCopy(p->k_hoony_new_spawn->s.v.angles, p->s.v.angles);
					p->s.v.fixangle = true;

					setnowep(p);
				}

				p->s.v.takedamage = 0;
				p->s.v.solid = 0;
				p->s.v.movetype = 0;
				p->s.v.modelindex = 0;
				p->model = "";
			}
		}
	}

	ent_remove(self);
}

// Called every second during the countdown.
void TimerStartThink(void)
{
	gedict_t *p;

	k_attendees = CountPlayers();

	if (!isCanStart( NULL, true))
	{
		G_bprint(2, "Aborting...\n");

		StopTimer(1);

		return;
	}

	self->cnt2 -= 1;

	if (self->cnt2 == 1)
	{
		p = spawn();
		p->s.v.owner = EDICT_TO_PROG(world);
		p->classname = "standby_th";
		p->s.v.nextthink = g_globalvars.time + 0.8;
		p->think = (func_t) standby_think;

		if (isHoonyModeAny())
		{
			HM_reset_map();
		}
	}
	else if (self->cnt2 <= 0)
	{
		G_cp2all("%s", "");

		StartMatch();

		return;
	}

	PrintCountdown(self->cnt2);

	if (self->cnt2 < 6)
	{
		char *gr = redtext("Get ready");

		for (p = world; (p = find_client(p));)
		{
			if ((p->ct == ctPlayer) && !p->ready)
			{
				G_sprint(p, 2, "%s!\n", gr);
			}

			stuffcmd(p, "play buttons/switch04.wav\n");
		}
	}

	self->s.v.nextthink = g_globalvars.time + 1;
}

void ShowMatchSettings(void)
{
	int i;
	char *txt = "";

//	G_bprint(2, "Spawnmodel: %s\n", redtext( respawn_model_name( cvar( "k_spw" ) ) ));

// changed to print only if other than default

	if ((i = get_fair_pack()))
	{
		// Output the Fairpack setting here
		switch (i)
		{
			case 0:
				txt = "off";
				break;

			case 1:
				txt = "best weapon";
				break;

			case 2:
				txt = "last weapon fired";
				break;

			default:
				txt = "!Unknown!";
				break;
		}

		G_bprint(2, "Fairpacks setting: %s\n", redtext(txt));
	}

// print qizmo ( FPD ) settings
	if (!isHoonyModeAny() || HM_current_point() == 0)
	{
		i = iKey(world, "fpd");
		if (i & 170)
		{
			char buf[256] =
				{ 0 };

			if (i & 2)
			{
				strlcat(buf, " timer", sizeof(buf));
			}

			if (i & 8)
			{
				strlcat(buf, " lag", sizeof(buf));
			}

			if (i & 32)
			{
				strlcat(buf, " enemy", sizeof(buf));
			}

			if (i & 128)
			{
				strlcat(buf, " point", sizeof(buf));
			}

			G_bprint(2, "QiZmo:%s disabled\n", redtext(buf));
		}
	}
}

// duel_dag_vs_zu-zu[dm3]
// team_no!_vs_fom[dm3]
// ctf_no!_vs_fom[dm3]
// ffa_10[dm3] // where 10 is count of players
// ra_10[dm3] // where 10 is count of players
// unknown_10[dm3] // where 10 is count of players

char* CompilateDemoName(void)
{
	static char demoname[60];
	char date[128], *fmt;
	char teams[MAX_CLIENTS][MAX_TEAM_NAME];

	int i;
	int players;
	gedict_t *p;
	char *name, *vs;

	demoname[0] = 0;
	if (isRA())
	{
		strlcat(demoname, va("ra_%d", (int)CountPlayers()), sizeof(demoname));
	}
	else if (isCA())
	{
		if (cvar("k_clan_arena") == 1)
		{
			strlcat(demoname, "ca", sizeof(demoname));
		}
		else
		{
			strlcat(demoname, "wipeout", sizeof(demoname));
		}

		getteams(teams);
		
		for (vs = "_", i = 0; i < MAX_CLIENTS; i++)
		{
			if (strnull(teams[i]))
			{
				break;
			}

			strlcat(demoname, vs, sizeof(demoname));
			strlcat(demoname, teams[i], sizeof(demoname));
			vs = "_vs_";
		}
	}
	else if (isRACE() && !race_match_mode())
	{
		players = 0;

		strlcat(demoname, "race", sizeof(demoname));
		for (vs = "_", p = world; (p = find_plr(p));)
		{
			if (strnull(name = getname(p)) || !(p->racer))
			{
				continue;
			}

			if (players < 2)
			{
				strlcat(demoname, vs, sizeof(demoname));
				strlcat(demoname, name, sizeof(demoname));
			}
			else if (players == 2)
			{
				strlcat(demoname, vs, sizeof(demoname));
				strlcat(demoname, "others", sizeof(demoname));
			}

			++players;
		}
	}
	else if (isDuel())
	{
		strlcat(demoname, "duel", sizeof(demoname));
		if (isRACE())
		{
			strlcat(demoname, "_race", sizeof(demoname));
		}

		if (cvar("k_midair"))
		{
			strlcat(demoname, "_midair", sizeof(demoname));
		}

		if (cvar("k_instagib"))
		{
			strlcat(demoname, "_instagib", sizeof(demoname));
		}

		for (vs = "_", p = world; (p = find_plr(p));)
		{
			if (strnull(name = getname(p)))
			{
				continue;
			}
			if (isRACE() && !(p->race_participant))
			{
				continue;
			}

			strlcat(demoname, vs, sizeof(demoname));
			strlcat(demoname, name, sizeof(demoname));
			vs = "_vs_";
		}
	}
	else if (isTeam() || isCTF())
	{
		char teams[MAX_CLIENTS][MAX_TEAM_NAME];
		int cnt = getteams(teams);
		int clt = cvar("maxclients"); //CountPlayers();

		// guess is this XonX
		if ((clt > 1) && (cnt > 1) && !(clt % cnt))
		{
			clt /= cnt; // yes
		}
		else
		{
			clt = 0; // no
		}

		if (isTeam())
		{
			const char *strCurrentUmode;

			// The parameter is (current_umode-1), because the UserModes_t enum has `umUnknown` as first element,
			// but the um_list[] array doesn't have an 'empty' first row
			strCurrentUmode = um_name_byidx(current_umode-1);
			if (strCurrentUmode != NULL)
			{
				strlcat(demoname, (char *)strCurrentUmode, sizeof(demoname));
			}
			else
			{
				// Something is wrong with current_umode. Let's use the legacy format
				strlcat(demoname, (clt ? va("%don%d", clt, clt) : "team"), sizeof(demoname));
			}
		}
		else
		{
			strlcat(demoname, "ctf", sizeof(demoname));
		}

		if (isRACE())
		{
			strlcat(demoname, "_race", sizeof(demoname));
		}

		for (vs = "_", i = 0; i < MAX_CLIENTS; i++)
		{
			if (strnull(teams[i]))
			{
				break;
			}

			strlcat(demoname, vs, sizeof(demoname));
			strlcat(demoname, teams[i], sizeof(demoname));
			vs = "_vs_";
		}
	}
	else if (isFFA())
	{
		if (isRACE())
		{
			strlcat(demoname, "race_", sizeof(demoname));
		}

		strlcat(demoname, va("ffa_%d", (int)CountPlayers()), sizeof(demoname));
	}
	else
	{
		if (isRACE())
		{
			strlcat(demoname, "race_", sizeof(demoname));
		}

		strlcat(demoname, va("unknown_%d", (int)CountPlayers()), sizeof(demoname));
	}

	if (isRACE())
	{
		strlcat(demoname, va("[%s_r%02d]", mapname, race.active_route),
				sizeof(demoname));
	}
	else
	{
		strlcat(demoname, va("[%s]", mapname), sizeof(demoname));
	}

	fmt = cvar_string("k_demoname_date");

	if (!strnull(fmt) && QVMstrftime(date, sizeof(date), fmt, 0))
	{
		strlcat(demoname, date, sizeof(demoname));
	}

	return demoname;
}

void StartDemoRecord(void)
{
	char *demoname;

	// extralog should be set by easyrecord and if we skip recording we will have it set to WRONG value.
	// So this set it at least to something reasonable ffs.
	cvar_set("extralogname", "");

	if (cvar("demo_tmp_record"))
	{ // FIXME: TODO: make this more like ktpro
		qbool record = false;

		if (isRACE())
		{
			record = true;
		}
		else if (!deathmatch)
		{
			record = false;
		}
		else if (isFFA() && cvar("demo_skip_ktffa_record"))
		{
			record = false;
		}
		else if (isHoonyModeAny() && HM_current_point() > 0)
		{
			record = false; // don't try to record (segfault) when already recording
		}
		else
		{
			record = true;
		}

		if (record)
		{
			if (!strnull(cvar_string("serverdemo")))
			{
				localcmd("sv_democancel\n"); // demo is recording, cancel before new one
			}

			demoname = CompilateDemoName();
			localcmd("sv_demoeasyrecord \"%s\"\n", demoname);
		}
	}
}

// Spawns the timer and starts the countdown.
void StartTimer(void)
{
	gedict_t *timer;

	if (match_in_progress || intermission_running || match_over)
	{
		return;
	}

	if (k_matchLess && !CountPlayers())
	{
		return; // can't start countdown in matchless mode due to no players,
	}

	k_force = 0;

	for (timer = world; (timer = find(timer, FOFCLSN, "idlebot"));)
	{
		ent_remove(timer);
	}

	for (timer = world; (timer = find(timer, FOFCLSN, "timer"));)
	{
		ent_remove(timer);
	}

	for (timer = world; (timer = find(timer, FOFCLSN, "standby_th"));)
	{
		ent_remove(timer);
	}

	if (!k_matchLess)
	{
		ShowMatchSettings();
	}

	if (!k_matchLess || k_bloodfest)
	{
		for (timer = world; (timer = find_client(timer));)
		{
			stuffcmd(timer, "play items/protect2.wav\n");
		}
	}

	timer = spawn();
	timer->s.v.owner = EDICT_TO_PROG(world);
	timer->classname = "timer";
	timer->cnt = 0;

	timer->cnt2 = max(3, (int)cvar("k_count")); // at the least we want a 3 second countdown

	if (isHoonyModeDuel() && (HM_current_point() > 0))
	{
		timer->cnt2 = 3; // first point gets usual 10 seconds, next points gets less
	}

	if (k_bloodfest)
	{
		// at the least 5 second countdown in bloodfest mode.
		timer->cnt2 = max(5, (int)cvar("k_count"));
	}
	else if (!deathmatch)
	{
		// no countdown in coop or similar modes.
		timer->cnt2 = 0;
	}
	else if (k_matchLess)
	{
		if (!cvar("k_matchless_countdown"))
		{
			timer->cnt2 = 0; // no countdown if variable is not specified.
		}
	}

	(timer->cnt2)++;

	timer->s.v.nextthink = g_globalvars.time + 0.001;
	timer->think = (func_t) TimerStartThink;

	match_in_progress = 1;

	localcmd("serverinfo status Countdown\n");

	StartDemoRecord(); // if allowed
}

static qbool match_can_cancel_demo(void)
{
	char matchtag[64] =
		{ 0 };
	int k_demo_mintime = bound(0, cvar("k_demo_mintime"), 3600);

	if (!match_start_time)
	{
		return true; // match not started
	}

#ifdef BOT_SUPPORT
	// if any bots involved, /break is a standard cancel, regardless of matchtag
	if (CountBots() == 0)
	{
#endif
	// don't cancel demo for match with matchtag
	infokey(world, "matchtag", matchtag, sizeof(matchtag));
	if (!strnull(matchtag))
	{
		return false;
	}
#ifdef BOT_SUPPORT
	}
#endif

	// only cancel if /break before set time elapsed
	if (k_demo_mintime <= 0)
	{
		k_demo_mintime = 120; // 120 seconds is default
	}

	return (g_globalvars.time - match_start_time) < k_demo_mintime;
}

// Whenever a countdown or match stops, remove the timer and reset everything.
// also stop/cancel demo recording
void StopTimer(int removeDemo)
{
	gedict_t *timer, *p;

	if (match_in_progress == 1)
		G_cp2all("%s", ""); // clear center print

	k_force = 0;
	match_in_progress = 0;

	if (k_standby)
	{
		// Stops the bug where players are set to ghosts 0.2 second to go and countdown aborts.
		// standby flag needs clearing (sturm)
		k_standby = 0;

		for (p = world; (p = find_plr(p));)
		{
			setfullwep(p);

			p->s.v.takedamage = DAMAGE_AIM;
			p->s.v.solid = SOLID_SLIDEBOX;
			p->s.v.movetype = MOVETYPE_WALK;
			setmodel(p, "progs/player.mdl");
		}
	}

	for (timer = world; (timer = find(timer, FOFCLSN, "timer"));)
	{
		ent_remove(timer);
	}

	for (timer = world; (timer = find(timer, FOFCLSN, "standby_th"));)
	{
		ent_remove(timer);
	}

	if (removeDemo && (match_can_cancel_demo()) && (race_can_cancel_demo())
			&& !strnull(cvar_string("serverdemo")))
	{
		localcmd("sv_democancel\n"); // demo is recording and must be removed, do it
	}

	match_start_time = 0;

	// do not set to Standby during points, (unless its the final point of course)
	if (!isHoonyModeAny() || (HM_current_point_type() == HM_PT_FINAL))
	{
		localcmd("serverinfo status Standby\n");
	}
}

void IdlebotForceStart(void)
{
	gedict_t *p;
	int i;

	G_bprint(2, "server is tired of waiting\n"
				"match WILL commence!\n");

	i = 0;
	for (p = world; (p = find_plr(p));)
	{
		if (p->ready)
		{
			i++;
		}
		else
		{
			G_bprint(2, "%s was kicked by IDLE BOT\n", p->netname);
			G_sprint(p, 2, "Bye bye! Pay attention next time.\n");

			stuffcmd(p, "disconnect\n"); // FIXME: stupid way
		}
	}

	k_attendees = i;

	if (k_attendees > 1)
	{
		StartTimer();
	}
	else
	{
		G_bprint(2, "Can't start! More players needed.\n");
		EndMatch(1);
	}
}

void IdlebotThink(void)
{
	gedict_t *p;
	int i;

	if (cvar("k_idletime") <= 0)
	{
		ent_remove(self);
		return;
	}

	self->attack_finished -= 1;

	i = CountPlayers();

	if (((0.5f * i) > CountRPlayers()) || (i < 2))
	{
		G_bprint(2, "console: bah! chickening out?\n"
					"server disables the %s\n",
					redtext("idle bot"));

		ent_remove(self);

		return;
	}

	k_attendees = CountPlayers();

	if (!isCanStart(NULL, true))
	{
		G_bprint(2, "%s removed\n", redtext("idle bot"));

		ent_remove(self);

		return;
	}

	if (self->attack_finished < 1)
	{

		IdlebotForceStart();

		ent_remove(self);

		return;

	}
	else
	{
		i = self->attack_finished;

		if ((i < 5) || !(i % 5))
		{
			for (p = world; (p = find_plr(p));)
			{
				if (!p->ready)
				{
					G_sprint(p, 2, "console: %d second%s to go ready\n", i, (i == 1 ? "" : "s"));
				}
			}
		}
	}

	self->s.v.nextthink = g_globalvars.time + 1;
}

void IdlebotCheck(void)
{
	gedict_t *p;
	int i;
	int bots = CountBots();

	if ((cvar("k_idletime") <= 0) || bots)
	{
		if ((p = find(world, FOFCLSN, "idlebot")))
		{
			ent_remove(p);
		}

		return;
	}

	i = CountPlayers();

	if (((0.5f * i) > CountRPlayers()) || (i < 2))
	{
		p = find(world, FOFCLSN, "idlebot");

		if (p)
		{
			G_bprint(2, "console: bah! chickening out?\n"
						"server disables the %s\n",
						redtext("idle bot"));

			ent_remove(p);
		}

		return;
	}

	if (match_in_progress || intermission_running || k_force)
	{
		return;
	}

	// no idle bot in practice mode
	if (k_practice) // #practice mode#
	{
		return;
	}

	if ((p = find(world, FOFCLSN, "idlebot"))) // already have idlebot
	{
		return;
	}

	//50% or more of the players are ready! go-go-go

	k_attendees = CountPlayers();

	if (!isCanStart( NULL, true))
	{
		G_sprint(self, 2, "Can't issue %s!\n", redtext("idle bot"));

		return;
	}

	p = spawn();
	p->classname = "idlebot";
	p->think = (func_t) IdlebotThink;
	p->s.v.nextthink = g_globalvars.time + 0.001;

	p->attack_finished = max(3, cvar("k_idletime"));

	G_bprint(2, "\n"
				"server activates the %s\n",
				redtext("idle bot"));
}

void CheckAutoXonX(qbool use_time);
void r_changestatus(float t);

// Called by a player to inform that (s)he is ready for a match.
void PlayerReady(qbool startIdlebot)
{
	gedict_t *p;
	float nready;
	char *matchtag = ezinfokey(world, "matchtag");

	// On a matchmade (qwleague) server, players ready up during the pre-map
	// warmup; the prewar ticker starts the countdown the moment everyone is
	// ready (else it auto-starts at the warmup deadline). Outside warmup, ready
	// is a no-op (the match is already starting / running).
	if (is_matchmade_server())
	{
		if (!match_in_progress && !match_over && find(world, FOFCLSN, "mm_prewar"))
		{
			if (!self->ready)
			{
				self->ready = 1;
				G_bprint(2, "%s %s\n", self->netname, redtext("is ready"));
			}
		}
		else
		{
			G_sprint(self, 2, "%s\n", redtext("You can ready up during warmup."));
		}
		return;
	}

	if (isRACE() && !race_match_mode())
	{
		r_changestatus(1); // race_ready

		return;
	}

	if (self->ct == ctSpec && !isRACE())
	{

		if (!cvar("k_auto_xonx") || k_matchLess)
		{
			G_sprint(self, 2, "Command not allowed\n");

			return;
		}

		if (self->ready)
		{
			G_sprint(self, 2, "Type break to unready yourself\n");

			return;
		}

		self->ready = 1;

		for (p = world; (p = (match_in_progress ? find_spc(p) : find_client(p)));)
		{
			G_sprint(p, 2, "%s %s to play\n", self->netname, redtext("desire"));
		}

		CheckAutoXonX(g_globalvars.time < 10 ? true : false); // force switch mode asap if possible after some time spent

		return;
	}

	if (intermission_running || (match_in_progress == 2) || match_over)
	{
		return;
	}

	if (k_practice && !isRACE())
	{ // #practice mode#
		G_sprint(self, 2, "%s\n", redtext("Server in practice mode"));

		return;
	}

	if (self->ready)
	{
		G_sprint(self, 2, "Type break to unready yourself\n");

		return;
	}

	if (is_private_game() && !is_logged_in(self))
	{
		G_sprint(self, 2, "You must login first\n");

		return;
	}

	if (isCTF() || isHoonyModeTDM())
	{
		if (!streq(getteam(self), "red") && !streq(getteam(self), "blue"))
		{
			G_sprint(self, 2, "You must be on team red or blue\n");

			return;
		}
	}

	if (k_force && (isTeam() || isCTF()))
	{
		nready = 0;
		for (p = world; (p = find_plr(p));)
		{
			if (p->ready)
			{
				if (streq(getteam(self), getteam(p)) && !strnull(getteam(self)))
				{
					nready = 1;
					break;
				}
			}
		}

		if (!nready)
		{
			G_sprint(self, 2, "Join an existing team!\n");

			return;
		}
	}

	// do not allow empty team in team mode, because it cause problems
	if ((isTeam() || isCTF() || isCA()) && strnull(getteam(self)))
	{
		G_sprint(self, 2, "Set your %s before ready!\n", redtext("team"));

		return;
	}

	if (GetHandicap(self) != 100)
	{
		G_sprint(self, 2, "\207%s you are using handicap!\n", redtext("WARNING:"));
	}

	self->ready = 1;
	self->v.brk = 0;
	self->k_teamnum = 0;

	// force red or blue color if ctf
	if (isCTF())
	{
		if (streq(getteam(self), "blue"))
		{
			stuffcmd_flags(self, STUFFCMD_IGNOREINDEMO, "color 13\n");
		}
		else if (streq(getteam(self), "red"))
		{
			stuffcmd_flags(self, STUFFCMD_IGNOREINDEMO, "color 4\n");
		}
	}

	if (!isHoonyModeAny() || (HM_current_point() == 0))
	{
		G_bprint(2, "%s %s%s%s\n", self->netname, redtext("is ready"),
					((isTeam() || isCTF()) ? va(" \220%s\221", getteam(self)) : ""),
					(matchtag[0] ? va(" - %s", matchtag) : " - no matchtag set"));
	}

	nready = CountRPlayers();
	k_attendees = CountPlayers();

	if (match_in_progress)
	{
		return; // possible in bloodfest.
	}

	if (!isCanStart( NULL, false))
	{
		return; // rules does't allow us to start match, idlebot ignored because of same reason
	}

	if (k_force)
	{
		return; // admin forces match - timer will started somewhere else
	}

	// we ignore "all players ready" and "at least two players ready" checks in bloodfest mode.
	if (!k_bloodfest)
	{
		if (nready != k_attendees)
		{
			// not all players ready, check idlebot and return
			if (startIdlebot)
			{
				IdlebotCheck();
			}

			return;
		}

		// ok all players ready.
		// only one or less players ready, match is pointless.
		if (nready < 2)
		{
			return;
		}
	}

	if (isHoonyModeAny() && k_attendees && nready == k_attendees)
	{
		HM_all_ready();
	}
	else
	{
		if (k_attendees && (nready == k_attendees))
		{
			G_bprint(2, "All players ready\n");
		}

		if (cvar("k_on_start_f_modified"))
		{
			stuffcmd(self, "say f_modified\n");
		}

		if (cvar("k_on_start_f_ruleset"))
		{
			stuffcmd(self, "say f_ruleset\n");
		}

		if (cvar("k_on_start_f_version"))
		{
			stuffcmd(self, "say f_version\n");
		}

		G_bprint(2, "Timer started\n");
	}

	StartTimer();
}

void PlayerSlowReady(void)
{
	PlayerReady(false);
}

void PlayerFastReady(void)
{
	PlayerReady(true);
}

void PlayerBreak(void)
{
	int votes;
	gedict_t *p;

	// qwleague matchmade servers: during the pre-map warmup, "break" just
	// un-readies you (take a break). Once the match is starting/running, breaks
	// are disabled — leaving triggers the disconnect/forfeit handling instead.
	if (is_matchmade_server())
	{
		if (!match_in_progress && !match_over && find(world, FOFCLSN, "mm_prewar"))
		{
			if (self->ready)
			{
				self->ready = 0;
				G_bprint(2, "%s %s\n", self->netname, redtext("is not ready"));
			}
		}
		else
		{
			G_sprint(self, PRINT_HIGH, "%s\n",
					redtext("/break is disabled on matchmade servers."));
		}
		return;
	}

	if (isRACE() && !race_match_mode())
	{
		r_changestatus(2); // race_break

		return;
	}

	if ((self->ct == ctSpec) && !isRACE())
	{
		if (!cvar("k_auto_xonx") || k_matchLess)
		{
			G_sprint(self, 2, "Command not allowed\n");

			return;
		}

		if (!self->ready)
		{
			return;
		}

		self->ready = 0;

		for (p = world; (p = (match_in_progress ? find_spc(p) : find_client(p)));)
		{
			G_sprint(p, 2, "%s %s to play\n", self->netname, redtext("lost desire"));
		}

		return;
	}

	if (!self->ready || intermission_running || match_over)
	{
		return;
	}

	if (isCA() && (match_in_progress == 2) && !self->ca_ready)
	{
		G_sprint(self, 2, "You must be in the game to vote\n");

		return;
	}

	if (k_matchLess && !k_bloodfest)
	{
		// do not allow break/next_map commands in some cases.
		if (cvar("k_no_vote_map"))
		{
			G_sprint(self, 2, "Voting next map is %s allowed\n", redtext("not"));

			return;
		}
	}

	if (!match_in_progress)
	{
		self->ready = 0;

		G_bprint(2, "%s %s\n", self->netname, redtext("is not ready"));

		return;
	}

	if (!k_matchLess || k_bloodfest)
	{
		// try stop countdown. (countdown between hoony-mode points can't be stopped, treat as standard break request).
		qbool can_stop_hoonymode = (!isHoonyModeAny() || HM_current_point() == 0);

		if (match_in_progress == 1 && can_stop_hoonymode)
		{
			p = find(world, FOFCLSN, "timer");

			if (p && p->cnt2 > 1)
			{
				self->ready = 0;

				if (!k_matchLess || (k_bloodfest && (CountRPlayers() < 1)))
				{
					G_bprint(2, "%s %s\n", self->netname, redtext("stops the countdown"));
					StopTimer(1);
				}
				else
				{
					G_bprint(2, "%s %s\n", self->netname, redtext("is not ready"));
				}
			}

			return;
		}
	}

	if (self->v.brk)
	{
		self->v.brk = 0;

		G_bprint(2, "%s %s %s vote%s\n", self->netname, redtext("withdraws"), redtext(g_his(self)),
					((votes = get_votes_req(OV_BREAK, true)) ? va(" (%d)", votes) : ""));

		return;
	}

	self->v.brk = 1;

	G_bprint(2, "%s %s%s\n", self->netname,
				redtext(k_matchLess ? "votes for next map" : "votes for stopping the match"),
				((votes = get_votes_req( OV_BREAK, true)) ? va(" (%d)", votes) : ""));

	// show warning to player - that he can't stop countdown alone in matchless mode.
	if (k_matchLess && (match_in_progress == 1) && (CountPlayers() == 1))
	{
		G_sprint(self, 2, "You can't stop countdown alone\n");
	}

	vote_check_break();
}
