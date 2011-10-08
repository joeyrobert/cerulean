#ifndef MOVE_H
#define MOVE_H
/* 
MOVE BIT STRUCTURE
==================

Moves are passed as one 32-bit unsigned integer. The format of each move is:

    BITS     PROMOTE  FROM     TO
    00000000 00000000 00000000 00000000

The BITS property is defined as follows (borrowed from TSCP):

    1  capture                   (000001) - 50 move rule
    2  castle                    (000010) - castling, tells Board to move rook too
    4  en passant capture        (000100) - tells Board which piece to remove
    8  pushing a pawn 2 squares  (001000) - en passant target square
    16 pawn move                 (010000) - 50 move rule
    32 promote                   (100000) - promotion

The PROMOTE property is equal to the piece (QUEEN, ROOK, BISHOP, KNIGHT).
*/

#define MOVTYPE            unsigned

#define BITS_CAPTURE       0x01000000
#define BITS_CASTLE        0x02000000
#define BITS_ENPASSANT     0x04000000
#define BITS_PAWN_DOUBLE   0x08000000
#define BITS_PAWN_MOVE     0x10000000
#define BITS_PROMOTE       0x20000000

#define MOVE_TO            0x000000FF
#define MOVE_FROM          0x0000FF00
#define MOVE_PROMOTE       0x00FF0000
#define MOVE_BITS          0xFF000000

#define MOVE2TO(move)      (move & MOVE_TO)
#define MOVE2FROM(move)    ((move & MOVE_FROM) >> 8)
#define MOVE2PROMOTE(move) ((move & MOVE_PROMOTE) >> 16)
#define MOVE2BITS(move)    (move & MOVE_BITS)

#define MOVE_NEW(from, to, bits, promote) ((bits) | (promote << 16) | (from << 8) | (to))

#endif
