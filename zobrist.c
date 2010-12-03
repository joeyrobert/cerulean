#include <stdlib.h>
#include <time.h>
#include "zobrist.h"
#include "mt64.h"

ZOBRIST zobrist_rand() {
    return genrand64_int64();
}

void zobrist_fill() {
    unsigned i;
    init_genrand64(3514744284158856431);
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