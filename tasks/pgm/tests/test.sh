#!/bin/bash
# Grades the collected /home/dev/PGM-index. Writes /logs/verifier/{results.json,reward.json,reward.txt}.
V=/opt/pgm-verifier
OUT=/logs/verifier
mkdir -p "$OUT" && chmod 755 "$OUT" 2>/dev/null
python3 "$V/eval.py" --submission /home/dev/PGM-index --pristine "$V/pristine" --harness "$V/harness.cpp" \
    --spec "$V/heldout_spec.json" --gen "$V/gen_workloads.py" --reference "$V/reference_values.json" \
    --workdir /tmp/pgm-eval --out-dir "$OUT"
if [ ! -s "$OUT/reward.json" ]; then
    echo '{"reward": 0, "pass": 0}' > "$OUT/reward.json"
    echo 0 > "$OUT/reward.txt"
fi
