// my_predictor.h
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

//
// Pentium M hybrid branch predictors
// This class implements a simple hybrid branch predictor based on the Pentium M branch outcome prediction units. 
// Instead of implementing the complete Pentium M branch outcome predictors, the class below implements a hybrid 
// predictor that combines a bimodal predictor and a global predictor. 
//
// Author: Rushi Kedhar Konduru (PSID 2481344)
//
// Structure (Pentium M diagram, Uzelac & Milenkovic, ISPASS 2009):
//
//   Bimodal table : 4096 x 2-bit counters, index = IP[11:0]
//   Global table  : 512 sets x 4 ways (2 ways also supported); entry =
//                   {valid, 6-bit tag, 2-bit counter}; true-LRU replacement
//                   HASH = IP[14:0] XOR GHR[14:0]   (GHR used instead of the PIR)
//                   set  = HASH[14:6] (9 bits -> 512 sets),  tag = HASH[5:0]
//
//   Prediction : global hit  -> global entry's counter
//                global miss -> bimodal counter
//   Update     : bimodal counter always trained;
//                global hit  -> train hit entry, make it MRU;
//                global miss -> allocate the invalid/LRU way, but only when the
//                               bimodal prediction was wrong (default);
//                GHR shifts in the outcome of every conditional branch.
//
// Only conditional branches are predicted (others: taken). No target prediction.
//
#include <vector>

// 2-bit saturating counter helper shared by all predictors below
static inline void sat2_train (unsigned char &c, bool taken) {
        if (taken) { if (c < 3) c++; }
        else       { if (c > 0) c--; }
}

class pm_update : public branch_update {
public:
        unsigned int index;        // bimodal table index
        unsigned int set;          // global table set
        unsigned int tag;          // global table tag
        int          way;          // way that hit in the global table, -1 on miss
        bool         bim_pred;     // bimodal prediction
};

class pm_predictor : public branch_predictor {
public:
        // ---- configuration (defaults = Pentium M sizes) ----
        int  BIM_BITS;          // log2(bimodal entries)
        int  SET_BITS;          // log2(global sets)
        int  TAG_BITS;          // global tag width
        int  WAYS;              // global associativity (4 = Pentium M, 2 = allowed simplification)
        int  HIST_BITS;         // GHR length = hash width = SET_BITS + TAG_BITS
        bool ALLOC_ON_BIM_MISS; // true: allocate only when bimodal mispredicts; false: on every global miss
        bool RESET_CTR_ON_ALLOC;// true: new entry's counter = weak toward outcome;
                                // false: keep the victim's old counter and train it (handout Appendix B)

        struct gentry {
                bool          valid;
                unsigned int  tag;
                unsigned char ctr;   // 2-bit counter, >= 2 means taken
                unsigned int  lru;   // 0 = MRU ... WAYS-1 = LRU
        };

        pm_update u;
        branch_info bi;
        unsigned int ghr;                    // global history register
        std::vector<unsigned char> bim;      // bimodal counters
        std::vector<gentry> gtab;            // global table, [set*WAYS + way]

        pm_predictor (int bim_bits = 12, int set_bits = 9, int tag_bits = 6, int ways = 4,
                      bool alloc_on_bim_miss = true, bool reset_ctr_on_alloc = true)
                : BIM_BITS(bim_bits), SET_BITS(set_bits), TAG_BITS(tag_bits),
                  WAYS(ways), HIST_BITS(set_bits + tag_bits),
                  ALLOC_ON_BIM_MISS(alloc_on_bim_miss), RESET_CTR_ON_ALLOC(reset_ctr_on_alloc),
                  ghr(0) {
                bim.assign (1u << BIM_BITS, 2);          // weakly taken
                gtab.resize ((size_t)(1u << SET_BITS) * WAYS);
                for (unsigned int s = 0; s < (1u << SET_BITS); s++)
                        for (int w = 0; w < WAYS; w++) {
                                gentry &e = gtab[s * WAYS + w];
                                e.valid = false; e.tag = 0; e.ctr = 2; e.lru = w;
                        }
        }

        // make 'way' the most recently used way of 'set'
        void touch (unsigned int set, int way) {
                gentry *row = &gtab[set * WAYS];
                unsigned int old = row[way].lru;
                for (int w = 0; w < WAYS; w++)
                        if (row[w].lru < old) row[w].lru++;
                row[way].lru = 0;
        }

        // victim: first invalid way, otherwise the LRU way
        int victim (unsigned int set) {
                gentry *row = &gtab[set * WAYS];
                for (int w = 0; w < WAYS; w++) if (!row[w].valid) return w;
                for (int w = 0; w < WAYS; w++) if (row[w].lru == (unsigned int)(WAYS - 1)) return w;
                return 0;
        }

        static void train (unsigned char &c, bool taken) { sat2_train (c, taken); }

        branch_update *predict (branch_info & b) {
                bi = b;
                if (b.br_flags & BR_CONDITIONAL) {
                        // bimodal lookup
                        u.index    = b.address & ((1u << BIM_BITS) - 1);
                        u.bim_pred = bim[u.index] >= 2;

                        // global lookup
                        unsigned int hash = (b.address ^ ghr) & ((1u << HIST_BITS) - 1);
                        u.set = hash >> TAG_BITS;
                        u.tag = hash & ((1u << TAG_BITS) - 1);
                        u.way = -1;
                        gentry *row = &gtab[u.set * WAYS];
                        for (int w = 0; w < WAYS; w++)
                                if (row[w].valid && row[w].tag == u.tag) { u.way = w; break; }

                        // final prediction: global on hit, bimodal otherwise
                        u.direction_prediction (u.way >= 0 ? row[u.way].ctr >= 2 : u.bim_pred);
                } else {
                        u.direction_prediction (true);   // unconditional branches are taken
                }
                u.target_prediction (0);                 // no target prediction in this part
                return &u;
        }

        void update (branch_update *bu, bool taken, unsigned int target) {
                if (!(bi.br_flags & BR_CONDITIONAL)) return;
                pm_update *pu = (pm_update *) bu;

                // 1) bimodal always learns
                train (bim[pu->index], taken);

                // 2) global table
                gentry *row = &gtab[pu->set * WAYS];
                if (pu->way >= 0) {
                        train (row[pu->way].ctr, taken);
                        touch (pu->set, pu->way);
                } else if (!ALLOC_ON_BIM_MISS || pu->bim_pred != taken) {
                        int w = victim (pu->set);
                        row[w].valid = true;
                        row[w].tag   = pu->tag;
                        if (RESET_CTR_ON_ALLOC) row[w].ctr = taken ? 2 : 1;
                        else                    train (row[w].ctr, taken);
                        touch (pu->set, w);
                }

                // 3) global history
                ghr = ((ghr << 1) | (taken ? 1 : 0)) & ((1u << HIST_BITS) - 1);
        }

};

//
// Complete Pentium M branch predictors for extra credit
// This class implements the complete Pentium M branch prediction units. 
// It implements both branch target prediction and branch outcome predicton. 
//
// Structures (sizes from the Pentium M diagram):
//   BTB    : 512 sets x 4 ways, index IP[12:4], tag IP[21:13] + offset IP[3:0],
//            type, 32-bit target, tree-PLRU (3 bits/set). Holds taken branches.
//   iBTB   : 256 sets x 4 ways, index HASH[13:6], tag {HASH[14],HASH[5:0]} (7 bits),
//            32-bit target, LRU. Allocated for indirect branches the BTB mispredicts.
//   LPB    : loop predictor, 64 sets x 2 ways, index IP[9:4], tag IP[15:10],
//            6-bit count, 6-bit limit, 1-bit loop direction, 2-bit confidence, LRU.
//            Trusted after the same trip count is seen LOOP_CONF times in a row.
//   Bimodal: 4096 x 2-bit, index IP[11:0]
//   Global : 512 x 4 ways, 6-bit tag, 2-bit counter, LRU; HASH = IP[14:0] XOR PIR
//   PIR    : 15-bit path history, updated on every branch as in Intel's patent
//            US 2005/0262332: PIR = ((PIR ^ addr) << 1 | taken), where addr is the
//            branch address (direct) or the target address (indirect).
//
//   Outcome : loop hit (LPB hit, confident, and BTB hit) ? loop : bimodal,
//             then a global hit overrides both - except that a confident loop
//             prediction keeps priority (LOOP_PRIORITY). Without that, a global entry
//             created while the loop was being learned keeps overriding the loop
//             predictor for loops longer than the history (see report).
//   Target  : predicted taken  -> iBTB target (indirect & iBTB hit) or BTB target,
//                                 0 on a BTB miss (the decoder would fix it later);
//             predicted not taken -> fall-through address = IP + instruction length.
//                                 Real x86 fetch knows this from the I-cache predecode
//                                 (length) bits; a 4096-entry table of learned jcc lengths
//                                 (2 = short, 6 = near) stands in for them. Before a
//                                 length is known it is guessed from the displacement.
//
class cpm_update : public branch_update {
public:
        unsigned int index;               // bimodal index
        unsigned int gset, gtag; int gway;// global
        unsigned int lset, ltag; int lway;// loop predictor
        unsigned int bset, btag; int bway;// BTB
        unsigned int iset, itag; int iway;// iBTB
        bool bim_pred, base_pred, loop_used;
        unsigned int btb_target_pred;     // what the BTB/iBTB said (taken target)
};

class cpm_predictor : public branch_predictor {
public:
        bool USE_PIR;   // true: PIR path history (Pentium M); false: conditional-outcome GHR
        bool USE_LOOP;  // enable the loop predictor
        int  LOOP_CONF; // same trip count must repeat this many times before the LPB is trusted
        bool LOOP_PRIORITY; // true: a confident loop prediction overrides a global hit (see report)

        enum { HBITS = 15 };

        struct gentry { bool valid; unsigned int tag; unsigned char ctr; unsigned char lru; };
        struct lentry { bool valid; unsigned int tag; unsigned int count, limit; bool dir; unsigned char conf; };
        struct bentry { bool valid; unsigned int tag, offset, target; bool indirect; };
        struct ientry { bool valid; unsigned int tag, target; unsigned char lru; };

        cpm_update u;
        branch_info bi;
        unsigned int hist;                       // PIR or GHR
        std::vector<unsigned char> bim;          // 4096
        std::vector<gentry> gtab;                // 512 x 4
        std::vector<lentry> ltab;                // 64 x 2
        std::vector<unsigned char> llru;         // 64 (way that is LRU)
        std::vector<bentry> btb;                 // 512 x 4
        std::vector<unsigned char> bplru;        // 512 x 3-bit tree PLRU
        std::vector<ientry> ibtb;                // 256 x 4
        std::vector<unsigned char> ilen;         // 4096 learned jcc lengths (0 = unknown), predecode stand-in

        cpm_predictor (bool use_pir = true, bool use_loop = true, int loop_conf = 3, bool loop_priority = true)
                : USE_PIR(use_pir), USE_LOOP(use_loop), LOOP_CONF(loop_conf), LOOP_PRIORITY(loop_priority), hist(0) {
                bim.assign (4096, 2);
                gtab.resize (512 * 4);
                for (int i = 0; i < 512 * 4; i++) { gtab[i].valid = false; gtab[i].tag = 0; gtab[i].ctr = 2; gtab[i].lru = i % 4; }
                ltab.resize (64 * 2);
                for (int i = 0; i < 64 * 2; i++) { ltab[i].valid = false; ltab[i].conf = 0; }
                llru.assign (64, 0);
                btb.resize (512 * 4);
                for (int i = 0; i < 512 * 4; i++) btb[i].valid = false;
                bplru.assign (512, 0);
                ilen.assign (4096, 0);
                ibtb.resize (256 * 4);
                for (int i = 0; i < 256 * 4; i++) { ibtb[i].valid = false; ibtb[i].lru = i % 4; }
        }

        // ---- generic true-LRU for small arrays with an 'lru' field ----
        template <class E> static void lru_touch (E *row, int ways, int way) {
                unsigned char old = row[way].lru;
                for (int w = 0; w < ways; w++) if (row[w].lru < old) row[w].lru++;
                row[way].lru = 0;
        }
        template <class E> static int lru_victim (E *row, int ways) {
                for (int w = 0; w < ways; w++) if (!row[w].valid) return w;
                for (int w = 0; w < ways; w++) if (row[w].lru == ways - 1) return w;
                return 0;
        }

        // ---- tree pseudo-LRU for the 4-way BTB (bit0: left/right, bit1: left pair, bit2: right pair) ----
        void plru_touch (unsigned int set, int way) {
                unsigned char &p = bplru[set];
                if (way < 2) { p |= 1; if (way == 0) p |= 2; else p &= ~2; }
                else         { p &= ~1; if (way == 2) p |= 4; else p &= ~4; }
        }
        int plru_victim (unsigned int set) {
                bentry *row = &btb[set * 4];
                for (int w = 0; w < 4; w++) if (!row[w].valid) return w;
                unsigned char p = bplru[set];
                if (!(p & 1)) return (p & 2) ? 1 : 0;
                return (p & 4) ? 3 : 2;
        }

        static unsigned int fallthrough (unsigned int addr, unsigned int taken_target) {
                // a short jcc (rel8) is 2 bytes, a near jcc (rel32) is 6 bytes
                int disp = (int)(taken_target - (addr + 2));
                return addr + ((disp >= -128 && disp <= 127) ? 2 : 6);
        }

        branch_update *predict (branch_info & b) {
                bi = b;
                unsigned int a = b.address;
                unsigned int hash = (a ^ hist) & ((1u << HBITS) - 1);

                // BTB lookup (all branches)
                u.bset = (a >> 4) & 511;
                u.btag = (a >> 13) & 511;
                u.bway = -1;
                bentry *brow = &btb[u.bset * 4];
                for (int w = 0; w < 4; w++)
                        if (brow[w].valid && brow[w].tag == u.btag && brow[w].offset == (a & 15)) { u.bway = w; break; }

                // iBTB lookup
                u.iset = (hash >> 6) & 255;
                u.itag = (((hash >> 14) & 1) << 6) | (hash & 63);
                u.iway = -1;
                ientry *irow = &ibtb[u.iset * 4];
                for (int w = 0; w < 4; w++)
                        if (irow[w].valid && irow[w].tag == u.itag) { u.iway = w; break; }

                // taken-target prediction
                u.btb_target_pred = 0;
                if (u.bway >= 0) {
                        u.btb_target_pred = brow[u.bway].target;
                        if (brow[u.bway].indirect && u.iway >= 0) u.btb_target_pred = irow[u.iway].target;
                }

                if (b.br_flags & BR_CONDITIONAL) {
                        // bimodal
                        u.index    = a & 4095;
                        u.bim_pred = bim[u.index] >= 2;

                        // loop predictor
                        u.loop_used = false;
                        u.lset = (a >> 4) & 63;
                        u.ltag = (a >> 10) & 63;
                        u.lway = -1;
                        bool loop_pred = false;
                        if (USE_LOOP) {
                                lentry *lrow = &ltab[u.lset * 2];
                                for (int w = 0; w < 2; w++)
                                        if (lrow[w].valid && lrow[w].tag == u.ltag) { u.lway = w; break; }
                                if (u.lway >= 0 && lrow[u.lway].conf >= LOOP_CONF && u.bway >= 0) {
                                        lentry &e = lrow[u.lway];
                                        loop_pred   = (e.count == e.limit) ? !e.dir : e.dir;
                                        u.loop_used = true;
                                }
                        }
                        u.base_pred = u.loop_used ? loop_pred : u.bim_pred;

                        // global predictor overrides on a hit
                        u.gset = hash >> 6;
                        u.gtag = hash & 63;
                        u.gway = -1;
                        gentry *grow = &gtab[u.gset * 4];
                        for (int w = 0; w < 4; w++)
                                if (grow[w].valid && grow[w].tag == u.gtag) { u.gway = w; break; }
                        bool dir = (u.gway >= 0 && !(LOOP_PRIORITY && u.loop_used)) ? grow[u.gway].ctr >= 2 : u.base_pred;
                        u.direction_prediction (dir);

                        // next-PC prediction
                        if (dir) u.target_prediction (u.btb_target_pred);
                        else if (ilen[a & 4095]) u.target_prediction (a + ilen[a & 4095]);
                        else     u.target_prediction (u.bway >= 0 ? fallthrough (a, u.btb_target_pred) : a + 2);
                } else {
                        u.direction_prediction (true);
                        u.target_prediction (u.btb_target_pred);
                }
                return &u;
        }

        void update (branch_update *bu, bool taken, unsigned int target) {
                cpm_update *p = (cpm_update *) bu;
                unsigned int a = bi.address;
                bool cond     = bi.br_flags & BR_CONDITIONAL;
                bool indirect = bi.br_flags & (BR_INDIRECT | BR_RETURN);

                if (cond) {
                        bool mispred = p->direction_prediction () != taken;

                        // bimodal
                        sat2_train (bim[p->index], taken);

                        // learn the instruction length when the branch falls through
                        if (!taken && target - a < 16) ilen[a & 4095] = (unsigned char)(target - a);

                        // loop predictor
                        if (USE_LOOP) {
                                lentry *lrow = &ltab[p->lset * 2];
                                if (p->lway >= 0) {
                                        lentry &e = lrow[p->lway];
                                        if (taken == e.dir) {
                                                if (++e.count > 63) { e.count = 0; e.conf = 0; }  // too long for 6 bits
                                        } else {
                                                if (e.count == e.limit && e.limit > 0) { if (e.conf < 3) e.conf++; }
                                                else e.conf = 0;              // trip count changed: not a counted loop
                                                e.limit = e.count;
                                                e.count = 0;
                                        }
                                        llru[p->lset] = 1 - p->lway;
                                } else if (mispred) {
                                        int w = !lrow[0].valid ? 0 : (!lrow[1].valid ? 1 : llru[p->lset]);
                                        lentry &e = lrow[w];
                                        e.valid = true; e.tag = p->ltag; e.dir = !taken;
                                        e.count = 0; e.limit = 0; e.conf = 0;
                                        llru[p->lset] = 1 - w;
                                }
                        }

                        // global predictor: allocate when the base (loop/bimodal) prediction was wrong
                        gentry *grow = &gtab[p->gset * 4];
                        if (p->gway >= 0) {
                                sat2_train (grow[p->gway].ctr, taken);
                                lru_touch (grow, 4, p->gway);
                        } else if (p->base_pred != taken) {
                                int w = lru_victim (grow, 4);
                                grow[w].valid = true; grow[w].tag = p->gtag; grow[w].ctr = taken ? 2 : 1;
                                lru_touch (grow, 4, w);
                        }
                }

                // BTB: holds taken branches
                if (taken) {
                        bentry *brow = &btb[p->bset * 4];
                        int w = p->bway;
                        if (w < 0) w = plru_victim (p->bset);
                        bool btb_wrong = (p->bway < 0) || brow[w].target != target;
                        brow[w].target = target;                 // BTB keeps the most recent target
                        brow[w].valid = true; brow[w].tag = p->btag; brow[w].offset = a & 15;
                        brow[w].indirect = indirect;
                        plru_touch (p->bset, w);

                        // iBTB: indirect branches whose target the BTB got wrong
                        if (indirect) {
                                ientry *irow = &ibtb[p->iset * 4];
                                if (p->iway >= 0) {
                                        irow[p->iway].target = target;
                                        lru_touch (irow, 4, p->iway);
                                } else if (btb_wrong) {
                                        int v = lru_victim (irow, 4);
                                        irow[v].valid = true; irow[v].tag = p->itag; irow[v].target = target;
                                        lru_touch (irow, 4, v);
                                }
                        }
                }

                // history
                if (USE_PIR) {
                        unsigned int portion = indirect ? target : a;
                        hist = (((hist ^ portion) << 1) | (taken ? 1 : 0)) & ((1u << HBITS) - 1);
                } else if (cond) {
                        hist = ((hist << 1) | (taken ? 1 : 0)) & ((1u << HBITS) - 1);
                }
        }

};
