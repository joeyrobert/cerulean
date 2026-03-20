/* search.c - Negamax search with full ceruleanjs intelligence
 *
 * Features:
 *   Iterative deepening, aspiration windows, PVS, LMR, razoring,
 *   futility pruning, IID, killer moves, MVV/LVA, relative history heuristic,
 *   SEE sign, transposition table (3 tables: search/eval/pawn)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "search.h"
#include "board.h"
#include "evaluate.h"
#include "util.h"
#include "move.h"

/* -----------------------------------------------------------------------
 * Timing helpers
 * ----------------------------------------------------------------------- */
long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

static int time_elapsed(void) {
    return (int)(get_time_ms() - search_start_ms);
}

/* -----------------------------------------------------------------------
 * Sq64 for history indexing
 * ----------------------------------------------------------------------- */
static inline int sq64(unsigned sq) {
    return (INDEX2ROW(sq) * 8) + INDEX2COLUMN(sq);
}

static inline int hist_index(unsigned move) {
    return sq64(MOVE2FROM(move)) * 64 + sq64(MOVE2TO(move));
}

static inline int turn_idx(void) {
    return (turn == WHITE) ? 0 : 1;
}

/* -----------------------------------------------------------------------
 * MVV/LVA values (0..6 mapped from cerulean pieces)
 * ----------------------------------------------------------------------- */
static const int mvv_lva_vals[7] = {0, 1, 2, 3, 4, 5, 6};
/* {EMPTY, PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING} */

static int mvv_lva(unsigned move) {
    unsigned to = MOVE2TO(move);
    unsigned from = MOVE2FROM(move);
    unsigned cap, atk;

    if (move & BITS_ENPASSANT)
        return mvv_lva_vals[PAWN] * 5 + 5 - mvv_lva_vals[PAWN];

    cap = pieces[to];
    if (cap == EMPTY) return 0;
    atk = pieces[from];
    return mvv_lva_vals[cap] * 5 + 5 - mvv_lva_vals[atk];
}

/* SEE gain: captured piece value minus attacker value (positive = winning) */
static int see_gain(unsigned move) {
    unsigned to = MOVE2TO(move);
    unsigned from = MOVE2FROM(move);
    if (move & BITS_ENPASSANT) return 0;  /* neutral */
    int cap = (int)pieces[to];
    if (cap == EMPTY) return 0;
    return piece_values[cap] - piece_values[pieces[from]];
}

/* Relative history score 0..189 */
static int rel_history(int ti, int hidx) {
    uint32_t b = butterfly_heuristic[ti][hidx];
    if (b == 0) return 0;
    return (int)(history_heuristic[ti][hidx] * 189u / b);
}

/* -----------------------------------------------------------------------
 * Mate score TT adjustments
 * ----------------------------------------------------------------------- */
static int score_to_tt(int score, int ply) {
    if (score > MATE_VALUE - 1000) return score + ply;
    if (score < -(MATE_VALUE - 1000)) return score - ply;
    return score;
}

static int score_from_tt(int score, int ply) {
    if (score > MATE_VALUE - 1000) return score - ply;
    if (score < -(MATE_VALUE - 1000)) return score + ply;
    return score;
}

/* -----------------------------------------------------------------------
 * Move ordering
 * ----------------------------------------------------------------------- */
#define ORDER_HASH    10000000
#define ORDER_PV      9000000
#define ORDER_KILLER  8000000
#define ORDER_GOODCAP 6000000
#define ORDER_PROMO   5000000
#define ORDER_BADCAP  3000000

static void score_moves(unsigned *moves, int *scores, unsigned count,
                        unsigned tt_move, unsigned pv_move, int ply) {
    unsigned i, m, to, from;
    int ti = turn_idx();

    for (i = 0; i < count; i++) {
        m = moves[i];
        from = MOVE2FROM(m);
        to   = MOVE2TO(m);
        int hidx = sq64(from) * 64 + sq64(to);

        /* Strip order bits (none in C moves, just use m directly) */
        if (tt_move && m == tt_move) {
            scores[i] = ORDER_HASH;
        } else if (pv_move && m == pv_move) {
            scores[i] = ORDER_PV;
        } else if (!(m & BITS_CAPTURE) && !(m & BITS_ENPASSANT) && ply < MAX_PLY &&
                   (m == killer_moves[ply][0] || m == killer_moves[ply][1])) {
            scores[i] = ORDER_KILLER;
        } else if (m & BITS_PROMOTE) {
            scores[i] = ORDER_PROMO;
        } else if ((m & BITS_CAPTURE) || (m & BITS_ENPASSANT)) {
            int mva = mvv_lva(m);
            int rh  = rel_history(ti, hidx);
            if (see_gain(m) >= 0)
                scores[i] = ORDER_GOODCAP + mva * 100 + rh;
            else
                scores[i] = ORDER_BADCAP + mva * 100 + rh;
        } else {
            scores[i] = rel_history(ti, hidx);
        }
    }
}

/* Selection sort: pick best move at position i */
static void pick_move(unsigned *moves, int *scores, unsigned start, unsigned count) {
    unsigned best = start, j;
    for (j = start + 1; j < count; j++) {
        if (scores[j] > scores[best]) best = j;
    }
    if (best != start) {
        unsigned tm = moves[start]; moves[start] = moves[best]; moves[best] = tm;
        int ts = scores[start]; scores[start] = scores[best]; scores[best] = ts;
    }
}

/* Legacy sort for perft/simple use */
void moves_sort(unsigned *moves, int *scores, unsigned move_count) {
    unsigned i, j, best, count = move_count;
    int ts;
    unsigned tm;
    for (i = 0; i < count; i++) {
        best = i;
        for (j = i + 1; j < count; j++) {
            if (scores[j] > scores[best]) best = j;
        }
        if (best != i) {
            tm = moves[i]; moves[i] = moves[best]; moves[best] = tm;
            ts = scores[i]; scores[i] = scores[best]; scores[best] = ts;
        }
    }
}

/* -----------------------------------------------------------------------
 * Quiescence search
 * ----------------------------------------------------------------------- */
int qsearch(int alpha, int beta, int ply) {
    unsigned moves[256], count, i;
    int scores[256];
    int stand_pat, score;
    (void)ply;

    if (ended_early) return 0;
    if ((nodes_searched & 1023) == 0 && time_elapsed() >= (int)time_per_move_ms) {
        ended_early = 1;
        return 0;
    }

    /* TT lookup */
    hash_node *node = hash_find(table, zobrist);
    if (node) {
        int tt_score = score_from_tt(node->score, ply);
        if (node->type == HASH_EXACT) return tt_score;
        if (node->type == HASH_ALPHA && tt_score <= alpha) return alpha;
        if (node->type == HASH_BETA  && tt_score >= beta)  return beta;
    }

    stand_pat = static_evaluation(0);

    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    count = gen_caps(moves);

    /* Score moves (MVV/LVA only in qsearch) */
    for (i = 0; i < count; i++)
        scores[i] = mvv_lva(moves[i]);

    for (i = 0; i < count; i++) {
        pick_move(moves, scores, i, count);
        unsigned m = moves[i];

        /* Delta pruning */
        unsigned to = MOVE2TO(m);
        int cap_val = 0;
        if (m & BITS_ENPASSANT) cap_val = piece_values[PAWN];
        else if (m & BITS_CAPTURE) cap_val = piece_values[pieces[to]];
        if (cap_val > 0 && stand_pat + cap_val + 350 < alpha) continue;

        if (!board_add(m)) continue;
        nodes_searched++;
        score = -qsearch(-beta, -alpha, ply + 1);
        board_subtract();

        if (ended_early) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

/* -----------------------------------------------------------------------
 * Main search (negamax with alpha-beta, PVS, LMR)
 * ----------------------------------------------------------------------- */
int search(int depth, int alpha, int beta, int ply) {
    unsigned moves[256], count, i;
    int scores[256];
    int score, searched_moves, stand_pat;
    unsigned tt_move = 0;
    int tt_depth = 0, tt_type = -1, tt_score = 0;
    hash_node *node;

    if (ended_early) return 0;
    if ((nodes_searched & 1023) == 0 && time_elapsed() >= (int)time_per_move_ms) {
        ended_early = 1;
        return 0;
    }

    /* TT lookup */
    node = hash_find(table, zobrist);
    if (node) {
        tt_score = score_from_tt(node->score, ply);
        tt_depth = node->depth;
        tt_type  = node->type;
        if (node->move) tt_move = node->move;

        if (tt_depth >= depth) {
            if (tt_type == HASH_EXACT) {
                pv_table[ply][ply] = tt_move;
                pv_length[ply] = ply + 1;
                return tt_score;
            }
            if (tt_type == HASH_ALPHA && tt_score <= alpha) return alpha;
            if (tt_type == HASH_BETA  && tt_score >= beta)  return beta;
        }
    }

    if (depth == 0)
        return qsearch(alpha, beta, ply);

    stand_pat = static_evaluation(0);

    /* Razoring (depth 1) */
    if (depth == 1 && stand_pat + 350 < alpha)
        return qsearch(alpha, beta, ply);

    /* Null move pruning: skip our turn and see if opponent can beat beta.
     * Only when not in check, have non-pawn pieces (avoid zugzwang), and
     * static eval suggests we're above beta (we're winning). */
    {
        int our_pieces = (turn == WHITE)
            ? (int)(w_pieces_by_type[KNIGHT].count + w_pieces_by_type[BISHOP].count +
                    w_pieces_by_type[ROOK].count   + w_pieces_by_type[QUEEN].count)
            : (int)(b_pieces_by_type[KNIGHT].count + b_pieces_by_type[BISHOP].count +
                    b_pieces_by_type[ROOK].count   + b_pieces_by_type[QUEEN].count);
        if (depth >= 3 && our_pieces > 0 && stand_pat >= beta - 150 && !is_in_check(turn)) {
            int R = (depth >= 6) ? 3 : 2;
            board_do_null_move();
            score = -search(depth - 1 - R, -beta, -beta + 1, ply + 1);
            board_undo_null_move();
            if (ended_early) return 0;
            if (score >= beta)
                return beta;
        }
    }

    /* Internal Iterative Deepening */
    if (!tt_move && depth >= 7) {
        search(depth - 1, alpha, beta, ply);
        node = hash_find(table, zobrist);
        if (node && node->move) tt_move = node->move;
        if (ended_early) return 0;
    }

    count = gen_moves(moves);

    /* Determine PV move from previous iteration */
    unsigned pv_move = 0;
    if (ply > 0 && pv_length[ply-1] > ply)
        pv_move = pv_table[ply-1][ply];

    score_moves(moves, scores, count, tt_move, pv_move, ply);

    pv_length[ply] = ply;
    searched_moves = 0;
    unsigned alpha_move = 0;
    int default_type = HASH_ALPHA;

    for (i = 0; i < count; i++) {
        pick_move(moves, scores, i, count);
        unsigned m = moves[i];
        int is_capture   = (m & BITS_CAPTURE) || (m & BITS_ENPASSANT);
        int is_promotion = (m & BITS_PROMOTE) != 0;
        int hidx = hist_index(m);
        int ti   = turn_idx();

        /* Futility pruning at depth 1: check before make/unmake */
        if (depth == 1 && !is_capture && !is_promotion && stand_pat + 110 < alpha) {
            butterfly_heuristic[ti][hidx]++;
            continue;
        }

        if (!board_add(m)) continue;
        searched_moves++;
        nodes_searched++;

        /* Check extension: extend 1 ply when move gives check */
        int gives_check = is_in_check(turn);
        int extension = gives_check ? 1 : 0;

        /* LMR: don't reduce checks or check-givers */
        int reduction = 0;
        if (!extension && alpha_move && (int)i > 5 && !is_capture && !gives_check && depth >= 4)
            reduction = 1;

        int sdepth = depth - 1 + extension - reduction;

        if (!alpha_move) {
            score = -search(sdepth, -beta, -alpha, ply + 1);
        } else {
            /* Null-window search */
            score = -search(sdepth, -alpha - 1, -alpha, ply + 1);
            if (!ended_early && score > alpha)
                score = -search(depth - 1, -beta, -alpha, ply + 1);
        }

        /* Re-search with full depth if LMR failed high */
        if (!ended_early && reduction > 0 && score >= beta)
            score = -search(depth - 1, -beta, -alpha, ply + 1);

        board_subtract();
        butterfly_heuristic[ti][hidx]++;

        if (ended_early) return 0;

        if (score >= beta) {
            /* Beta cutoff */
            if (!is_capture && ply < MAX_PLY) {
                if (killer_moves[ply][0] != m) {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = m;
                }
            }
            history_heuristic[ti][hidx]++;
            hash_add_move(table, zobrist, depth,
                          score_to_tt(score, ply), HASH_BETA, m);
            return beta;
        }

        if (score > alpha) {
            alpha = score;
            alpha_move = m;
            default_type = HASH_EXACT;

            /* Update PV */
            pv_table[ply][ply] = m;
            if (ply + 1 < MAX_PLY && pv_length[ply+1] > ply + 1) {
                int j;
                for (j = ply + 1; j < pv_length[ply+1]; j++)
                    pv_table[ply][j] = pv_table[ply+1][j];
                pv_length[ply] = pv_length[ply+1];
            } else {
                pv_length[ply] = ply + 1;
            }
        }
    }

    if (searched_moves == 0) {
        if (is_in_check(turn)) return -(MATE_VALUE + depth);
        return 0;  /* stalemate */
    }

    hash_add_move(table, zobrist, depth,
                  score_to_tt(alpha, ply), default_type,
                  alpha_move ? alpha_move : tt_move);
    return alpha;
}

/* -----------------------------------------------------------------------
 * Heuristic management
 * ----------------------------------------------------------------------- */
void clear_heuristics(void) {
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_heuristic, 0, sizeof(history_heuristic));
    memset(butterfly_heuristic, 0, sizeof(butterfly_heuristic));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));
}

/* -----------------------------------------------------------------------
 * Time management
 * ----------------------------------------------------------------------- */
void update_time_per_move(void) {
    int ideal = base_time_ms / (moves_per_tc > 0 ? moves_per_tc : 50) + increment_ms;
    int remaining = engine_time_ms / 3;
    time_per_move_ms = (ideal < remaining) ? ideal : remaining;
    if (time_per_move_ms < 10) time_per_move_ms = 10;
}

/* -----------------------------------------------------------------------
 * Iterative deepening
 * ----------------------------------------------------------------------- */
unsigned iterative_deepening(int time_ms, int max_dep, int hide_display) {
    int depth, score = 0, prev_score = 0;
    char move_str[10];
    unsigned best = 0;
    int d;

    search_start_ms = get_time_ms();
    time_per_move_ms = time_ms - 1;  /* go slightly under */
    nodes_searched = 0;
    ended_early = 0;

    clear_heuristics();
    /* Clear search TT for new search */
    memset(table->table, 0, sizeof(hash_node) * table->size);

    for (depth = 1; depth <= max_dep && depth < MAX_PLY; depth++) {
        memset(pv_table[0], 0, sizeof(pv_table[0]));
        pv_length[0] = 0;

        /* Aspiration windows */
        int alpha, beta;
        if (depth > 1 && score > -(MATE_VALUE - 1000) && score < (MATE_VALUE - 1000)) {
            alpha = prev_score - 50;
            beta  = prev_score + 50;
        } else {
            alpha = -INFINITE;
            beta  =  INFINITE;
        }

        score = search(depth, alpha, beta, 0);

        if (!ended_early && (score <= alpha || score >= beta)) {
            /* Re-search with full window */
            score = search(depth, -INFINITE, INFINITE, 0);
        }

        if (ended_early) {
            /* Use previous depth's result */
            break;
        }

        prev_score = score;

        /* Extract best move from PV */
        if (pv_length[0] > 0) {
            best = pv_table[0][0];
        } else {
            /* Fallback: try TT */
            hash_node *n = hash_find(table, zobrist);
            if (n && n->move) best = n->move;
        }

        if (!hide_display && best) {
            /* Print PV: "depth score time nodes pv..." */
            int elapsed_cs = (int)(time_elapsed() / 10);
            printf("%d %d %d %llu", depth, score, elapsed_cs,
                   (unsigned long long)nodes_searched);
            for (d = 0; d < pv_length[0] && d < depth; d++) {
                move_to_string(pv_table[0][d], move_str);
                printf(" %s", move_str);
            }
            printf("\n");
            fflush(stdout);
        }

        if (time_elapsed() >= (int)time_per_move_ms) break;
    }

    return best;
}

/* -----------------------------------------------------------------------
 * search_root: compatibility wrapper used by xboard
 * ----------------------------------------------------------------------- */
unsigned search_root(void) {
    update_time_per_move();
    return iterative_deepening((int)time_per_move_ms, max_depth, 0);
}
