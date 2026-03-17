#ifndef EVALUATE_H
#define EVALUATE_H

#include <stdint.h>
#include "zobrist.h"

/* Ported from ceruleanjs eval_params.json */
#define DOUBLED_PAWN_PENALTY         20
#define ISOLATED_PAWN_PENALTY        10
#define BACKWARD_PAWN_PENALTY         8
#define PASSED_PAWN_BONUS            32
#define CANDIDATE_PASSED_BONUS       10
#define PROTECTED_PAWN_BONUS         10
#define PAWN_WING_ADVANCE_BONUS       4
#define CONNECTED_PASSED_BONUS       20

#define KNIGHT_CLOSED_GAME_BONUS      5
#define KNIGHT_OUTPOST_BONUS         12
#define KNIGHT_ON_RIM_PENALTY         8
#define KNIGHT_MOBILITY_BONUS         5

#define BISHOP_DOUBLE_BONUS           5
#define BISHOP_OPPOSITE_COLOR_BONUS   8
#define BISHOP_PAIRITY_PENALTY       10
#define BISHOP_OPEN_GAME_BONUS        8
#define BISHOP_MOBILITY_BONUS         5

#define ROOK_SEMI_OPEN_FILE_BONUS    10
#define ROOK_OPEN_FILE_BONUS         15
#define ROOK_ON_SEVENTH_BONUS        22
#define ROOK_BEHIND_PASSED_BONUS     14
#define ROOK_OPEN_FILE_VS_KING_BONUS 12
#define ROOK_MOBILITY_BONUS           5

#define QUEEN_MOBILITY_BONUS          5
#define QUEEN_ON_SEVENTH_BONUS       15

#define KING_MOBILITY_BONUS           5
#define KING_SHIELD_PENALTY           5
#define KING_ACTIVITY_BONUS          10

#define CENTER_CONTROL_BONUS          8
#define TEMPO_BONUS                  20

/* Evaluation coefficients (matching ceruleanjs) */
#define MATERIAL_COEFF    100
#define PST_COEFF         100
#define MOBILITY_COEFF    100
#define PIECE_BONUS_COEFF 100
#define PAWN_STRUCT_COEFF 100
#define CENTER_COEFF      100
#define TOTAL_COEFF       600

/* Piece values (ceruleanjs values) */
/* Indexed by cerulean piece enum: EMPTY=0,PAWN=1,BISHOP=2,KNIGHT=3,ROOK=4,QUEEN=5,KING=6 */
extern int piece_values[7];

/* Mate/infinite values */
#define MATE_VALUE 1000000
#define INFINITE   (MATE_VALUE + 5000)

/* PST tables: [color_index][piece][square_0x88]
   color_index: 0=white, 1=black */
int PST[2][7][128];
int PST_KING_LATE[2][128];

/* Single-entry hash tables for eval and pawn caching */
typedef struct {
    ZOBRIST key;
    int score;
} single_hash_node;

extern single_hash_node *eval_table;
extern uint64_t eval_table_size;
extern single_hash_node *pawn_table;
extern uint64_t pawn_table_size;

void generate_PST(void);
void init_eval_tables(int mb);

int static_evaluation(int display);

#endif
