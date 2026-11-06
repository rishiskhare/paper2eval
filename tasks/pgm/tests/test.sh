#!/bin/bash
# Scores 1 only if the agent changed just the header, the segment counts match the authors', and the authors' tests pass.
V=/opt/pgm-verifier
SUB=/home/dev/PGM-index
HEADER=include/pgm/piecewise_linear_model.hpp
OUT=/logs/verifier
WORK=$(mktemp -d)
RUN="setpriv --reuid=grader --regid=grader --clear-groups"

mkdir -p "$OUT" && chmod 755 "$OUT" 2>/dev/null
echo 0 > "$OUT/reward.txt"
fail() { echo 0 > "$OUT/reward.txt"; echo "FAIL: $*"; exit 0; }

[ -f "$SUB/$HEADER" ] && [ ! -L "$SUB/$HEADER" ] || fail "header missing or a symlink"
# Edited or deleted files fail; new files such as scratch tests are ignored.
changed=$(diff -rq "$V/pristine" "$SUB" | grep -v "^Only in $SUB" | grep -v "$HEADER")
[ -z "$changed" ] || fail "files outside the header changed: $changed"

cp -r "$V/pristine/." "$WORK/" && cp "$SUB/$HEADER" "$WORK/$HEADER" && cd "$WORK" || fail "could not set up"
CXX="g++ -std=c++17 -O2 -w -Iinclude"
$CXX "$V/count.cpp" -o count && $CXX test/main.cpp test/tests.cpp -o tests || fail "does not compile"
chown -R grader:grader "$WORK"

timeout 600 $RUN ./count > counts.txt
code=$?
cat counts.txt
[ "$code" = 0 ] || fail "wrong lookup, crash or timeout on held-out keys (exit $code)"
awk 'NR==FNR { ref[$1,$2] = $3; n++; next } ($1,$2) in ref && $3 <= ref[$1,$2] { ok++ } END { exit ok != n }' \
    "$V/reference_counts.txt" counts.txt || fail "more segments than the authors' optimal counts"

timeout 1200 $RUN ./tests || fail "the authors' tests failed"

echo 1 > "$OUT/reward.txt"
echo PASS
