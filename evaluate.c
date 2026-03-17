/* evaluate.c - Static evaluation ported from ceruleanjs
 *
 * Evaluation components (each multiplied by coefficient, then divided by TOTAL_COEFF):
 *   Material, PST, Mobility, Piece Bonuses, Pawn Structure, Center Control
 *
 * Returns score from the side-to-move's perspective.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "evaluate.h"
#include "board.h"
#include "util.h"

/* Piece values: {EMPTY, PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING} */
int piece_values[7] = {0, 100, 310, 300, 500, 975, 20000};

/* Eval/pawn hash tables (single-value, always-replace) */
single_hash_node *eval_table = NULL;
uint64_t eval_table_size = 0;
single_hash_node *pawn_table = NULL;
uint64_t pawn_table_size = 0;

/* -----------------------------------------------------------------------
 * Piece Square Tables (from ceruleanjs)
 * Layout: PST[color_idx][piece][0x88_index]
 *   color_idx 0 = white, 1 = black
 * The JS PST arrays are indexed [a8..h8, a7..h7, ..., a1..h1]
 * For white: pst_js_index = (7 - row) * 8 + col
 * For black: pst_js_index = row * 8 + col  (mirrored)
 * ----------------------------------------------------------------------- */

/* ceruleanjs PST arrays (white-oriented, a8=0, h1=63) */
static const int JS_PST_PAWN[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
     6,  10,  14,  18,  18,  14,  10,   6,
     4,   8,  12,  16,  16,  12,   8,   4,
     3,   6,   9,  12,  12,   9,   6,   3,
     2,   4,   6,   8,   8,   6,   4,   2,
     1,   2,   3, -12, -12,   3,   2,   1,
     0,   0,   0, -45, -45,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0
};

static const int JS_PST_KNIGHT[64] = {
    -25, -20, -15, -15, -15, -15, -20, -25,
    -20, -10,   0,   5,   5,   0, -10, -20,
    -15,   0,  10,  15,  15,  10,   0, -15,
    -15,   5,  15,  20,  20,  15,   5, -15,
    -15,   5,  15,  20,  20,  15,   5, -15,
    -15,   0,  10,  15,  15,  10,   0, -15,
    -20, -10,   0,   0,   0,   0, -10, -20,
    -25, -20, -15, -15, -15, -15, -20, -25
};

static const int JS_PST_BISHOP[64] = {
    -15, -10, -10, -10, -10, -10, -10, -15,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   5,  10,  12,  12,  10,   5, -10,
    -10,   5,  12,  15,  15,  12,   5, -10,
    -10,   5,  12,  15,  15,  12,   5, -10,
    -10,   5,  10,  12,  12,  10,   5, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -15, -10, -10, -10, -10, -10, -10, -15
};

static const int JS_PST_ROOK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

static const int JS_PST_QUEEN[64] = {
    -10, -5, -5, -1, -1, -5, -5,-10,
     -5,  0,  2,  2,  2,  2,  0, -5,
     -5,  2,  5,  6,  6,  5,  2, -5,
     -1,  2,  6,  8,  8,  6,  2, -1,
      0,  2,  6,  8,  8,  6,  2, -1,
     -5,  2,  5,  6,  6,  5,  2, -5,
     -5,  0,  2,  2,  2,  2,  0, -5,
    -10, -5, -5, -1, -1, -5, -5,-10
};

static const int JS_PST_KING_EARLY[64] = {
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -20, -20, -20, -20, -20, -20, -20, -20,
      0,  20,  40, -20,   0, -20,  40,  20
};

static const int JS_PST_KING_LATE[64] = {
      0,  10,  20,  30,  30,  20,  10,   0,
     10,  20,  30,  40,  40,  30,  20,  10,
     20,  30,  40,  50,  50,  40,  30,  20,
     30,  40,  50,  60,  60,  50,  40,  30,
     30,  40,  50,  60,  60,  50,  40,  30,
     20,  30,  40,  50,  50,  40,  30,  20,
     10,  20,  30,  40,  40,  30,  20,  10,
      0,  10,  20,  30,  30,  20,  10,   0
};

void generate_PST(void) {
    int row, col, sq, js_white, js_black;

    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            sq = ROWCOLUMN2INDEX(row, col);
            /* JS white PST: (7-row)*8+col  (row 0=rank1=bottom) */
            js_white = (7 - row) * 8 + col;
            /* JS black PST: mirrored (row 0 = black's rank1) */
            js_black = row * 8 + col;

            PST[0][PAWN][sq]   = JS_PST_PAWN[js_white];
            PST[0][KNIGHT][sq] = JS_PST_KNIGHT[js_white];
            PST[0][BISHOP][sq] = JS_PST_BISHOP[js_white];
            PST[0][ROOK][sq]   = JS_PST_ROOK[js_white];
            PST[0][QUEEN][sq]  = JS_PST_QUEEN[js_white];
            PST[0][KING][sq]   = JS_PST_KING_EARLY[js_white];
            PST_KING_LATE[0][sq] = JS_PST_KING_LATE[js_white];

            PST[1][PAWN][sq]   = JS_PST_PAWN[js_black];
            PST[1][KNIGHT][sq] = JS_PST_KNIGHT[js_black];
            PST[1][BISHOP][sq] = JS_PST_BISHOP[js_black];
            PST[1][ROOK][sq]   = JS_PST_ROOK[js_black];
            PST[1][QUEEN][sq]  = JS_PST_QUEEN[js_black];
            PST[1][KING][sq]   = JS_PST_KING_EARLY[js_black];
            PST_KING_LATE[1][sq] = JS_PST_KING_LATE[js_black];
        }
    }
}

/* -----------------------------------------------------------------------
 * Single-value hash table helpers
 * ----------------------------------------------------------------------- */

void init_eval_tables(int mb) {
    /* 25% for eval, 25% for pawn */
    int bytes_per = (mb * 1024 * 1024) / 4;
    int exp;
    uint64_t new_size;

    exp = 0;
    while (((uint64_t)sizeof(single_hash_node) << (exp + 1)) <= (uint64_t)bytes_per)
        exp++;
    new_size = (uint64_t)1 << exp;
    if (new_size < 1) new_size = 1;

    free(eval_table);
    free(pawn_table);

    eval_table = (single_hash_node*)calloc(new_size, sizeof(single_hash_node));
    pawn_table = (single_hash_node*)calloc(new_size, sizeof(single_hash_node));
    eval_table_size = new_size;
    pawn_table_size = new_size;
}

static inline single_hash_node* single_find(single_hash_node *tbl, uint64_t size, ZOBRIST key) {
    single_hash_node *n = &tbl[key & (size - 1)];
    return (n->key == key) ? n : NULL;
}

static inline void single_add(single_hash_node *tbl, uint64_t size, ZOBRIST key, int score) {
    single_hash_node *n = &tbl[key & (size - 1)];
    n->key = key;
    n->score = score;
}

/* -----------------------------------------------------------------------
 * Pawn hash: XOR of all pawn square keys
 * ----------------------------------------------------------------------- */
static ZOBRIST compute_pawn_hash(void) {
    ZOBRIST h = 0;
    unsigned i;
    for (i = 0; i < w_pieces_by_type[PAWN].count; i++)
        h ^= zobrist_w[PAWN][w_pieces_by_type[PAWN].index[i]];
    for (i = 0; i < b_pieces_by_type[PAWN].count; i++)
        h ^= zobrist_b[PAWN][b_pieces_by_type[PAWN].index[i]];
    return h;
}

/* -----------------------------------------------------------------------
 * Game phase: 1.0 = opening, 0.0 = endgame
 * ----------------------------------------------------------------------- */
static double game_phase(void) {
    int check = 24;
    check -= (int)w_pieces_by_type[KNIGHT].count;
    check -= (int)b_pieces_by_type[KNIGHT].count;
    check -= (int)w_pieces_by_type[BISHOP].count;
    check -= (int)b_pieces_by_type[BISHOP].count;
    check -= (int)(w_pieces_by_type[ROOK].count * 2);
    check -= (int)(b_pieces_by_type[ROOK].count * 2);
    check -= (int)(w_pieces_by_type[QUEEN].count * 4);
    check -= (int)(b_pieces_by_type[QUEEN].count * 4);
    if (check < 0) check = 0;
    if (check > 24) check = 24;
    return (24.0 - check) / 24.0;
}

/* Closed game: 1.0 = fully closed, 0.0 = fully open */
static double closed_game(void) {
    int total = (int)(w_pieces_by_type[PAWN].count + b_pieces_by_type[PAWN].count);
    return total / 16.0;
}

/* -----------------------------------------------------------------------
 * Mobility helpers
 * ----------------------------------------------------------------------- */
extern int delta_knight[8];
extern int delta_king[8];
extern int delta_diagonal[4];
extern int delta_vertical[4];

static int delta_move_count(int *deltas, int n, unsigned idx, int piece_color) {
    int count = 0, d, ni;
    for (d = 0; d < n; d++) {
        ni = (int)idx + deltas[d];
        if (LEGAL_MOVE((unsigned)ni) && colours[(unsigned)ni] != piece_color)
            count++;
    }
    return count;
}

static int sliding_move_count(int *deltas, int n, unsigned idx, int piece_color) {
    int count = 0, d, ni;
    for (d = 0; d < n; d++) {
        ni = (int)idx;
        do {
            ni += deltas[d];
            if (!LEGAL_MOVE((unsigned)ni) || colours[(unsigned)ni] == piece_color) break;
            count++;
            if (pieces[(unsigned)ni] != EMPTY) break;
        } while (1);
    }
    return count;
}

/* -----------------------------------------------------------------------
 * Square color: 0 or 1 (like index % 2 in JS but for 0x88)
 * ----------------------------------------------------------------------- */
#define SQ_COLOR(idx) ((INDEX2ROW(idx) + INDEX2COLUMN(idx)) & 1)

/* -----------------------------------------------------------------------
 * Pawn structure evaluation (returns white - black differential)
 * ----------------------------------------------------------------------- */
static int eval_pawn(unsigned idx, int color_idx,
                     int pawn_rank[2][8], int pawns_by_file[2][8]) {
    int bonus = 0;
    int col = (int)INDEX2COLUMN(idx);
    int row = (int)INDEX2ROW(idx);
    int rank_offset   = (color_idx == 0) ? (7 - row) : row;
    int inv_rank_off  = 7 - rank_offset;

    /* Doubled pawn: friendly pawn one square behind */
    int behind_row = (color_idx == 0) ? row - 1 : row + 1;
    if (behind_row >= 0 && behind_row < 8) {
        unsigned behind_sq = ROWCOLUMN2INDEX(behind_row, col);
        if ((behind_sq & 0x88) == 0 && pieces[behind_sq] == PAWN &&
            colours[behind_sq] == (color_idx == 0 ? WHITE : BLACK))
            bonus -= DOUBLED_PAWN_PENALTY;
    }

    /* Isolated pawn */
    int left_empty  = (col <= 0 || pawns_by_file[color_idx][col-1] == 0);
    int right_empty = (col >= 7 || pawns_by_file[color_idx][col+1] == 0);
    if (left_empty && right_empty)
        bonus -= ISOLATED_PAWN_PENALTY;

    /* Backward pawn */
    if ((col - 1 >= 0 && pawn_rank[color_idx][col-1] > rank_offset) &&
        (col + 1 <= 7 && pawn_rank[color_idx][col+1] > rank_offset))
        bonus -= BACKWARD_PAWN_PENALTY;

    /* Passed pawn */
    int opp = 1 - color_idx;
    int is_passed = ((col - 1 < 0  || pawn_rank[opp][col-1] <= inv_rank_off) &&
                     (pawn_rank[opp][col]   <= inv_rank_off) &&
                     (col + 1 > 7  || pawn_rank[opp][col+1] <= inv_rank_off));
    if (is_passed) {
        bonus += PASSED_PAWN_BONUS;
        if ((col > 0 && pawns_by_file[color_idx][col-1] > 0) ||
            (col < 7 && pawns_by_file[color_idx][col+1] > 0))
            bonus += CONNECTED_PASSED_BONUS;
    } else if (rank_offset < 7) {
        /* Candidate passed pawn */
        int inv_next = inv_rank_off + 1;
        if ((col - 1 < 0  || pawn_rank[opp][col-1] <= inv_rank_off) &&
            (pawn_rank[opp][col]   <= inv_next) &&
            (col + 1 > 7  || pawn_rank[opp][col+1] <= inv_rank_off))
            bonus += CANDIDATE_PASSED_BONUS;
    }

    /* Wing advance bonus */
    if (rank_offset >= 4 && (col <= 2 || col >= 5))
        bonus += PAWN_WING_ADVANCE_BONUS * (rank_offset - 3);

    /* Protected pawn: friendly pawn diagonally behind */
    {
        int behind_dir = (color_idx == 0) ? -1 : 1;
        int bl = (int)idx + behind_dir * 17;
        int br = (int)idx + behind_dir * 15;
        int fc = (color_idx == 0) ? WHITE : BLACK;
        if (bl >= 0 && (((unsigned)bl) & 0x88) == 0 &&
            pieces[(unsigned)bl] == PAWN && colours[(unsigned)bl] == fc)
            bonus += PROTECTED_PAWN_BONUS;
        else if (br >= 0 && (((unsigned)br) & 0x88) == 0 &&
                 pieces[(unsigned)br] == PAWN && colours[(unsigned)br] == fc)
            bonus += PROTECTED_PAWN_BONUS;
    }

    return bonus;
}

/* -----------------------------------------------------------------------
 * Pawn structure: returns (white_total - black_total) differential
 * ----------------------------------------------------------------------- */
static int compute_pawn_structure(int pawn_rank[2][8], int pawns_by_file[2][8]) {
    int result = 0;
    unsigned i, idx;

    for (i = 0; i < w_pieces_by_type[PAWN].count; i++) {
        idx = w_pieces_by_type[PAWN].index[i];
        result += eval_pawn(idx, 0, pawn_rank, pawns_by_file);
    }
    for (i = 0; i < b_pieces_by_type[PAWN].count; i++) {
        idx = b_pieces_by_type[PAWN].index[i];
        result -= eval_pawn(idx, 1, pawn_rank, pawns_by_file);
    }
    return result;
}

/* -----------------------------------------------------------------------
 * Main evaluation
 * ----------------------------------------------------------------------- */
int static_evaluation(int display) {
    int i, col_idx, row, col;
    unsigned idx, piece;
    double gphase, cgame;
    single_hash_node *cached;

    /* Eval hash lookup (skip in display mode) */
    if (!display && eval_table && eval_table_size) {
        cached = single_find(eval_table, eval_table_size, zobrist);
        if (cached) {
            int cached_score = cached->score;
            return (turn == WHITE) ? cached_score : -cached_score;
        }
    }

    gphase = game_phase();
    cgame  = closed_game();

    /* Pawn preprocessing: pawns_by_file[color_idx][file], pawn_rank[color_idx][file],
       pawn_number[color_idx][sq_color] */
    int pawns_by_file[2][8] = {{0},{0}};
    int pawn_rank[2][8]     = {{0},{0}};
    int pawn_number[2][2]   = {{0},{0}};

    for (i = 0; i < (int)w_pieces_by_type[PAWN].count; i++) {
        idx = w_pieces_by_type[PAWN].index[i];
        col = INDEX2COLUMN(idx);
        row = INDEX2ROW(idx);
        int ro = 7 - row;  /* pawnRankOffset for white */
        pawns_by_file[0][col]++;
        if (ro > pawn_rank[0][col]) pawn_rank[0][col] = ro;
        pawn_number[0][SQ_COLOR(idx)]++;
    }
    for (i = 0; i < (int)b_pieces_by_type[PAWN].count; i++) {
        idx = b_pieces_by_type[PAWN].index[i];
        col = INDEX2COLUMN(idx);
        row = INDEX2ROW(idx);
        int ro = row;  /* pawnRankOffset for black */
        pawns_by_file[1][col]++;
        if (ro > pawn_rank[1][col]) pawn_rank[1][col] = ro;
        pawn_number[1][SQ_COLOR(idx)]++;
    }

    /* Pawn structure (from pawn hash) */
    int pawn_struct_diff;
    ZOBRIST ph = compute_pawn_hash();
    if (pawn_table && pawn_table_size) {
        single_hash_node *pn = single_find(pawn_table, pawn_table_size, ph);
        if (pn) {
            pawn_struct_diff = pn->score;
        } else {
            pawn_struct_diff = compute_pawn_structure(pawn_rank, pawns_by_file);
            single_add(pawn_table, pawn_table_size, ph, pawn_struct_diff);
        }
    } else {
        pawn_struct_diff = compute_pawn_structure(pawn_rank, pawns_by_file);
    }

    /* Main evaluation loop */
    int material[2]     = {0, 0};
    int pst[2]          = {0, 0};
    int mobility[2]     = {0, 0};
    int piece_bonus[2]  = {0, 0};
    int center[2]       = {0, 0};

    /* We process both sides */
    for (col_idx = 0; col_idx < 2; col_idx++) {
        piece_list *plist = (col_idx == 0) ? &w_pieces : &b_pieces;
        int fc = (col_idx == 0) ? WHITE : BLACK;

        for (i = 0; i < (int)plist->count; i++) {
            idx = plist->index[i];
            piece = pieces[idx];
            row = INDEX2ROW(idx);
            col = INDEX2COLUMN(idx);
            int rank_offset = (col_idx == 0) ? (7 - row) : row;
            int pst_idx = (col_idx == 0) ?
                ((7 - row) * 8 + col) :
                (row * 8 + col);
            /* pst_idx 0-63, but we use PST[col_idx][piece][idx] */

            material[col_idx] += piece_values[piece];
            pst[col_idx] += PST[col_idx][piece][idx];

            switch (piece) {
            case PAWN:
                /* Pawn PST already accumulated; pawn_struct via hash */
                break;

            case KNIGHT: {
                /* Mobility */
                mobility[col_idx] += KNIGHT_MOBILITY_BONUS *
                    delta_move_count(delta_knight, 8, idx, fc);

                /* Center control bonus (JS: pstIndex & 7 in 2..5, pstIndex in 18..45) */
                if (col >= 2 && col <= 5 && pst_idx >= 18 && pst_idx <= 45)
                    center[col_idx] += CENTER_CONTROL_BONUS;

                /* Closed game bonus */
                if (cgame >= 0.5)
                    piece_bonus[col_idx] += KNIGHT_CLOSED_GAME_BONUS;

                /* Outpost: friendly pawn diagonally forward */
                if (rank_offset > 3) {
                    int fwd = (col_idx == 0) ? 1 : -1;
                    int fl = (int)idx + fwd * 15;
                    int fr = (int)idx + fwd * 17;
                    if ((fl >= 0 && (((unsigned)fl) & 0x88) == 0 &&
                         pieces[(unsigned)fl] == PAWN && colours[(unsigned)fl] == fc) ||
                        (fr >= 0 && (((unsigned)fr) & 0x88) == 0 &&
                         pieces[(unsigned)fr] == PAWN && colours[(unsigned)fr] == fc))
                        piece_bonus[col_idx] += KNIGHT_OUTPOST_BONUS;
                }

                /* Rim penalty */
                if (col == 0 || col == 7)
                    piece_bonus[col_idx] -= KNIGHT_ON_RIM_PENALTY;
                break;
            }

            case BISHOP: {
                /* Mobility */
                mobility[col_idx] += BISHOP_MOBILITY_BONUS *
                    sliding_move_count(delta_diagonal, 4, idx, fc);

                /* Center control */
                if (col >= 2 && col <= 5 && pst_idx >= 18 && pst_idx <= 45)
                    center[col_idx] += CENTER_CONTROL_BONUS;

                /* Bishop pair */
                if (w_pieces_by_type[BISHOP].count >= 2 && col_idx == 0) {
                    piece_bonus[col_idx] += BISHOP_DOUBLE_BONUS;
                    if (w_pieces_by_type[BISHOP].count == 2 &&
                        SQ_COLOR(w_pieces_by_type[BISHOP].index[0]) !=
                        SQ_COLOR(w_pieces_by_type[BISHOP].index[1]))
                        piece_bonus[col_idx] += BISHOP_OPPOSITE_COLOR_BONUS;
                } else if (b_pieces_by_type[BISHOP].count >= 2 && col_idx == 1) {
                    piece_bonus[col_idx] += BISHOP_DOUBLE_BONUS;
                    if (b_pieces_by_type[BISHOP].count == 2 &&
                        SQ_COLOR(b_pieces_by_type[BISHOP].index[0]) !=
                        SQ_COLOR(b_pieces_by_type[BISHOP].index[1]))
                        piece_bonus[col_idx] += BISHOP_OPPOSITE_COLOR_BONUS;
                }

                /* Open game bonus */
                if (cgame < 0.5)
                    piece_bonus[col_idx] += BISHOP_OPEN_GAME_BONUS;

                /* Parity penalty: opponent pawns on same square color */
                piece_bonus[col_idx] -= BISHOP_PAIRITY_PENALTY *
                    pawn_number[1 - col_idx][SQ_COLOR(idx)];
                break;
            }

            case ROOK: {
                /* Mobility */
                mobility[col_idx] += ROOK_MOBILITY_BONUS *
                    sliding_move_count(delta_vertical, 4, idx, fc);

                /* Open/semi-open file */
                int our_adv  = pawn_rank[col_idx][col];
                int their_adv = pawn_rank[1-col_idx][col];

                if (our_adv == 0) {   /* no friendly pawn on this file */
                    if (their_adv == 0) {
                        piece_bonus[col_idx] += ROOK_OPEN_FILE_BONUS;
                        /* vs enemy king */
                        unsigned eking = (col_idx == 0) ? b_king : w_king;
                        int ekcol = INDEX2COLUMN(eking);
                        if (abs(ekcol - col) <= 1)
                            piece_bonus[col_idx] += ROOK_OPEN_FILE_VS_KING_BONUS;
                    } else {
                        piece_bonus[col_idx] += ROOK_SEMI_OPEN_FILE_BONUS;
                    }
                }

                /* Rook on 7th (rank_offset == 6) */
                if (rank_offset == 6)
                    piece_bonus[col_idx] += ROOK_ON_SEVENTH_BONUS;

                /* Rook behind passed pawn */
                if (our_adv >= 4 && (their_adv == 0 || their_adv < our_adv)) {
                    int pawn_row_abs = (col_idx == 0) ? (7 - our_adv) : our_adv;
                    int rook_behind = (col_idx == 0) ? (row > pawn_row_abs) : (row < pawn_row_abs);
                    if (rook_behind)
                        piece_bonus[col_idx] += ROOK_BEHIND_PASSED_BONUS;
                }
                break;
            }

            case QUEEN: {
                /* Mobility */
                mobility[col_idx] += QUEEN_MOBILITY_BONUS *
                    (sliding_move_count(delta_diagonal, 4, idx, fc) +
                     sliding_move_count(delta_vertical, 4, idx, fc));

                /* Queen on 7th */
                if (rank_offset == 6)
                    piece_bonus[col_idx] += QUEEN_ON_SEVENTH_BONUS;
                break;
            }

            case KING: {
                /* Mobility */
                mobility[col_idx] += KING_MOBILITY_BONUS *
                    delta_move_count(delta_king, 8, idx, fc);

                /* King PST: interpolate between early and late */
                int king_pst_early = PST[col_idx][KING][idx];
                int king_pst_late  = PST_KING_LATE[col_idx][idx];
                /* Overwrite what was accumulated above */
                pst[col_idx] -= king_pst_early;
                pst[col_idx] += (int)(gphase * king_pst_early + (1.0 - gphase) * king_pst_late);

                /* Pawn shield */
                if (gphase > 0.3) {
                    int shield_dir = (col_idx == 0) ? 1 : -1;
                    int s1 = (int)idx + shield_dir * 16;
                    int s2 = (int)idx + shield_dir * 15;
                    int s3 = (int)idx + shield_dir * 17;
                    int shield = 0;
                    if (s1 >= 0 && (((unsigned)s1) & 0x88) == 0 &&
                        pieces[(unsigned)s1] == PAWN && colours[(unsigned)s1] == fc) shield++;
                    if (col > 0 && s2 >= 0 && (((unsigned)s2) & 0x88) == 0 &&
                        pieces[(unsigned)s2] == PAWN && colours[(unsigned)s2] == fc) shield++;
                    if (col < 7 && s3 >= 0 && (((unsigned)s3) & 0x88) == 0 &&
                        pieces[(unsigned)s3] == PAWN && colours[(unsigned)s3] == fc) shield++;
                    piece_bonus[col_idx] -= KING_SHIELD_PENALTY * (3 - shield);
                }

                /* King activity in endgame */
                if (gphase < 0.5) {
                    /* Manhattan distance from center (3.5, 3.5) */
                    /* Use integer: |2*col-7| + |2*row-7| = 2 * dist */
                    int dc = abs(2*col - 7);
                    int dr = abs(2*row - 7);
                    /* 4 - centerDist = (8 - dc - dr) / 2 */
                    piece_bonus[col_idx] += KING_ACTIVITY_BONUS * (8 - dc - dr) / 2;
                }
                break;
            }
            }
        }
    }

    /* Total: white - black for each component */
    int mat_diff    = material[0]    - material[1];
    int pst_diff    = pst[0]         - pst[1];
    int mob_diff    = mobility[0]    - mobility[1];
    int pb_diff     = piece_bonus[0] - piece_bonus[1];
    int ctr_diff    = center[0]      - center[1];

    int total =
        MATERIAL_COEFF    * mat_diff +
        PST_COEFF         * pst_diff +
        MOBILITY_COEFF    * mob_diff +
        PIECE_BONUS_COEFF * pb_diff  +
        PAWN_STRUCT_COEFF * pawn_struct_diff +
        CENTER_COEFF      * ctr_diff;

    /* Tempo bonus: add for current side */
    total += (turn == WHITE ? 1 : -1) * (TEMPO_BONUS * TOTAL_COEFF / 6);

    total = total / TOTAL_COEFF;

    if (display) {
        printf("              White Black Total\n");
        printf("Material......%5d %5d %5d\n", material[0], material[1], mat_diff);
        printf("PST...........%5d %5d %5d\n", pst[0], pst[1], pst_diff);
        printf("Mobility......%5d %5d %5d\n", mobility[0], mobility[1], mob_diff);
        printf("PieceBonuses..%5d %5d %5d\n", piece_bonus[0], piece_bonus[1], pb_diff);
        printf("PawnStruct....%5d (diff=%d)\n", 0, pawn_struct_diff);
        printf("Center........%5d %5d %5d\n", center[0], center[1], ctr_diff);
        printf("\nTotal eval....%5d (white perspective)\n\n", total);
        /* return white-perspective for display */
        return (turn == WHITE) ? total : -total;
    }

    /* Cache the white-perspective score */
    if (eval_table && eval_table_size)
        single_add(eval_table, eval_table_size, zobrist, total);

    return (turn == WHITE) ? total : -total;
}
