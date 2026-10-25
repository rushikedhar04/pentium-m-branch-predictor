// compare.cc
// Experiment driver (not needed for grading): runs one predictor configuration
// on one trace.
//
// usage: compare <trace> taken
//        compare <trace> bimodal  <bim_bits>
//        compare <trace> gshare
//        compare <trace> pm <bim_bits> <set_bits> <tag_bits> <ways> <alloc_on_bim_miss 0|1> [reset_ctr 0|1]
//        compare <trace> cpm <use_pir 0|1> <use_loop 0|1> [loop_conf 1..3] [loop_priority 0|1]   (extra credit)
// output: <direction MPKI> <global hit %> <target MPKI>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/branch.h"
#include "../src/trace.h"
#include "../src/predictor.h"
#include "../src/my_predictor.h"

// always-taken (the original stub behaviour)
class taken_predictor : public branch_predictor {
public:
	branch_update u;
	branch_update *predict (branch_info &) { u.direction_prediction (true); return &u; }
};

// bimodal only (the Pentium M bimodal table on its own)
class bimodal_predictor : public branch_predictor {
public:
	pm_update u; branch_info bi; std::vector<unsigned char> t; int bits;
	bimodal_predictor (int b) : bits(b) { t.assign (1u << b, 2); }
	branch_update *predict (branch_info &b) {
		bi = b; u.way = -1;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = b.address & ((1u << bits) - 1);
			u.direction_prediction (t[u.index] >= 2);
		} else u.direction_prediction (true);
		return &u;
	}
	void update (branch_update *bu, bool taken, unsigned int) {
		if (bi.br_flags & BR_CONDITIONAL) pm_predictor::train (t[((pm_update *) bu)->index], taken);
	}
};

int main (int argc, char *argv[]) {
	if (argc < 3) { fprintf (stderr, "see source for usage\n"); return 1; }
	branch_predictor *p;
	pm_predictor *pm = 0;
	cpm_predictor *cpm = 0;
	if (!strcmp (argv[2], "taken")) p = new taken_predictor ();
	else if (!strcmp (argv[2], "gshare")) p = new gshare_predictor ();
	else if (!strcmp (argv[2], "bimodal")) {
		p = new bimodal_predictor (atoi (argv[3]));
	} else if (!strcmp (argv[2], "cpm")) {
		cpm = new cpm_predictor (atoi (argv[3]) != 0, atoi (argv[4]) != 0, argc > 5 ? atoi (argv[5]) : 3, argc > 6 ? atoi (argv[6]) != 0 : true);
		p = cpm;
	} else {
		pm = new pm_predictor (atoi (argv[3]), atoi (argv[4]), atoi (argv[5]),
		                       atoi (argv[6]), atoi (argv[7]) != 0,
		                       argc > 8 ? atoi (argv[8]) != 0 : true);
		p = pm;
	}
	init_trace (argv[1]);
	long long dmiss = 0, tmiss = 0, cond = 0, hits = 0;
	for (;;) {
		trace *t = read_trace ();
		if (!t) break;
		branch_update *u = p->predict (t->bi);
		if (t->bi.br_flags & BR_CONDITIONAL) {
			cond++;
			if (pm && pm->u.way >= 0) hits++;
			if (cpm && cpm->u.gway >= 0) hits++;
			dmiss += u->direction_prediction () != t->taken;
			tmiss += u->target_prediction () != t->target;
		}
		p->update (u, t->taken, t->target);
	}
	end_trace ();
	printf ("%0.3f %0.2f %0.3f\n", 1000.0 * (dmiss / 1e8), cond ? 100.0 * hits / cond : 0.0, 1000.0 * (tmiss / 1e8));
	return 0;
}
