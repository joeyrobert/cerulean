#include <stdio.h>
#include <string.h>
#include "evaluate.h"
#include "board.h"
#include "util.h"

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

/* Populates the PSTs.
- Using simple linear function. Would polynomials be a better fit? Likely.
*/
void generate_PST() {
    int rank, file;

    /* PAWN */
    int P_rank[] = {0, 0, 5, 10, 15, 20, 30, 0};
    int P_file[] = {0, 0, 5, 10, 10, 5, 0, 0};

    /* KING (from IPPOLIT, http://www.open-chess.org/download/file.php?id=13) */
    int K_rank[] = {4, 1, -2, -5, -10, -15, -25, -35};
    int K_file[] = {40, 45, 15, -5, -5, 15, 45, 40};

    /* KNIGHT (from Rybka/Fruit) */
    int N_rank[] = {-10, -5, 0, 5, 10, 15, 10, 5};
    int N_file[] = {-20, -10, 0, 5, 5, 0, -10, -20};

    /* QUEEN (Queens happy in center. I just made this up) */
    int Q_rank[] = {-5, 0, 2, 5, 5, 2, 0, -5};
    int Q_file[] = {-5, 0, 2, 5, 5, 2, 0, -5};

    /* BISHOP (I made this up too) */
    int B_rank[] = {-10, 0, 10, 8, 7, 7, 0, -10};
    int B_file[] = {-10, 0, 5, 7, 7, 5, 0, -10};

    /* ROOK */ 
    int R_rank[] = {0, 0, 0, 0, 0, 0, 10, 0};
    int R_file[] = {-5, 0, 0, 0, 0, 0, 0, -5};

    for(rank = 0; rank < 8; ++rank) {
        for(file = 0; file < 8; ++file) {
            PST[0][KING][ROWCOLUMN2INDEX(rank, file)] = K_rank[rank] + K_file[file];
            PST[0][KNIGHT][ROWCOLUMN2INDEX(rank, file)] = N_rank[rank] + N_file[file];
            PST[0][QUEEN][ROWCOLUMN2INDEX(rank, file)] = Q_rank[rank] + Q_file[file];
            PST[0][BISHOP][ROWCOLUMN2INDEX(rank, file)] = B_rank[rank] + B_file[file];

            if(rank == 2 && (file == 3 && file == 4))
                PST[0][PAWN][ROWCOLUMN2INDEX(rank, file)] = 0; /* screw d3, e3 */
            else
                PST[0][PAWN][ROWCOLUMN2INDEX(rank, file)] = P_rank[rank] + P_file[file];

            /* Rooks should be happy in the corners. Non-symmetrical parts */
            if((rank == 0 || rank == 7) && (file == 0 || file == 7))
                PST[0][ROOK][ROWCOLUMN2INDEX(rank, file)] = 0;
            else
                PST[0][ROOK][ROWCOLUMN2INDEX(rank, file)] = R_rank[rank] + R_file[file];
        }
    }

    /* Precompile black's lookup tables too */
    for(rank = 0; rank < 8; ++rank) {
        for(file = 0; file < 8; ++file) {
            PST[1][PAWN][ROWCOLUMN2INDEX(rank, file)]   = PST[0][PAWN][ROWCOLUMN2INDEX(7-rank, file)];
            PST[1][BISHOP][ROWCOLUMN2INDEX(rank, file)] = PST[0][BISHOP][ROWCOLUMN2INDEX(7-rank, file)];
            PST[1][KNIGHT][ROWCOLUMN2INDEX(rank, file)] = PST[0][KNIGHT][ROWCOLUMN2INDEX(7-rank, file)];
            PST[1][ROOK][ROWCOLUMN2INDEX(rank, file)]   = PST[0][ROOK][ROWCOLUMN2INDEX(7-rank, file)];
            PST[1][QUEEN][ROWCOLUMN2INDEX(rank, file)]  = PST[0][QUEEN][ROWCOLUMN2INDEX(7-rank, file)];
            PST[1][KING][ROWCOLUMN2INDEX(rank, file)]   = PST[0][KING][ROWCOLUMN2INDEX(7-rank, file)];
        }
    }

}

unsigned piece_values[] = {
    0,          /* EMPTY */
    100,        /* PAWN */
    330,        /* BISHOP */
    330,        /* KNIGHT */
    550,        /* ROOK */
    975,        /* QUEEN */
    0           /* KING */
};

/*
Static Evaluation
- Material evaluation (should this value be stored and constantly updated?)
*/
int static_evaluation(int draw) {
    unsigned i;
    int mat[2] = {0}, mob[2] = {0}, dev[2] = {0}, pos[2] = {0}, trop[2] = {0}, totals[2] = {0}, total;

    for(i = 0; i < w_pieces.count; i++) {
        mat[0] += piece_values[pieces[w_pieces.index[i]]];
        pos[0] += PST[0][pieces[w_pieces.index[i]]][w_pieces.index[i]]; 
    }

    for(i = 0; i < b_pieces.count; i++) {
        mat[1] += piece_values[pieces[b_pieces.index[i]]];
        pos[1] += PST[1][pieces[b_pieces.index[i]]][b_pieces.index[i]]; 
    }

    totals[0] = mat[0] + mob[0] + dev[0] + pos[0];
    totals[1] = mat[1] + mob[1] + dev[1] + pos[1];
    total = totals[0] - totals[1];

    if(draw) {
        printf("              White Black Total\n");
        printf("Material......%5d %5d %5d\n", mat[0], mat[1], mat[0] - mat[1]);
        printf("Mobility......%5d %5d %5d\n", mob[0], mob[1], mob[0] - mob[1]);
        printf("Development...%5d %5d %5d\n", dev[0], dev[1], dev[0] - dev[1]);
        printf("Positioning...%5d %5d %5d\n", pos[0], pos[1], pos[0] - pos[1]);
        printf("\n");
        printf("Total eval....%5d %5d %5d\n", totals[0], totals[1], total);
        printf("\n");
    }

    return total;
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
Positioning
 - Apply positioning tables
*/
int positioning(int side) {
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
