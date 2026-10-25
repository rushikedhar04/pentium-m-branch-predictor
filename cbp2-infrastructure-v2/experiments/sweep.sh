#!/usr/bin/env bash
# Runs every configuration on every trace and writes results/results.csv
# usage (from cbp2-infrastructure-v2/):  experiments/sweep.sh
set -e
cd "$(dirname "$0")/.."
g++ -O3 -w -o experiments/compare experiments/compare.cc src/trace.cc
mkdir -p results
CONFIGS=(
  "always_taken|taken"
  "gshare_32K_h15|gshare"
  "bimodal_4K|bimodal 12"
  "pm_4way|pm 12 9 6 4 1 1"
  "pm_4way_handoutCtr|pm 12 9 6 4 1 0"
  "pm_2way|pm 12 9 6 2 1 1"
  "pm_4way_allocAlways|pm 12 9 6 4 0 1"
  "pm_2way_allocAlways|pm 12 9 6 2 0 1"
  "pm_4way_256sets|pm 12 8 6 4 1 1"
  "pm_4way_1024sets|pm 12 10 6 4 1 1"
  "pm_4way_2048sets|pm 12 11 6 4 1 1"
  "cpm_full|cpm 1 1 3 1"
  "cpm_ghr_loop|cpm 0 1 3 1"
  "cpm_pir_noloop|cpm 1 0 3 1"
  "cpm_ghr_noloop|cpm 0 0 3 1"
  "cpm_loop_noPriority|cpm 1 1 3 0"
  "cpm_loopConf1|cpm 1 1 1 1"
)
echo "config,trace,mpki,global_hit_pct,target_mpki" > results/results.csv
for c in "${CONFIGS[@]}"; do
  name="${c%%|*}"; args="${c#*|}"
  for t in $(find traces -name '*.trace.*' | sort); do
    echo "$name,$(basename $(dirname $t)),$(experiments/compare "$t" $args | tr ' ' ',')" &
  done | sort -t, -k2 >> results/results.csv
  wait
done
echo "wrote results/results.csv"
