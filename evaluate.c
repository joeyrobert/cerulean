#include <string.h>
#include "evaluate.h"
#include "board.h"
#include "util.h"

unsigned piece_values[] = {
    0,          /* EMPTY */
    100,        /* PAWN */
    330,        /* BISHOP */
    330,        /* KNIGHT */
    550,        /* ROOK */
    975,        /* QUEEN */
    INFINITE    /* KING */
};

void generate_distance() {
    unsigned A, B;
    int i, j, diff;

    for(i = 0; i < 64; i++) {
        for(j = 0; j < 64; j++) {
            A = ROWCOLUMN2INDEX(i/8, i%8);
            B = ROWCOLUMN2INDEX(j/8, j%8);

            diff = MAX(ABS(i/8 - j/8), ABS(i%8 - j%8));
            distance[0x77 + A - B] = diff;
        }
    }
}

void generate_vertical_distance() {
    unsigned i, j, A, B, diff;

    for(i = 0; i < 64; i++) {
        for(j = 0; j < 64; j++) {
            A = ROWCOLUMN2INDEX(i/8, i%8);
            B = ROWCOLUMN2INDEX(j/8, j%8);

            diff = ABS(i/8 - j/8) + ABS(i%8 - j%8);
            vertical_distance[0x77 + A - B] = diff;
        }
    }
}

/*
Static Evaluation:
 - Weighted sum of scores
*/
int static_evaluation() {
    int result;

    result  = material();
    result += mobility();
    result += development();
    result += pawn_structure();

    return turn*result;
}

/*
Piece Evaluation:
 - Sums piece_values for each side.
*/
int material() {
    unsigned i, result;
    result = 0;

    for(i = 0; i < w_pieces.count; i++)
        result += piece_values[pieces[w_pieces.index[i]]];

    for(i = 0; i < b_pieces.count; i++)
        result -= piece_values[pieces[b_pieces.index[i]]];

    return result;
}

/*
Mobility:
 - Knight distance to center

TODO:
 - Knight outposts
 - Bishop effectiveness
*/
int knight_effectiveness[] = {20, 10, 5, 0};
int mobility() {
    unsigned i, result, index, distance_to_center;
    result = 0;
    
    for(i = 0; i < w_pieces.count; i++) {
        index = w_pieces.index[i];
        if(pieces[index] == KNIGHT) {
            distance_to_center = MIN(
                MIN(distance[0x77+index-51], distance[0x77+index-52]),
                MIN(distance[0x77+index-67], distance[0x77+index-68])
            );
            result += knight_effectiveness[distance_to_center];
        }
    }

    for(i = 0; i < b_pieces.count; i++) {
        index = b_pieces.index[i];
        if(pieces[index] == KNIGHT) {
            distance_to_center = MIN(
                MIN(distance[0x77+index-51], distance[0x77+index-52]),
                MIN(distance[0x77+index-67], distance[0x77+index-68])
            );
            result -= knight_effectiveness[distance_to_center];
        }
    }

    return result;
}

/*
Development:
 -

TODO:
 - King safety (early)
 - King importance (endgame)
*/
int development() {
    return 0;
}

/*
Pawn structure:
 - Doubled pawn penalty     (15)
 - Isolated pawn penalty    (15)

TODO:
 - Backward pawn penalty    (10)
*/
int defensive_places[] = {-15, -17, -1, 1};
int pawn_structure() {
    unsigned i, j, result, w_column[8], b_column[8], right, left;
    unsigned defender, defended;
    result = 0;

    memset(w_column, 0, 8*sizeof(unsigned));
    memset(b_column, 0, 8*sizeof(unsigned));

    for(i = 0; i < w_pieces.count; i++) {
        ++w_column[INDEX2COLUMN(w_pieces.index[i])];
        //
        ///* Backward pawn */
        //defended = 0;
        //for(j = 0; j < 4; j++) {
        //    defender = w_pieces.index[i] + defensive_places[j];
        //    if(pieces[defender] == PAWN && colours[defender] == WHITE) {
        //        defended = 1;
        //        break;
        //    }
        //}

        //if(!defended)
        //    result -= BACKWARD_PAWN_PENALTY;
    }

    for(i = 0; i < b_pieces.count; i++) {
        ++b_column[INDEX2COLUMN(b_pieces.index[i])];
    
        ///* Backward pawn */
        //defended = 0;
        //for(j = 0; j < 4; j++) {
        //    defender = b_pieces.index[i] - defensive_places[j];
        //    if(pieces[defender] == PAWN && colours[defender] == BLACK) {
        //        defended = 1;
        //        break;
        //    }
        //}

        //if(!defended)
        //    result += BACKWARD_PAWN_PENALTY;
    }

    for(i = 0; i < 8; i++) {
        /* Doubled-up pawns */
        if(w_column[i] > 1)
            result -= DOUBLE_PAWN_PENALTY;

        if(b_column[i] > 1)
            result += DOUBLE_PAWN_PENALTY;

        /* Isolated pawns */
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
