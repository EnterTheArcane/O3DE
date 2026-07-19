import sys
from pathlib import Path
TOOL_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOL_ROOT))
from azsl2slang.discover import discover, DEFAULT_SEARCH_ROOTS
from azsl2slang import registry as reg

repo_root = TOOL_ROOT.parents[5]
files = discover([Path(r) for r in DEFAULT_SEARCH_ROOTS], repo_root)
r = reg.build([f.source for f in files])

print(f"SRGs:      {len(r.srgs)}")
print(f"semantics: {len(r.semantics)}")
print(f"options:   {len(r.options)}")
print(f"enums:     {len(r.enums)}")
print(f"collisions:{len(r.collisions)}")

print("\n-- semantics --")
for name, s in sorted(r.semantics.items()):
    print(f"  {name:28} freq={s.frequency_id} fallback={s.variant_fallback_bits} -> {s.slot_name}")

print("\n-- SRGs (first 20) --")
for name, s in sorted(r.srgs.items())[:20]:
    print(f"  {name:34} semantic={s.semantic!s:24} partial={s.is_partial} slot={r.slot_expression(s.semantic)}")

unresolved = [n for n, s in r.srgs.items() if r.slot_expression(s.semantic) is None]
print(f"\nSRGs with unresolved slot: {len(unresolved)}")
for n in unresolved[:15]:
    print(f"  {n}  (semantic={r.srgs[n].semantic})")

print("\n-- options (first 15) --")
for name, o in sorted(r.options.items())[:15]:
    print(f"  {name:44} type={o.type_text!s:22} default={o.default_text!s:18} enum={o.inline_enum_name}")

bad = [n for n, o in r.options.items() if not o.type_text]
print(f"\noptions with empty type: {len(bad)} {bad[:10]}")
