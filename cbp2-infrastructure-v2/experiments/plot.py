# plot.py - builds the report charts from results/results.csv
# usage (from cbp2-infrastructure-v2/): python3 experiments/plot.py   (needs matplotlib)
import csv, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

rows = list(csv.DictReader(open("results/results.csv")))
d = {(r["config"], r["trace"]): r for r in rows}
traces = sorted({r["trace"] for r in rows})
avg = lambda c, k="mpki": sum(float(d[c, t][k]) for t in traces) / len(traces)

INK, MUTED, GRID, BASE, SURF = "#0b0b0b", "#898781", "#e1e0d9", "#c3c2b7", "#fcfcfb"
BLUE, ORANGE, AQUA, VIOLET = "#2a78d6", "#eb6834", "#1baf7a", "#6250d6"
plt.rcParams.update({"font.size": 10, "axes.edgecolor": BASE, "axes.labelcolor": INK,
                     "xtick.color": MUTED, "ytick.color": INK, "figure.facecolor": SURF,
                     "axes.facecolor": SURF})

def style(ax):
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    ax.set_axisbelow(True)

def hbar(labels, highlight, fname, title, key="mpki", xlabel="Average MPKI over 20 traces (lower is better)"):
    cfgs = sorted(labels, key=lambda c: avg(c, key), reverse=True)
    fig, ax = plt.subplots(figsize=(7.5, 0.42 * len(cfgs) + 1.2))
    vals = [avg(c, key) for c in cfgs]
    bars = ax.barh([labels[c] for c in cfgs], vals, height=0.6,
                   color=[AQUA if c == highlight else BLUE for c in cfgs])
    for b, v in zip(bars, vals):
        ax.text(v + max(vals) * 0.01, b.get_y() + b.get_height() / 2, f"{v:.3f}", va="center", color=INK, fontsize=9)
    ax.set_xlabel(xlabel); ax.xaxis.grid(True, color=GRID, lw=0.8); style(ax)
    ax.set_xlim(0, max(vals) * 1.12)
    ax.set_title(title, loc="left", color=INK)
    fig.tight_layout(); fig.savefig(fname, dpi=160); plt.close(fig)

# Chart 1: part-1 design space
hbar({
  "bimodal_4K": "Bimodal only (4K)",
  "gshare_32K_h15": "gshare 32K (given sample)",
  "pm_2way_allocAlways": "PM 2-way, allocate always",
  "pm_4way_allocAlways": "PM 4-way, allocate always",
  "pm_4way_256sets": "PM 4-way, 256 sets",
  "pm_2way": "PM 2-way, 512 sets",
  "pm_4way_handoutCtr": "PM 4-way, handout counter rule",
  "pm_4way": "PM 4-way, 512 sets (final)",
  "pm_4way_1024sets": "PM 4-way, 1024 sets",
  "pm_4way_2048sets": "PM 4-way, 2048 sets",
}, "pm_4way", "results/avg_mpki.png", "Hybrid predictor: average direction MPKI")

# Chart 2: per-trace comparison
series = [("bimodal_4K", "Bimodal only", BLUE), ("gshare_32K_h15", "gshare 32K", ORANGE),
          ("pm_4way", "PM hybrid (main)", AQUA), ("cpm_full", "Complete PM (extra credit)", VIOLET)]
fig, ax = plt.subplots(figsize=(10.5, 4.4))
w = 0.21
for i, (c, name, col) in enumerate(series):
    ax.bar([x + (i - 1.5) * w for x in range(len(traces))], [float(d[c, t]["mpki"]) for t in traces],
           width=w - 0.025, color=col, label=name)
ax.set_xticks(range(len(traces)))
ax.set_xticklabels(traces, rotation=45, ha="right", color=INK, fontsize=8.5)
ax.set_ylabel("Direction MPKI"); ax.yaxis.grid(True, color=GRID, lw=0.8); style(ax)
ax.legend(frameon=False, ncol=4, loc="upper right", fontsize=9)
ax.set_title("Direction MPKI per benchmark trace", loc="left", color=INK)
fig.tight_layout(); fig.savefig("results/per_trace_mpki.png", dpi=160); plt.close(fig)

# Chart 3: extra-credit variants
hbar({
  "cpm_full": "Complete PM: PIR + loop (default)",
  "cpm_ghr_loop": "GHR + loop",
  "cpm_pir_noloop": "PIR, no loop predictor",
  "cpm_ghr_noloop": "GHR, no loop (= main predictor)",
  "cpm_loop_noPriority": "PIR + loop, global always wins",
  "cpm_loopConf1": "PIR + loop, confidence 1",
}, "cpm_full", "results/extra_credit_mpki.png", "Complete Pentium M variants: average direction MPKI")
print("saved charts in results/")
