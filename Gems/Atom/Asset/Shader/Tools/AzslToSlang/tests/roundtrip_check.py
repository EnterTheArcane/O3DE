"""Lex every AZSL source and re-emit it from tokens; output must be byte-identical."""
import sys
from pathlib import Path

TOOL_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOL_ROOT))

from azsl2slang.discover import discover, DEFAULT_SEARCH_ROOTS
from azsl2slang.lexing import lex_file

repo_root = TOOL_ROOT.parents[5]
files = discover([Path(r) for r in DEFAULT_SEARCH_ROOTS], repo_root)
print(f"repo root: {repo_root}")
print(f"discovered: {len(files)} AZSL sources")

mismatched, errored, unbalanced, ok = [], [], [], 0
for entry in files:
    try:
        lexed = lex_file(entry.source)
    except Exception as exc:
        errored.append((entry.source, f"{type(exc).__name__}: {exc}"))
        continue
    if lexed.lex_errors:
        errored.append((entry.source, f"{len(lexed.lex_errors)} lex error(s): {lexed.lex_errors[0]}"))
    rebuilt = "".join(t.text for t in lexed.tokens)
    if rebuilt != lexed.text:
        mismatched.append(entry.source)
    else:
        ok += 1
    if not lexed.brace_depth_balanced():
        unbalanced.append((entry.source, lexed.final_brace_depth()))

print(f"\nbyte-identical round trip: {ok}/{len(files)}")
print(f"mismatched:  {len(mismatched)}")
print(f"lex errors:  {len(errored)}")
print(f"unbalanced braces: {len(unbalanced)}")

for path in mismatched[:10]:
    print(f"  MISMATCH {path.relative_to(repo_root)}")
for path, msg in errored[:10]:
    print(f"  LEXERR   {path.relative_to(repo_root)}: {msg}")
for path, depth in unbalanced[:10]:
    print(f"  UNBAL    {path.relative_to(repo_root)}: depth {depth}")
