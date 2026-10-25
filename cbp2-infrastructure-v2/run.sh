#!/usr/bin/env bash
# Bash version of the csh "run" script (for systems without csh/dc).
# usage: ./run.sh traces
if [ -z "$1" ]; then echo "Usage: $0 <trace-file-directory>"; exit 1; fi
[ -x src/predict ] || { echo "predict program is not built (cd src && make)"; exit 1; }
sum=0; n=0
for i in $(find "$1" -name '*.trace.*' | sort); do
  mpki=$(./src/predict "$i" | tail -1 | sed -e 's/ MPKI//')
  printf "%-40s\t%0.3f\n" "$i" "$mpki"
  sum=$(echo "$sum + $mpki" | awk '{print $1 + $3}'); n=$((n + 1))
done
awk -v s="$sum" -v n="$n" 'BEGIN { printf "average MPKI: %.3f\n", s / n }'
