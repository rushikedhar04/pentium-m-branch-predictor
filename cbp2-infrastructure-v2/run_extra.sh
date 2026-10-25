#!/usr/bin/env bash
# Bash version of the csh "run_extra" script (extra credit: complete Pentium M).
# usage: ./run_extra.sh traces
if [ -z "$1" ]; then echo "Usage: $0 <trace-file-directory>"; exit 1; fi
[ -x src/predict_extra_credit ] || { echo "predict_extra_credit is not built (cd src && make)"; exit 1; }
n=0; sd=0; st=0
for i in $(find "$1" -name '*.trace.*' | sort); do
  line=$(./src/predict_extra_credit "$i" | tail -1)          # "Direction: X MPKI; Target: Y MPKI"
  dm=$(echo "$line" | awk '{print $2}'); tm=$(echo "$line" | awk '{print $5}')
  printf "%-40s\tdirection %0.3f\ttarget %0.3f\n" "$i" "$dm" "$tm"
  sd=$(awk -v a="$sd" -v b="$dm" 'BEGIN{print a+b}'); st=$(awk -v a="$st" -v b="$tm" 'BEGIN{print a+b}'); n=$((n + 1))
done
awk -v d="$sd" -v t="$st" -v n="$n" 'BEGIN { printf "average MPKI: direction %.3f, target %.3f\n", d / n, t / n }'
