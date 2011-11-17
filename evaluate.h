#ifndef EVALUATE_H 
#define EVALUATE_H

#define INFINITE 10000
#define DOUBLE_PAWN_PENALTY 5
#define ISOLATED_PAWN_PENALTY 10
#define BACKWARD_PAWN_PENALTY 2

/* Distance related */
unsigned distance[240];
unsigned vertical_distance[240];

/* Piece square tables */
int PST[2][7][128]; /* indexed by [TURN][PIECE][SQUARE]. Tables are white oriented. */

void generate_distance();
void generate_vertical_distance();
void generate_PST();

int static_evaluation(int draw);
int material(int side);
int mobility(int side);
int development(int side);
int pawn_structure(int side);
int positioning(int side);

#endif
