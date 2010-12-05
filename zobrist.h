#ifndef ZOBRIST_H 
#define ZOBRIST_H

#include <stdint.h>

#define ZOBRIST uint64_t

ZOBRIST zobrist_w[7][128];      /* size 7 so it maps PIECENAMES */
ZOBRIST zobrist_b[7][128];
ZOBRIST zobrist_side;           /* changes sides */
ZOBRIST zobrist_enpassant[129]; /* 129 so it maps NO_ENPASSANT */
ZOBRIST zobrist_castling[9];    /* size 9 so it maps CASTLE_* */

ZOBRIST zobrist_rand();
void zobrist_fill();

#endif
