# Pentium M Hybrid Branch Predictor

**Assignment 1: Branch Predictor Implementation**
Rushi Kedhar Konduru · PeopleSoft ID 2481344

This repo implements the Intel Pentium M branch outcome predictor (a bimodal table plus a 4-way tagged global predictor) in the CBP-2 branch prediction simulator. For extra credit, it also implements the complete Pentium M unit (BTB, iBTB, loop predictor and PIR).

| Predictor | Avg. direction MPKI (20 traces) |
|---|--:|
| Original stub (always taken) | 86.312 |
| Bimodal only | 10.267 |
| Sample gshare (8 KB) | 6.306 |
| **Pentium M hybrid, `pm_predictor` (3.75 KB)** | **5.849** |
| Complete Pentium M, `cpm_predictor` (extra credit) | 5.748 (target MPKI 6.213) |

📄 **Report:** [REPORT.md](REPORT.md) (PDF: [REPORT.pdf](REPORT.pdf)) covers the design, verification, results and analysis.

## Repository layout

```
README.md                         this file
REPORT.md / REPORT.pdf            report
docs/assignment.pdf               assignment handout
docs/milenkovic_WDDD02.pdf        reference paper provided with the assignment
cbp2-infrastructure-v2/
  src/my_predictor.h              <-- MY CODE: pm_predictor (assignment) + cpm_predictor (extra credit)
  src/predictor.h                 provided; one-line bug fix to target_prediction() (see report 7.1)
  src/predict.cc, trace.cc, ...   provided simulator (unchanged)
  run, run_extra                  provided csh scripts (unchanged)
  run.sh, run_extra.sh            bash versions for machines without csh/dc
  traces/                         20 CBP-2 traces (100M instructions each)
  experiments/test_pm.cc          unit tests (includes the handout's Appendix B example)
  experiments/appendix_b.cc       replays Appendix B step by step and prints the table
  experiments/compare.cc          runs any predictor configuration on one trace
  experiments/sweep.sh            runs all 17 configurations on all traces -> results/results.csv
  experiments/plot.py             makes the charts in results/
  results/                        results.csv, charts, block diagram
```

## Requirements

- g++ (or clang++) and `bzip2` on Linux, macOS or WSL
- optional: `csh` + `dc` for the original `run` scripts, and Python 3 + matplotlib for the charts

## Build and run

```bash
cd cbp2-infrastructure-v2/src
make                    # builds ./predict (pm_predictor) and ./predict_extra_credit (cpm_predictor)
cd ..

./run traces            # original script (needs csh + dc)
./run.sh traces         # same output in bash  -> last line: average MPKI: 5.849

./run_extra.sh traces   # extra credit         -> average MPKI: direction 5.748, target 6.213
# (the provided csh run_extra expects a single number per line, but predict_extra_credit
#  prints direction and target, so run_extra.sh is the reliable way to get both averages)
```

Single trace:

```bash
./src/predict traces/164.gzip/gzip.trace.bz2                 # 12.439 MPKI
./src/predict_extra_credit traces/164.gzip/gzip.trace.bz2    # Direction: 11.881 MPKI; Target: 11.996 MPKI
```

## Tests and experiments

```bash
cd cbp2-infrastructure-v2
g++ -O2 -o experiments/test_pm experiments/test_pm.cc && experiments/test_pm           # unit tests
g++ -O2 -o experiments/appendix_b experiments/appendix_b.cc && experiments/appendix_b  # 22/22 rows match
experiments/sweep.sh          # about 5 minutes on 2 cores; writes results/results.csv
python3 experiments/plot.py   # regenerates the charts
```

## How the predictor works (short version)

- **Bimodal table:** 4096 × 2-bit counters, index = IP[11:0]. Always trained.
- **Global predictor:** 512 sets × 4 ways; each entry is {valid, 6-bit tag, 2-bit counter}; true LRU.
  HASH = IP[14:0] XOR GHR[14:0]; set = HASH[14:6]; tag = HASH[5:0].
- **Prediction:** global hit → global counter; otherwise → bimodal counter.
- **Allocation:** on a global miss, a global entry is allocated only when the bimodal predictor was wrong, so the global table holds only the "hard" branches.
- **GHR:** shifts in each conditional branch's outcome.

Constructor (defaults = Pentium M):

```cpp
pm_predictor (int bim_bits = 12, int set_bits = 9, int tag_bits = 6, int ways = 4,
              bool alloc_on_bim_miss = true, bool reset_ctr_on_alloc = true);
cpm_predictor (bool use_pir = true, bool use_loop = true, int loop_conf = 3, bool loop_priority = true);
```

Use `ways = 2` for the 2-way simplification the assignment allows.

## References

- V. Uzelac, A. Milenkovic, "Experiment Flows and Microbenchmarks for Reverse Engineering of Branch Predictor Structures," ISPASS 2009.
- M. Milenkovic, A. Milenkovic, J. Kulick, "Demystifying Intel Branch Predictors," WDDD 2002.
- D. A. Jiménez, CBP-2 branch prediction infrastructure, 2006.
- Intel, US Patent Application 2005/0262332 (path information register).

No outside code was copied. See section 10 of the report for details.
