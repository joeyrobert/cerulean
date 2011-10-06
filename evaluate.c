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
    0           /* KING */
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
    unsigned A, B;
    int i, j, diff;

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

    result = material(WHITE);
    result -= material(BLACK);
    result += mobility(WHITE);
    result -= mobility(BLACK);
    result += development(WHITE);
    result -= development(BLACK);

    return turn*result;
}

/*
Static Evaluation draw
*/
void static_evaluation_draw() {
    int mat[2], mob[2], dev[2];

    mat[0] = material(WHITE);
    mat[1] = material(BLACK);
    mob[0] = mobility(WHITE);
    mob[1] = mobility(BLACK);
    dev[0] = development(WHITE);
    dev[1] = development(BLACK);

    printf("              White Black Total\n");
    printf("Material......%5d %5d %5d\n", mat[0], mat[1], mat[0] - mat[1]);
    printf("Mobility......%5d %5d %5d\n", mob[0], mob[1], mob[0] - mob[1]);
    printf("Development...%5d %5d %5d\n", dev[0], dev[1], dev[0] - dev[1]);
    printf("\n");
}

/*
Piece Evaluation:
 - Sums piece_values for each side.
 - +ve for both black and white
*/
int material(int side) {
    unsigned i, result;
    result = 0;

    if(side == WHITE) { 
        for(i = 0; i < w_pieces.count; i++) {
            result += piece_values[pieces[w_pieces.index[i]]];
        }
    } else {
        for(i = 0; i < b_pieces.count; i++) {
            result += piece_values[pieces[b_pieces.index[i]]];
        }
    }

    return result;
}

/*
Mobility:
 - Knight distance to center

TODO:
 - Knight outposts
 - Bishop effectiveness
*/
int knight_effectiveness[] = {10, 8, 3, 0};
int mobility(int side) {
    unsigned i, result, index, distance_to_center;
    result = 0;
    
    if(side == WHITE) {
        for(i = 0; i < w_pieces_by_type[KNIGHT].count; i++) {
            index = w_pieces_by_type[KNIGHT].index[i];
            distance_to_center = MIN(
                MIN(distance[0x77+index-51], distance[0x77+index-52]),
                MIN(distance[0x77+index-67], distance[0x77+index-68])
            );
            result += knight_effectiveness[distance_to_center];
        }
    } else {
        for(i = 0; i < b_pieces_by_type[KNIGHT].count; i++) {
            index = b_pieces_by_type[KNIGHT].index[i];
            distance_to_center = MIN(
                MIN(distance[0x77+index-51], distance[0x77+index-52]),
                MIN(distance[0x77+index-67], distance[0x77+index-68])
            );
            result += knight_effectiveness[distance_to_center];
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
int development(int side) {
    return 0;
}

/*
Pawn structure:
 - Doubled pawn penalty
 - Isolated pawn penalty

TODO:
 - Backward pawn penalty
*/
int defensive_places[] = {-15, -17, -1, 1};
int pawn_structure(int side) {
    unsigned i, j, result, w_column[8], b_column[8], right, left, index;
    unsigned defender, defended;
    result = 0;

    memset(w_column, 0, 8*sizeof(unsigned));
    memset(b_column, 0, 8*sizeof(unsigned));

    for(i = 0; i < w_pieces_by_type[PAWN].count; i++) {
		index = w_pieces_by_type[PAWN].index[i];
        ++w_column[INDEX2COLUMN(index)];
        
        /* Backward pawn */
        defended = 0;
        for(j = 0; j < 4; j++) {
            defender = index + defensive_places[j];
            if(pieces[defender] == PAWN && colours[defender] == WHITE) {
                defended = 1;
                break;
            }
        }

        if(!defended)
            result -= BACKWARD_PAWN_PENALTY;
    }

    for(i = 0; i < b_pieces_by_type[PAWN].count; i++) {
		index = b_pieces_by_type[PAWN].index[i];
        ++b_column[INDEX2COLUMN(index)];
    
        /* Backward pawn */
        defended = 0;
        for(j = 0; j < 4; j++) {
            defender = index - defensive_places[j];
            if(pieces[defender] == PAWN && colours[defender] == BLACK) {
                defended = 1;
                break;
            }
        }

        if(!defended)
            result += BACKWARD_PAWN_PENALTY;
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
