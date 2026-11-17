/*
 * Orb mode (k_orb): every player has an orb on a cooldown. It is the
 * server's setting, as are k_orb_cooldown and k_orb_life (its config or
 * rcon); no player command changes them.
 *
 * "cmd orb" throws it; no alias is stuffed, so a server without orb mode
 * leaves a player's own "orb" alias alone. It flies straight on until it
 * meets the world, a brush entity or a teleporter, and sticks there. Players
 * don't stop it. While it flies, or while it's stuck, "cmd orb" again
 * teleports its owner to it. Its life counts down only once it's stuck
 * (k_orb_life); in flight it lives as long as it flies. Stuck in a
 * teleporter, the teleport goes through it. A teleport never fails: it lands
 * the player at the nearest place they fit (orb_find_spot), at worst back
 * along the orb's flight.
 *
 * A stuck orb that runs out bursts: ORB_BURST_DAMAGE round it, its
 * thrower's (half to them, as their own rocket), kills "zapped by X's orb".
 * The burst goes out as TE_TAREXPLOSION, a blob explosion every ezQuake
 * draws (no tarbabies in deathmatch); QWLeague's client draws it as an
 * electric burst where it saw an orb.
 *
 * The orb is progs/s_light.spr with EF_RED | EF_GREEN | EF_BLUE: every
 * ezQuake draws that as a white light, and QWLeague's client recognises it
 * by the pair and draws it as a white sphere crackling with electricity.
 *
 * Its files: the sprite and its stock sounds are in id1's pak0, which every
 * client has; they are precached when k_orb is on as a map loads, or on any
 * QWLeague match server (k_match_id set), so there k_orb can be turned on
 * mid-map over rcon and work at once. The QWLeague client's own sounds are
 * precached only when k_orb is on as the map loads; turned on mid-map, the orb
 * sounds stock (ORB_SND_*_STOCK) until the next map. Any other server
 * precaches nothing for the orb, and turned on mid-map there it starts with
 * the next map.
 */

#include "g_local.h"

void teleport_touch(void);

#define ORB_MODEL			"progs/s_light.spr"
#define ORB_CLASSNAME		"orb"
#define ORB_EFFECTS			(EF_RED | EF_GREEN | EF_BLUE)

// The QWLeague client's own (tools/qwl_sounds.py), and the stock ones in pak0 they stand in for.
#define ORB_SND_LAUNCH			"qwl/orb_launch.wav"	// a nod to the Vore's
#define ORB_SND_LAUNCH_STOCK	"weapons/ax1.wav"
#define ORB_SND_HUM				"qwl/orb_hum.wav"		// looped: plays until replaced
#define ORB_SND_HUM_STOCK		"ambience/hum1.wav"		// looped too
#define ORB_SND_STICK			"weapons/tink1.wav"
#define ORB_SND_STOP			"misc/null.wav"

#define ORB_SPEED			900		// units / second
#define ORB_MAX_FLIGHT		15		// seconds; only a hole in the map flies this long
#define ORB_SURFACE_GAP		2		// how far off the surface a stuck orb sits
#define ORB_EYE_HEIGHT		22		// a player's view above their origin
#define ORB_DEFAULT_COOLDOWN	30
#define ORB_DEFAULT_LIFE	2.5
#define ORB_BURST_DAMAGE	100		// at its heart, half a point less a unit out (T_RadiusDamage): gone 140 out

// The player hull a teleport has to fit.
#define ORB_HULL_MINS		-16, -16, -24
#define ORB_HULL_MAXS		16, 16, 32

static float orb_cooldown(void)
{
	float c = cvar("k_orb_cooldown");

	return (c > 0) ? c : ORB_DEFAULT_COOLDOWN;
}

static float orb_life(void)
{
	float l = cvar("k_orb_life");

	return (l > 0) ? l : ORB_DEFAULT_LIFE;
}

static qbool orb_files;			// the sprite and stock sounds are precached on this map
static qbool orb_own_sounds;	// ...and the QWLeague client's own sounds

// The map's teleporters, gathered at the first orb that needs them (they are spawned with the map).
#define ORB_TELEPORTERS_MAX		64
static gedict_t *orb_teleporters[ORB_TELEPORTERS_MAX];
static int orb_teleporter_count;	// -1: not gathered on this map yet; ORB_TELEPORTERS_MAX + 1: too many, scan

// At a map's load: the orb's files when it may be played here (k_orb on, or a QWLeague match server, where rcon
// may turn it on); the QWLeague client's own sounds only with k_orb on. ORB_SND_STICK is precached by every map
// (weapons.c).
void Orb_Precache(void)
{
	orb_teleporter_count = -1;
	orb_own_sounds = (cvar("k_orb") != 0);
	orb_files = orb_own_sounds || !strnull(cvar_string("k_match_id"));
	if (!orb_files)
	{
		return;
	}

	trap_precache_model(ORB_MODEL);
	trap_precache_sound(ORB_SND_LAUNCH_STOCK);
	trap_precache_sound(ORB_SND_HUM_STOCK);
	trap_precache_sound(ORB_SND_STOP);
	if (orb_own_sounds)
	{
		trap_precache_sound(ORB_SND_LAUNCH);
		trap_precache_sound(ORB_SND_HUM);
	}
}

static void orb_remove(gedict_t *orb)
{
	gedict_t *owner = PROG_TO_EDICT(orb->s.v.owner);

	if (owner->orb == orb)
	{
		owner->orb = NULL;
	}

	// The hum loops on its channel until something replaces it.
	sound(orb, CHAN_VOICE, ORB_SND_STOP, 1, ATTN_IDLE);
	ent_remove(orb);
}

// A stuck orb run out: it bursts, ORB_BURST_DAMAGE round it, its thrower's.
static void orb_burst(gedict_t *orb)
{
	WriteByte(MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte(MSG_MULTICAST, TE_TAREXPLOSION);
	WriteCoord(MSG_MULTICAST, orb->s.v.origin[0]);
	WriteCoord(MSG_MULTICAST, orb->s.v.origin[1]);
	WriteCoord(MSG_MULTICAST, orb->s.v.origin[2]);
	trap_multicast(PASSVEC3(orb->s.v.origin), MULTICAST_PHS);

	T_RadiusDamage(orb, PROG_TO_EDICT(orb->s.v.owner), ORB_BURST_DAMAGE, world, dtORB);
	orb_remove(orb);
}

// True while the orb still belongs in the game.
static qbool orb_owner_ok(gedict_t *orb)
{
	gedict_t *owner = PROG_TO_EDICT(orb->s.v.owner);

	return cvar("k_orb") && !intermission_running && (owner->ct == ctPlayer) && (owner->orb == orb)
			&& !ISDEAD(owner);
}

/*
 * Where the segment a -> b first enters the box, as a fraction of it; false
 * if it never does. A start inside the box enters it at 0.
 */
static qbool orb_segment_enters(vec3_t a, vec3_t b, vec3_t mins, vec3_t maxs, float *frac)
{
	float tmin = 0, tmax = 1;
	int i;

	for (i = 0; i < 3; i++)
	{
		float d = b[i] - a[i];
		float t1, t2;

		if (fabs(d) < 0.0001)
		{
			if ((a[i] < mins[i]) || (a[i] > maxs[i]))
			{
				return false;
			}

			continue;
		}

		t1 = (mins[i] - a[i]) / d;
		t2 = (maxs[i] - a[i]) / d;
		if (t1 > t2)
		{
			float t = t1;
			t1 = t2;
			t2 = t;
		}

		tmin = max(tmin, t1);
		tmax = min(tmax, t2);
		if (tmin > tmax)
		{
			return false;
		}
	}

	*frac = tmin;

	return true;
}

static void orb_gather_teleporters(void)
{
	gedict_t *t;

	orb_teleporter_count = 0;
	for (t = world; (t = find(t, FOFS(classname), "trigger_teleport"));)
	{
		if (orb_teleporter_count == ORB_TELEPORTERS_MAX)
		{
			orb_teleporter_count = ORB_TELEPORTERS_MAX + 1;	// too many to keep: scan for them each time

			return;
		}

		orb_teleporters[orb_teleporter_count++] = t;
	}
}

// The next teleporter after t (world: the first), or NULL.
static gedict_t* orb_next_teleporter(gedict_t *t, int *i)
{
	if (orb_teleporter_count > ORB_TELEPORTERS_MAX)
	{
		return find(t, FOFS(classname), "trigger_teleport");
	}

	// A teleporter removed since (its edict maybe reused) is skipped.
	while (*i < orb_teleporter_count)
	{
		t = orb_teleporters[(*i)++];
		if (streq(t->classname, "trigger_teleport"))
		{
			return t;
		}
	}

	return NULL;
}

// The first teleporter the segment a -> b enters before frac, or NULL.
static gedict_t* orb_teleporter_on(vec3_t a, vec3_t b, float *frac)
{
	gedict_t *t, *hit = NULL;
	float f;
	int i = 0;

	if (orb_teleporter_count < 0)
	{
		orb_gather_teleporters();
	}

	for (t = world; (t = orb_next_teleporter(t, &i));)
	{
		if (orb_segment_enters(a, b, t->s.v.absmin, t->s.v.absmax, &f) && (f < *frac))
		{
			*frac = f;
			hit = t;
		}
	}

	return hit;
}

static void orb_stick(gedict_t *orb, vec3_t point, vec3_t normal, gedict_t *anchor)
{
	vec3_t at;

	VectorMA(point, ORB_SURFACE_GAP, normal, at);
	setorigin(orb, PASSVEC3(at));
	VectorCopy(normal, orb->orb_normal);
	VectorClear(orb->s.v.velocity);

	orb->orb_stuck = true;
	orb->orb_anchor = anchor;
	if (anchor)
	{
		VectorSubtract(at, anchor->s.v.origin, orb->orb_anchor_ofs);
	}

	orb->orb_die_time = g_globalvars.time + orb_life();
	sound(orb, CHAN_WEAPON, ORB_SND_STICK, 0.4, ATTN_NORM);
}

// One frame of flight: move as far as the world and teleporters allow.
static void orb_fly(gedict_t *orb)
{
	vec3_t start, end, dir, back;
	gedict_t *tele, *hit;
	float dt = g_globalvars.time - orb->orb_move_time;
	float frac;

	orb->orb_move_time = g_globalvars.time;
	if (dt <= 0)
	{
		return;
	}

	VectorCopy(orb->s.v.origin, start);
	VectorMA(start, dt, orb->s.v.velocity, end);

	// Players don't stop it: the world and brush entities only.
	traceline(PASSVEC3(start), PASSVEC3(end), true, orb);
	frac = g_globalvars.trace_fraction;

	normalize(orb->s.v.velocity, dir);
	VectorScale(dir, -1, back);

	tele = orb_teleporter_on(start, end, &frac);
	if (tele)
	{
		vec3_t point;

		VectorSubtract(end, start, point);
		VectorMA(start, frac, point, point);
		orb_stick(orb, point, back, tele);

		return;
	}

	if (g_globalvars.trace_fraction < 1)
	{
		hit = PROG_TO_EDICT(g_globalvars.trace_ent);
		orb_stick(orb, g_globalvars.trace_endpos, g_globalvars.trace_plane_normal,
					((hit != world) && (hit->s.v.solid == SOLID_BSP)) ? hit : NULL);

		return;
	}

	setorigin(orb, PASSVEC3(end));
}

static void orb_think(void)
{
	gedict_t *orb = self;

	orb->s.v.nextthink = g_globalvars.time + 0.001;	// every server frame

	if (!orb_owner_ok(orb))
	{
		orb_remove(orb);

		return;
	}

	if (!orb->orb_stuck)
	{
		if (g_globalvars.time - orb->orb_launch_time > ORB_MAX_FLIGHT)
		{
			orb_remove(orb);

			return;
		}

		orb_fly(orb);

		return;
	}

	if (g_globalvars.time >= orb->orb_die_time)
	{
		orb_burst(orb);

		return;
	}

	// Ride a door or a plat it stuck to (a teleporter doesn't move).
	if (orb->orb_anchor && (orb->orb_anchor->s.v.solid == SOLID_BSP))
	{
		vec3_t at;

		VectorAdd(orb->orb_anchor->s.v.origin, orb->orb_anchor_ofs, at);
		setorigin(orb, PASSVEC3(at));
	}
}

static void orb_launch(gedict_t *p)
{
	gedict_t *orb = spawn();
	vec3_t eye;

	orb->classname = ORB_CLASSNAME;
	orb->s.v.owner = EDICT_TO_PROG(p);
	orb->s.v.movetype = MOVETYPE_NONE;	// orb_fly moves it, through players
	orb->s.v.solid = SOLID_NOT;
	orb->s.v.effects = ORB_EFFECTS;
	setmodel(orb, ORB_MODEL);
	setsize(orb, 0, 0, 0, 0, 0, 0);

	trap_makevectors(p->s.v.v_angle);
	VectorScale(g_globalvars.v_forward, ORB_SPEED, orb->s.v.velocity);
	VectorCopy(p->s.v.origin, orb->orb_thrown_from);
	VectorCopy(p->s.v.origin, eye);
	eye[2] += ORB_EYE_HEIGHT;
	setorigin(orb, PASSVEC3(eye));

	orb->orb_launch_time = orb->orb_move_time = g_globalvars.time;
	orb->think = (func_t) orb_think;
	orb->s.v.nextthink = g_globalvars.time + 0.001;

	p->orb = orb;
	p->orb_ready_time = g_globalvars.time + orb_cooldown();

	sound(p, CHAN_AUTO, orb_own_sounds ? ORB_SND_LAUNCH : ORB_SND_LAUNCH_STOCK, 1, ATTN_NORM);
	sound(orb, CHAN_VOICE, orb_own_sounds ? ORB_SND_HUM : ORB_SND_HUM_STOCK, 0.5, ATTN_IDLE);
}

// Does a player standing at `at` fit, inside the map? (Other players don't count: landing on one telefrags them.)
static qbool orb_fits(gedict_t *p, vec3_t at)
{
	int contents = trap_pointcontents(PASSVEC3(at));

	if ((contents == CONTENT_SOLID) || (contents == CONTENT_SKY))
	{
		return false;
	}

	TraceCapsule(PASSVEC3(at), PASSVEC3(at), true, p, ORB_HULL_MINS, ORB_HULL_MAXS);

	return !g_globalvars.trace_startsolid && !g_globalvars.trace_allsolid;
}

// Can `from` see `to` (no wall between)?
static qbool orb_sees(gedict_t *p, vec3_t from, vec3_t to)
{
	traceline(PASSVEC3(from), PASSVEC3(to), true, p);

	return (g_globalvars.trace_fraction == 1);
}

// Does a player standing at `at` fit, and can the orb at `from` see there?
static qbool orb_spot_ok(gedict_t *p, vec3_t from, vec3_t at)
{
	return orb_fits(p, at) && orb_sees(p, from, at);
}

static int orb_offset_cmp(const void *a, const void *b)
{
	const float *x = a, *y = b;
	float dx = DotProduct(x, x), dy = DotProduct(y, y);

	return (dx < dy) ? -1 : (dx > dy);
}

// Every offset of the grid round a spot, nearest first: ORB_GRID steps of 16 each way, a little more up than down.
#define ORB_GRID		3
#define ORB_GRID_STEP	16
#define ORB_GRID_DOWN	-3
#define ORB_GRID_UP		4
#define ORB_GRID_COUNT	((2 * ORB_GRID + 1) * (2 * ORB_GRID + 1) * (ORB_GRID_UP - ORB_GRID_DOWN + 1))

static const vec3_t *orb_grid(void)
{
	static vec3_t grid[ORB_GRID_COUNT];
	static qbool made;
	int x, y, z, n = 0;

	if (made)
	{
		return grid;
	}

	for (x = -ORB_GRID; x <= ORB_GRID; x++)
	{
		for (y = -ORB_GRID; y <= ORB_GRID; y++)
		{
			for (z = ORB_GRID_DOWN; z <= ORB_GRID_UP; z++)
			{
				VectorSet(grid[n], x * ORB_GRID_STEP, y * ORB_GRID_STEP, z * ORB_GRID_STEP);
				n++;
			}
		}
	}

	qsort(grid, ORB_GRID_COUNT, sizeof(grid[0]), orb_offset_cmp);
	made = true;

	return grid;
}

// The nearest spot round base (on the grid) that fits and the orb sees.
static qbool orb_spot_near(gedict_t *p, gedict_t *orb, vec3_t base, vec3_t out)
{
	const vec3_t *grid = orb_grid();
	vec3_t at;
	int i;

	for (i = 0; i < ORB_GRID_COUNT; i++)
	{
		VectorAdd(base, grid[i], at);
		if (orb_spot_ok(p, orb->s.v.origin, at))
		{
			VectorCopy(at, out);

			return true;
		}
	}

	return false;
}

/*
 * Where the player lands; always somewhere they fit, inside the map.
 *
 * Near the orb first: stuck, just off its surface with the hull clear of it;
 * in flight, the player's eyes where the orb is; then the grid round that.
 * Failing that (a crevice, a ledge too thin), back along the orb's flight
 * towards where it was thrown, the first place a player fits whose eyes the
 * orb sees (an orb that rode a door away no longer has its flight behind it).
 * Then where it was thrown, if a player still fits there (a lift may have
 * moved into it), else nowhere new: the player stays where they are.
 */
#define ORB_PATH_STEP	16
static const float orb_path_lift[] = { 0, 16, -16, 32 };

static void orb_find_spot(gedict_t *p, gedict_t *orb, vec3_t out)
{
	vec3_t base, back, at, eye;
	float length, d;
	int i;

	if (orb->orb_stuck)
	{
		vec_t *n = orb->orb_normal;
		float reach = 16 * fabs(n[0]) + 16 * fabs(n[1]) + ((n[2] > 0) ? 24 : 32) * fabs(n[2]) + 1;

		VectorMA(orb->s.v.origin, reach, n, base);
	}
	else
	{
		VectorCopy(orb->s.v.origin, base);
		base[2] -= ORB_EYE_HEIGHT;
	}

	if (orb_spot_near(p, orb, base, out))
	{
		return;
	}

	// The flight, as player origins: from under the orb back to where they stood.
	VectorCopy(orb->s.v.origin, base);
	base[2] -= ORB_EYE_HEIGHT;
	VectorSubtract(orb->orb_thrown_from, base, back);
	length = VectorNormalize(back);

	for (d = ORB_PATH_STEP; d < length; d += ORB_PATH_STEP)
	{
		VectorMA(base, d, back, eye);
		eye[2] += ORB_EYE_HEIGHT;
		if (!orb_sees(p, orb->s.v.origin, eye))
		{
			continue;
		}

		for (i = 0; i < (int)(sizeof(orb_path_lift) / sizeof(orb_path_lift[0])); i++)
		{
			VectorMA(base, d, back, at);
			at[2] += orb_path_lift[i];
			if (orb_fits(p, at))
			{
				VectorCopy(at, out);

				return;
			}
		}
	}

	if (orb_fits(p, orb->orb_thrown_from))
	{
		VectorCopy(orb->orb_thrown_from, out);
	}
	else
	{
		VectorCopy(p->s.v.origin, out);
	}
}

// Stuck in a teleporter: the teleporter takes the player. False if it didn't (and nothing of theirs changed).
static qbool orb_through_teleporter(gedict_t *p, gedict_t *tele)
{
	gedict_t *saved_self = self, *saved_other = other;
	float saved_time = p->teleport_time;

	p->teleport_time = 0;
	self = tele;
	other = p;
	teleport_touch();
	self = saved_self;
	other = saved_other;

	if (p->teleport_time == g_globalvars.time)
	{
		return true;
	}

	p->teleport_time = saved_time;

	return false;
}

static void orb_teleport(gedict_t *p, gedict_t *orb)
{
	vec3_t spot;

	if (orb->orb_anchor && streq(orb->orb_anchor->classname, "trigger_teleport")
			&& orb_through_teleporter(p, orb->orb_anchor))
	{
		orb_remove(orb);

		return;
	}

	orb_find_spot(p, orb, spot);
	teleport_player(p, spot, p->s.v.v_angle, TFLAGS_FOG_SRC | TFLAGS_FOG_DST | TFLAGS_SND_SRC | TFLAGS_SND_DST);
	// Keep the player's own aim: fixangle would snap them back to where the
	// server last saw them look, a ping ago.
	p->s.v.fixangle = 0;
	orb_remove(orb);
}

// The "orb" command: throw the orb, or teleport to the one out.
void Orb_Command(void)
{
	gedict_t *p = self;

	if (!cvar("k_orb") || intermission_running || ISDEAD(p) || (p->ct != ctPlayer))
	{
		return;
	}

	if (!orb_files)
	{
		G_sprint(p, PRINT_HIGH, "Orb mode starts with the next map\n");

		return;
	}

	if (p->orb)
	{
		orb_teleport(p, p->orb);

		return;
	}

	if (g_globalvars.time < p->orb_ready_time)
	{
		G_sprint(p, PRINT_HIGH, "Orb ready in %d\n", (int)ceil(p->orb_ready_time - g_globalvars.time));

		return;
	}

	orb_launch(p);
}

// A match starts with no orbs out and every cooldown clear.
void Orb_MatchStart(void)
{
	gedict_t *e;

	for (e = world; (e = find(e, FOFS(classname), ORB_CLASSNAME));)
	{
		orb_remove(e);
	}

	for (e = world; (e = find_plr(e));)
	{
		e->orb = NULL;
		e->orb_ready_time = 0;
	}
}
