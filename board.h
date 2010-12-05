#ifndef BOARD_H 
#define BOARD_H

#include "piece_list.h"
#include "zobrist.h"
#include "hash_table.h"

#define WHITE        1
#define BLACK        -1
#define NO_ENPASSANT 128 /* important for zobrist */
#define OFF          137 /* used in reverse list */
#define EMPTY        0

/* Pieces (important for zobrist) */
#define PAWN   1
#define BISHOP 2
#define KNIGHT 3
#define ROOK   4
#define QUEEN  5
#define KING   6

/* Castling */
#define CASTLE_WK 1
#define CASTLE_WQ 2
#define CASTLE_BK 4
#define CASTLE_BQ 8

/* Macros, zero indexed */
#define ROWCOLUMN2INDEX(row, column)  (row * 16 + column)
#define INDEX2ROW(index)              (index >> 4)
#define INDEX2COLUMN(index)           (index & 7)
#define LEGAL_MOVE(move)              ((move & 0x88) == 0 && move < 120)

unsigned pieces[128];
int colours[128];
piece_list w_pieces, b_pieces;
unsigned w_king, b_king; /* saves lookup time in check methods */

int turn;
unsigned castling;
unsigned enpassant_target;
unsigned half_move_clock;
unsigned full_move_number;
unsigned history[2048][4];
unsigned total_history;

ZOBRIST zobrist;
ZOBRIST zobrist_history[2048];
hash_table *table;

void board_create_table();
void board_new();
void board_draw();
void board_set_fen(char*);
unsigned board_add(unsigned);
void board_subtract();
ZOBRIST board_gen_zobrist();
void board_enpassant(unsigned);
unsigned gen_moves(unsigned*);
void board_capture_piece(unsigned, unsigned);
void move_piece(unsigned, unsigned);
void move_piece_discreetly(unsigned, unsigned);
unsigned is_in_check(int);
unsigned is_attacked(unsigned, int);
unsigned board_debug();

#endif
