#ifndef EVALUATE_H 
#define EVALUATE_H

#define INFINITE 100000
#define DOUBLE_PAWN_PENALTY 20
#define ISOLATED_PAWN_PENALTY 20

int static_evaluation();
int pawn_structure();
int mobility();

#endif
