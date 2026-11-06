#!/bin/bash
# Greps every file in an agent image for verifier-only content. Usage: leak_scan_image.sh <image> <task>
set -u
IMG=$1; TASK=$2
case $TASK in
  pgm) PAT='lower_start|upper_start|outside_line|intercept_n|rounding_term|convex hull|heldout|held_out|reference_values|ho_uniform64|ho_near_max|7310|7317|\beval\.py|reference\.patch|pgm-reference|pgm-verifier|_rules|check_forbidden|FORBIDDEN|grader|grading|benchmark|paperbench';;
  fsst) PAT='heldout|held_out|reference_values|eval\.py|reference\.patch|fsst-reference|fsst-verifier|buildSymbolTable_reference|_rules|check_forbidden|FORBIDDEN|grader|grading|benchmark|paperbench';;
esac
docker run --rm --network none --entrypoint sh "$IMG" -c "
  echo '-- .git dirs:'; find / -xdev -name .git -not -path '/proc/*' 2>/dev/null
  echo '-- verifier files:'; find / -xdev \( -name eval.py -o -name 'reference*.patch' -o -name heldout_spec.json -o -name reference_values.json -o -name gen_workloads.py -o -name '*_rules.py' \) -not -path '/proc/*' -not -path '/usr/*' 2>/dev/null
  echo '-- pattern hits in /app /opt /root /home /tests /solution:'
  grep -rIoE '$PAT' /app /opt /root /home /tests /solution 2>/dev/null
  echo '-- done'"
echo "-- image history (COPY/ADD layers):"
docker history --no-trunc --format '{{.CreatedBy}}' "$IMG" | grep -E 'COPY|ADD' | sed 's/^/   /'
