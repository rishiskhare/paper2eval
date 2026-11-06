#!/usr/bin/env bash
# Verifies the git history contract for each task (see tasks/<task>/review/history.md).
# Usage: scripts/check_history.sh [task ...]   (default: every task with a <task>-upstream tag)
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"

KINDS='upstream|build|leak|cut|ref|harness|eval|rung|harbor|review|trial'
fail=0
ok()   { echo "  PASS $*"; }
bad()  { echo "  FAIL $*"; fail=1; }

allowed_files() {
  case "$1" in
    pgm)  echo "tasks/pgm/repo/include/pgm/piecewise_linear_model.hpp" ;;
    fsst) echo "tasks/fsst/repo/fsst_build.inc" ;;
  esac
}

# Prints C++ function $2 (signature line through the first closing brace in column 0) from file content on stdin.
# Column 0 rather than brace counting: buildSymbolTable has an #ifdef with two alternative opening braces.
extract_fn() {
  python3 -c '
import re, sys
src, name = sys.stdin.read(), sys.argv[1]
m = re.search(r"^[^\n;]*\b" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", src, re.M)
if not m: sys.exit("function %s not found" % name)
print(src[m.start():re.compile(r"^\}", re.M).search(src, m.end()).end()])
' "$1"
}

check_task() {
  local t=$1 up=$1-upstream st=$1-stripped ref=$1-reference repo=tasks/$1/repo/
  echo "== $t"
  for tag in $up $st $ref; do git rev-parse -q --verify "refs/tags/$tag" >/dev/null || { bad "missing tag $tag"; return; }; done
  local allowed; allowed=$(allowed_files "$t")

  echo "-- [1] reference == upstream for the allowed files"
  if [ "$t" = fsst ]; then
    # The cut moved buildSymbolTable from libfsst.cpp into fsst_build.inc, so compare the function itself.
    local a b
    a=$(git show "$up:tasks/fsst/repo/libfsst.cpp" | extract_fn buildSymbolTable)
    b=$(git show "$ref:tasks/fsst/repo/fsst_build.inc" | extract_fn buildSymbolTable)
    if [ -n "$a" ] && [ "$a" = "$b" ]; then ok "buildSymbolTable body identical (upstream libfsst.cpp vs reference fsst_build.inc)"
    else bad "buildSymbolTable differs:"; diff <(echo "$a") <(echo "$b") | head -40; fi
  else
    local d; d=$(git diff "$up" "$ref" -- $allowed)
    if [ -z "$d" ]; then ok "git diff $up $ref -- $allowed is empty"; else bad "reference differs from upstream:"; echo "$d" | head -40; fi
  fi

  echo "-- [2] upstream..reference on repo/ is only [build]/[leak] changes"
  git diff "$up" "$ref" --stat -- "$repo" | sed 's/^/     /'
  local cut; cut=$(git log --format=%H --grep='^\[cut\]' "$up..$st" -- "$repo")
  [ "$(echo "$cut" | grep -c .)" = 1 ] && [ "$(git rev-parse "$st")" = "$cut" ] || bad "$st must be the single [cut] commit"
  local kinds; kinds=$(git log --format=%s "$up..$st^" -- "$repo" | grep -Ev '^\[(build|leak)\]')
  [ -z "$kinds" ] && ok "commits between $up and the cut are [build]/[leak] only" || bad "unexpected commits before the cut: $kinds"
  local residue
  if [ "$t" = fsst ]; then
    residue=$(git diff "$st^" "$ref" --stat -- "$repo" ":!$allowed" ":!tasks/fsst/repo/libfsst.cpp" ":!tasks/fsst/repo/paper/dbtext")
    # The cut also deletes the held-out corpora; they must be deletions only and match evaluator/heldout/ byte for byte.
    local hd; hd=$(git diff --name-only --diff-filter=D "$st^" "$st" -- tasks/fsst/repo/paper/dbtext)
    [ -z "$(git diff --name-only --diff-filter=d "$st^" "$st" -- tasks/fsst/repo/paper/dbtext)" ] && [ -n "$hd" ] \
      && ok "cut deletes held-out corpora only: $(echo $hd | xargs -n1 basename | xargs)" || bad "cut changes dbtext beyond deleting held-out corpora"
    for f in $hd; do
      cmp -s <(git show "$st^:$f") "tasks/fsst/evaluator/heldout/$(basename "$f")" || bad "held-out copy of $(basename "$f") missing or differs"
    done
  else residue=$(git diff "$st^" "$ref" --stat -- "$repo"); fi
  [ -z "$residue" ] && ok "reference tree == pre-cut tree (only [build]/[leak] differ from upstream)" || { bad "reference differs from the pre-cut tree:"; echo "$residue"; }
  if [ "$t" = fsst ]; then
    git diff "$st^" "$ref" -- tasks/fsst/repo/libfsst.cpp | grep -q . && echo "     note: libfsst.cpp differs from pre-cut only by the moved function (checked by [1])"
  fi
  local leaks; leaks=$(git log --format=%H --grep='^\[leak\]' "$up..$st" -- "$repo")
  for c in $leaks; do
    [ -z "$(git diff --diff-filter=d --name-only "$c^" "$c")" ] && ok "[leak] $(git rev-parse --short "$c") is deletions only" || bad "[leak] $(git rev-parse --short "$c") has non-deletions"
  done
  local rc; rc=$(git rev-list --count "$st..$ref")
  [ "$rc" = 1 ] && [ "$(git log -1 --format=%s "$ref" | cut -c1-5)" = "[ref]" ] && ok "$ref is one [ref] commit on top of $st" || bad "$ref must be exactly one [ref] commit after $st"
  # Each task lives on main or on a side branch named after it.
  local home=main; git cat-file -e "main:$repo" 2>/dev/null || home=$t
  [ -z "$(git diff "$st" "$home" -- "$repo")" ] && ok "$home's $repo == $st (reference not on $home)" || bad "$home's $repo differs from $st"

  echo "-- [3] committed patches match regenerated ones"
  local ev=tasks/$t/evaluator
  diff -q <(git diff "$up" "$st" -- "$repo") "$ev/strip.patch" >/dev/null && ok "$ev/strip.patch" || bad "$ev/strip.patch is stale or missing"
  diff -q <(git diff "$st" "$ref" -- "$repo") "$ev/reference.patch" >/dev/null && ok "$ev/reference.patch" || bad "$ev/reference.patch is stale or missing"
  for copy in tasks/$t/solution/reference.patch; do
    [ -e "$copy" ] || continue
    cmp -s "$copy" "$ev/reference.patch" && ok "$copy == $ev/reference.patch" || bad "$copy differs from $ev/reference.patch"
  done
}

check_global() {
  echo "== all commits after pgm-upstream (every branch and tag)"
  local revs; revs=$(git rev-list --all --not pgm-upstream^ )
  echo "-- [4] no commit mixes a task's repo/ with anything else"
  local mixed=0
  for c in $revs; do
    files=$(git diff-tree --no-commit-id --name-only -r --root "$c")
    groups=$(echo "$files" | sed -E 's#^(tasks/[^/]+/repo)/.*#\1#; t; s#.*#ours#' | sort -u)
    if echo "$groups" | grep -q repo && [ "$(echo "$groups" | grep -c .)" -gt 1 ]; then
      bad "$(git log -1 --format='%h %s' "$c") mixes: $(echo $groups)"; mixed=1
    fi
  done
  [ $mixed = 0 ] && ok "$(echo "$revs" | grep -c .) commits checked"
  echo "-- [5] every commit message starts with an allowed kind"
  local untagged; untagged=$(for c in $revs; do git log -1 --format='%h %s' "$c"; done | grep -Ev "^[0-9a-f]+ \[($KINDS)\] [a-z0-9]+: ")
  [ -z "$untagged" ] && ok "all messages match '[kind] task: summary'" || { bad "bad messages:"; echo "$untagged"; }
}

tasks=("$@")
[ ${#tasks[@]} -eq 0 ] && for t in pgm fsst; do git rev-parse -q --verify "refs/tags/$t-upstream" >/dev/null && tasks+=("$t"); done
for t in "${tasks[@]}"; do check_task "$t"; done
check_global
echo
[ $fail = 0 ] && echo "check_history: PASS" || echo "check_history: FAIL"
exit $fail
