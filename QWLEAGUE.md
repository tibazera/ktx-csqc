# QWLeague — KTX matchmaking mods

This is the **`qwleague`** branch: a fork of
[QW-Group/ktx](https://github.com/QW-Group/ktx) carrying the server-side mods
that power [QWLeague](https://qwleague.com)'s matchmade games. It tracks upstream
KTX and layers the league logic on top.

## What it adds

- **Match-token connect gate** — only rostered players' tokens may join a
  matchmade server. IP-to-token binding was removed so players connecting through
  qwfwd / UDP proxies aren't locked out.
- **Bo3/BoN series** — one server plays an ordered map list via `k_series_*`.
- **Recoverable-disconnect match flow** — ready-gate warmup, per-map forfeit,
  `reconnect` / `proceed` / `abort`, and a no-fault early-abort window (`k_mm_*`).
- **Spectator auto-promote** + **QWLeague stats POST** (`/ServerApi/*`).
- **Series epilogue** (`k_qwl_epilogue`, seconds; 0 off) — at a matchmade
  series' end, the players on QWLeague's own client (userinfo `qwl`) play on a
  few seconds at a cut top speed before the intermission, cued by serverinfo
  `qwl_epilogue`; everyone else gets the usual intermission at once, the
  players hidden from them and no one firing meanwhile. The result is final
  first (stats posted, the demo stopped at the end); its frags don't count.
- **Orb mode** (`k_orb`; `src/orb.c`) — a server setting, as are
  `k_orb_cooldown` and `k_orb_life` (config or rcon; no player command
  changes them). `cmd orb` (no alias stuffed) throws each player's orb
  (cooldown `k_orb_cooldown`, 30 s). It flies until it meets the world, a
  brush or a teleporter (players don't stop it) and sticks; `cmd orb` again
  teleports its owner to it, in flight or stuck. A stuck orb lasts
  `k_orb_life` (2.5 s); stuck in a teleporter, the teleport goes through it.
  A teleport never fails: it lands at the nearest spot a player fits, at
  worst back along the orb's flight.
  A stuck orb that runs out bursts: 100 damage at its heart, falling off as a
  rocket's (half to its thrower), sent as `TE_TAREXPLOSION`; kills read
  "zapped by X's orb" (`dtORB`, appended after `dtUNKNOWN` in `deathtype.h`).
  Its sprite and stock sounds (all in id1's `pak0`, which every client has)
  are precached when `k_orb` is on as the map loads, or on any QWLeague match
  server (`k_match_id` set), where `k_orb` can be turned on mid-map over rcon
  and works at once. The QWLeague client's own sounds (`qwl/orb_*.wav`) are
  precached only when `k_orb` is on as the map loads; turned on mid-map, the
  orb sounds stock (axe swing, `ambience/hum1.wav`) until the next map. Any
  other server precaches nothing for the orb (turned on mid-map there, it
  starts with the next map). Landing on a player telefrags them.
- Assorted `motd` / `vote` / `world` tweaks for matchmade servers.

Most behaviour is gated behind `k_qwleague_*` / `k_mm_*` cvars (off by default),
so it stays inert on non-matchmade servers.

## Use these mods in your own KTX build

```bash
git remote add qwleague https://github.com/niQQe/ktx.git
git fetch qwleague
git merge qwleague/qwleague        # bring the mods onto your branch
# …or replay just the mod commits onto any base:
#   git cherry-pick <upstream-base>..qwleague/qwleague
```

Then build `qwprogs.so` as usual (see upstream KTX build docs).

---
Based on upstream KTX; this branch is periodically rebased onto the latest
`master`. Issues/PRs for the league mods go here, not upstream.
