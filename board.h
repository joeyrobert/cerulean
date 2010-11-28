#ifndef BOARD_H 
#define BOARD_H

#include "piece_list.h"

#define WHITE        1
#define BLACK        -1
#define NO_EN_PASSANT  137
#define OFF          137 /* used in reverse list */
#define EMPTY        0

/* Pieces */
#define PAWN   1
#define BISHOP 2
#define KNIGHT 4
#define ROOK   8
#define QUEEN  16
#define KING   32

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
unsigned en_passant_target;
unsigned half_move_clock;
unsigned full_move_number;
unsigned history[1024][4];
unsigned total_history;

void board_new();
void board_draw();
void board_set_fen(char*);
unsigned board_add(unsigned);
void board_subtract();
unsigned gen_moves(unsigned*);

#endif
