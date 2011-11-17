#ifndef BOARD_H 
#define BOARD_H

#include "piece_list.h"
#include "zobrist.h"
#include "hash_table.h"

#define NO_ENPASSANT 128 /* important for zobrist */
#define OFF          137 /* used in reverse list */
#define EMPTY        0

/* Macros, zero indexed */
#define ROWCOLUMN2INDEX(row, column)  (row * 16 + column)
#define INDEX2ROW(index)              (index >> 4)
#define INDEX2COLUMN(index)           (index & 7)
#define LEGAL_MOVE(move)              ((move & 0x88) == 0 && move < 120)

enum Turn {
    WHITE=1,
    BLACK=-1
};

/* Pieces (important for zobrist) */
enum Pieces {
    PAWN=1,
    BISHOP,
    KNIGHT,
    ROOK,
    QUEEN,
    KING
};

/* Castling */
enum Castling {
    CASTLE_WK=1,
    CASTLE_WQ=2,
    CASTLE_BK=4,
    CASTLE_BQ=8
};

unsigned pieces[128];
int colours[128];

/* Piece lists.

 - Separate piece lists are needed in eval().
 - A full piece list for each side makes for cleaner code
   when all pieces need to be iterated over.
 - King locations are needed in is_in_check().
*/
piece_list w_pieces, b_pieces;
unsigned w_king, b_king;

/* Piece lists by type are nested piece lists. They can be
accessed by w_pieces_by_type[PAWN] for example. */
piece_list w_pieces_by_type[7], b_pieces_by_type[7];

int turn;
unsigned castling;
unsigned enpassant_target;
unsigned half_move_clock;
unsigned full_move_number; /* incremented after blacks move */
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
unsigned gen_caps(unsigned*);
void board_capture_piece(unsigned, unsigned);
void move_piece(unsigned, unsigned);
void move_piece_discreetly(unsigned, unsigned);
unsigned is_in_check(int);
unsigned is_attacked(unsigned, int);

#endif
