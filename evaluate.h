#ifndef EVALUATE_H 
#define EVALUATE_H

#define INFINITE 100000
#define DOUBLE_PAWN_PENALTY 10
#define ISOLATED_PAWN_PENALTY 15
#define BACKWARD_PAWN_PENALTY 0

/* Distance related */
unsigned distance[240];
unsigned vertical_distance[240];

void generate_distance();
void generate_vertical_distance();

int static_evaluation();
int material();
int mobility();
int development();
int pawn_structure();

#endif
