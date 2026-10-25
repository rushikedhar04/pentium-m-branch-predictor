// test_pm.cc - unit tests for pm_predictor and cpm_predictor (no trace files needed)
// build/run from cbp2-infrastructure-v2/:
//   g++ -O2 -o experiments/test_pm experiments/test_pm.cc && experiments/test_pm
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/branch.h"
#include "../src/predictor.h"
#include "../src/my_predictor.h"

static branch_info cond (unsigned addr) { branch_info b; b.address = addr; b.opcode = 0; b.br_flags = BR_CONDITIONAL; return b; }

// run a repeating outcome pattern; count direction misses after 'skip' repetitions
static int run (branch_predictor &p, unsigned addr, const bool *pat, int len, int reps, int skip) {
	int miss = 0; branch_info b = cond (addr);
	for (int r = 0; r < reps; r++) for (int i = 0; i < len; i++) {
		branch_update *u = p.predict (b);
		if (r >= skip && u->direction_prediction () != pat[i]) miss++;
		p.update (u, pat[i], pat[i] ? addr - 0x40 : addr + 2);
	}
	return miss;
}

// Appendix B of the handout: 8-bit addresses, 16-entry local table indexed by
// address bits [5:2], 4-bit GHR, global table with 4 rows x 2 ways and 2-bit tags.
// The handout's initial counter values are loaded and the 22 final predictions compared.
static int appendix_b (bool reset_ctr, int *first_bad) {
	const unsigned addr[] = {0x44,0x90,0x44,0x6C,0x90,0x44,0x90,0x44,0x6C,0x90,0x44,0x6C,0x44,0x6C,0x90,0x44,0x90,0x44,0x6C,0x90,0x44,0x90};
	const bool out[]      = {1,1,0,0,1,1,1,0,0,1,0,1,0,0,1,1,1,0,0,1,1,1};
	const bool expect[]   = {0,0,1,1,0,1,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1};   // "Final Prediction" column
	pm_predictor p (4, 2, 2, 2, false, reset_ctr);                         // allocate on every miss
	for (int i = 0; i < 16; i++) p.bim[i] = i % 4;                         // local: 0,1,2,3,0,1,...
	for (int r = 0; r < 4; r++) { p.gtab[r*2].ctr = r; p.gtab[r*2+1].ctr = 3 - r; }   // way0: 0..3, way1: 3..0
	int ok = 0; *first_bad = -1;
	for (int i = 0; i < 22; i++) {
		branch_info b = cond (addr[i] >> 2);     // the example indexes with address bits [5:2]
		branch_update *u = p.predict (b);
		if (u->direction_prediction () == expect[i]) ok++; else if (*first_bad < 0) *first_bad = i + 1;
		p.update (u, out[i], 0);
	}
	return ok;
}

int main () {
	int bad;
	// 1. handout example, using the handout's rule (keep the victim's counter on allocation)
	{ int ok = appendix_b (false, &bad);
	  printf ("Appendix B example (handout rules)      : %d/22 predictions match\n", ok); assert (ok == 22); }
	// 2. same example with the default "reset counter" rule
	{ int ok = appendix_b (true, &bad);
	  printf ("Appendix B example (reset-counter rule) : %d/22 match (differs at row %d)\n", ok, bad); assert (ok == 21); }
	// 3. always-taken branch: bimodal handles it, no global entries needed
	{ pm_predictor p; bool pat[] = {true};
	  int m = run (p, 0x401000, pat, 1, 1000, 0);
	  int valid = 0; for (size_t i = 0; i < p.gtab.size (); i++) valid += p.gtab[i].valid;
	  printf ("always taken                            : %d misses, %d global entries\n", m, valid); assert (m == 0 && valid == 0); }
	// 4. alternating / short loop patterns need the global history
	{ pm_predictor p; bool pat[] = {true, false};
	  int m = run (p, 0x401234, pat, 2, 1000, 50);
	  printf ("alternating T,N                         : %d misses after warm-up\n", m); assert (m == 0); }
	{ pm_predictor p; bool pat[] = {true, true, true, false};
	  int m = run (p, 0x402000, pat, 4, 1000, 50);
	  printf ("loop TTTN                               : %d misses after warm-up\n", m); assert (m == 0); }
	{ pm_predictor p (12, 9, 6, 2, true); bool pat[] = {true, true, false, true, false};
	  int m = run (p, 0x403000, pat, 5, 1000, 50);
	  printf ("2-way, TTNTN                            : %d misses after warm-up\n", m); assert (m == 0); }
	// 5. LRU state is always a permutation
	{ pm_predictor p; bool pat[] = {true, false, false, true, true, false};
	  run (p, 0x404000, pat, 6, 200, 0);
	  for (int s = 0; s < 512; s++) { bool seen[4] = {0};
	    for (int w = 0; w < 4; w++) seen[p.gtab[s*4+w].lru] = true;
	    for (int w = 0; w < 4; w++) assert (seen[w]); }
	  printf ("LRU state                               : ok\n"); }
	// 6. a 30-iteration loop is longer than the 15-bit history: only the loop predictor gets the exit right
	{ bool pat[30]; for (int i = 0; i < 30; i++) pat[i] = i < 29;
	  pm_predictor p1; cpm_predictor p2;
	  int m1 = run (p1, 0x405000, pat, 30, 200, 20), m2 = run (p2, 0x405000, pat, 30, 200, 20);
	  printf ("30-iteration loop, 180 loop exits       : hybrid %d misses, complete PM (loop predictor) %d misses\n", m1, m2);
	  fflush (stdout); assert (m1 == 180 && m2 == 0); }
	// 7. BTB: taken target and fall-through address are predicted after the first visit
	{ cpm_predictor p; branch_info b = cond (0x406000); bool pat[] = {true, false};
	  int tm = 0;
	  for (int r = 0; r < 200; r++) for (int i = 0; i < 2; i++) {
		branch_update *u = p.predict (b);
		unsigned actual = pat[i] ? 0x406100 : 0x406006;          // near jcc: 6 bytes
		if (r >= 20 && u->target_prediction () != actual) tm++;
		p.update (u, pat[i], actual);
	  }
	  printf ("BTB target + fall-through               : %d target misses after warm-up\n", tm); assert (tm == 0); }
	printf ("all tests passed\n");
	return 0;
}
