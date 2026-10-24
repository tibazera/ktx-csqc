#!/bin/sh
set -eu

qwl_ref="${1:-niqqe/qwleague}"
binary="${QWPROGS:-build/qwprogs.so}"
csprogs="${CSPROGS:-build/csprogs.dat}"

fail() {
    echo "audit failed: $*" >&2
    exit 1
}

git rev-parse --verify "$qwl_ref^{commit}" >/dev/null 2>&1 \
    || fail "unknown QWLeague ref: $qwl_ref"
git merge-base --is-ancestor "$qwl_ref" HEAD \
    || fail "$qwl_ref is not merged into HEAD"

[ -f src/antilag.c ] || fail "src/antilag.c is missing"
[ -f qcsrc/progs.src ] || fail "CSQC sources are missing"
[ -f "$binary" ] || fail "$binary is missing; build qwprogs.so first"
[ -f "$csprogs" ] || fail "$csprogs is missing; build CSQC from this commit"

grep -q 'cvar("sv_antilag") != 1' src/antilag.c \
    || fail "Antilag 1 source gate is missing"

required_symbols='antilag_lagmove_all_hitscan
antilag_lagmove_all_proj
antilag_lagmove_all_proj_bounce
SendEntity_Projectile
UpdateProjectileSendNeeded
mm_player_token
mm_forced_colors
mm_shutdown_think'

symbols="$(nm -a "$binary" 2>/dev/null || true)"
echo "$required_symbols" | while IFS= read -r symbol; do
    echo "$symbols" | grep -q "[[:space:]]$symbol$" \
        || fail "required symbol missing from qwprogs.so: $symbol"
done

echo "audit ok"
echo "source=$(git rev-parse HEAD)"
echo "qwleague=$(git rev-parse "$qwl_ref")"
sha256sum "$binary" "$csprogs"
