#include <stdlib.h>
#include <time.h>
#include "zobrist.h"

ZOBRIST zobrist_rand() {
    ZOBRIST random;
    unsigned i;
    random = 0;
    for(i = 0; i < 64; i++)
        random += (ZOBRIST)(rand()&1)<<i;
    return random;
}

void zobrist_fill() {
    unsigned i;
    srand(time(NULL));
    for(i = 0; i < 7*128; i++) {
        zobrist_w[i/128][i%128] = zobrist_rand();
        zobrist_b[i/128][i%128] = zobrist_rand();
    }
    zobrist_side = zobrist_rand();
    for(i = 0; i < 128; i++)
        zobrist_enpassant[i] = zobrist_rand();
    for(i = 0; i < 9; i++)
        zobrist_castling[i] = zobrist_rand();
}