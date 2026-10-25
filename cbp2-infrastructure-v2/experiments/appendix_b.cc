// appendix_b.cc - replays the handout's Appendix B example through pm_predictor
// and prints a markdown table (used in the report).
// build/run from cbp2-infrastructure-v2/:
//   g++ -O2 -o experiments/appendix_b experiments/appendix_b.cc && experiments/appendix_b
#include <stdio.h>
#include <string.h>
#include "../src/branch.h"
#include "../src/predictor.h"
#include "../src/my_predictor.h"

static void bits (char *s, unsigned v, int n) { for (int i = 0; i < n; i++) s[i] = ((v >> (n - 1 - i)) & 1) ? '1' : '0'; s[n] = 0; }

int main () {
	const char *name[]    = {"b1","b3","b1","b2","b3","b1","b3","b1","b2","b3","b1","b2","b1","b2","b3","b1","b3","b1","b2","b3","b1","b3"};
	const unsigned addr[] = {0x44,0x90,0x44,0x6C,0x90,0x44,0x90,0x44,0x6C,0x90,0x44,0x6C,0x44,0x6C,0x90,0x44,0x90,0x44,0x6C,0x90,0x44,0x90};
	const bool out[]      = {1,1,0,0,1,1,1,0,0,1,0,1,0,0,1,1,1,0,0,1,1,1};
	const bool expect[]   = {0,0,1,1,0,1,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1};
	pm_predictor p (4, 2, 2, 2, false, false);   // handout settings
	for (int i = 0; i < 16; i++) p.bim[i] = i % 4;
	for (int r = 0; r < 4; r++) { p.gtab[r*2].ctr = r; p.gtab[r*2+1].ctr = 3 - r; }
	printf ("| # | Branch | Outcome | GHR | Index [5:2] | XOR | Row | Tag | Local | Global | Final (mine) | Final (handout) |\n");
	printf ("|--:|---|---|---|--:|---|--:|---|---|---|---|---|\n");
	int ok = 0;
	for (int i = 0; i < 22; i++) {
		branch_info b; b.address = addr[i] >> 2; b.opcode = 0; b.br_flags = BR_CONDITIONAL;
		char g[8], h[8], t[8]; bits (g, p.ghr, 4);
		branch_update *u = p.predict (b);
		bits (h, (p.u.set << 2) | p.u.tag, 4); bits (t, p.u.tag, 2);
		bool gl = p.u.way >= 0, gp = gl && p.gtab[p.u.set * 2 + p.u.way].ctr >= 2;
		bool f = u->direction_prediction ();
		ok += f == expect[i];
		printf ("| %d | %s | %s | %s | %u | %s | %u | %s | %s | %s | %s | %s%s |\n", i + 1, name[i], out[i] ? "T" : "NT", g, p.u.index, h, p.u.set, t,
		        p.u.bim_pred ? "T" : "NT", gl ? (gp ? "hit, T" : "hit, NT") : "miss", f ? "T" : "NT", expect[i] ? "T" : "NT", f == expect[i] ? "" : " (differs)");
		p.update (u, out[i], 0);
	}
	printf ("\nmatching rows: %d/22\n", ok);
	return 0;
}
