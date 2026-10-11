"""Exercise the protected-path expression actually used by the sync workflow."""
import re
from pathlib import Path

workflow = Path('.github/workflows/qwleague-upstream-sync.yml').read_text()
pattern = re.search(r"protected='([^']+)'", workflow).group(1)
protected = [
    'CMakeLists.txt', 'include/g_local.h', 'include/progs.h',
    'qcsrc/pred.qc', 'qcsrc/projectiles.qc', 'qcsrc/nested/new.qc',
    'qcsrc/progs.src', 'src/antilag.c', 'src/client.c', 'src/g_userinfo.c',
    'src/weaponpred_defs.c', 'src/weapons.c', 'src/world.c',
]
allowed = ['src/match.c', 'src/stats_json.c', 'README.md', 'docs/qcsrc/example.md']
for path in protected:
    assert re.search(pattern, path), f'Protected path escaped policy: {path}'
for path in allowed:
    assert not re.search(pattern, path), f'Unrelated path blocked: {path}'
assert 'nm -a build/qwprogs.so | grep -q' not in workflow
assert '| grep -Eiq' not in workflow
print(f'Policy checks passed ({len(protected) + len(allowed)} paths).')
