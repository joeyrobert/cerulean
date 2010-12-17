#include <string.h>
#include "evaluate.h"
#include "board.h"

unsigned piece_values[] = {
    0,          /* EMPTY */
    100,        /* PAWN */
    315,        /* BISHOP */
    320,        /* KNIGHT */
    550,        /* ROOK */
    975,        /* QUEEN */
    INFINITE    /* KING */
};

int static_evaluation() {
    unsigned i, result;
    result = 0;

    for(i = 0; i < w_pieces.count; i++)
        result += piece_values[pieces[w_pieces.index[i]]];

    for(i = 0; i < b_pieces.count; i++)
        result -= piece_values[pieces[b_pieces.index[i]]];
    
    result += pawn_structure();
    //result += mobility();

    return turn*result;
}

int mobility() {

    return 0;
}

int pawn_structure() {
    unsigned i, result, w_column[8], b_column[8], right, left;
    result = 0;

    memset(w_column, 0, 8*sizeof(unsigned));
    memset(b_column, 0, 8*sizeof(unsigned));

    for(i = 0; i < w_pieces.count; i++)
        ++w_column[INDEX2COLUMN(w_pieces.index[i])];

    for(i = 0; i < b_pieces.count; i++)
        ++b_column[INDEX2COLUMN(b_pieces.index[i])];

    for(i = 0; i < 8; i++) {
        /* Doubled-up pawns (-20 for doubled pawns, -30 for tripled, etc.) */
        if(w_column[i] > 1)
            result -= DOUBLE_PAWN_PENALTY*w_column[i];

        if(b_column[i] > 1)
            result += DOUBLE_PAWN_PENALTY*w_column[i];


        /* Isolated pawns (-10 each) */
        left  = (i+1 < 8 ? w_column[i+1] == 0 : 1);
        right = (i-1 < 8 ? w_column[i-1] == 0 : 1);/*  < 8 since they're unsigned*/ 
        if(w_column[i] >= 1 && left && right)
            result -= ISOLATED_PAWN_PENALTY;

        left  = (i+1 < 8 ? b_column[i+1] == 0 : 1);
        right = (i-1 < 8 ? b_column[i-1] == 0 : 1);
        if(b_column[i] >= 1 && left && right)
            result += ISOLATED_PAWN_PENALTY;

    }
    
    return result;
}
