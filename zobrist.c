#include <stdlib.h>
#include <time.h>
#include "zobrist.h"
#include "mt64.h"

ZOBRIST zobrist_rand() {
    return genrand64_int64();
}

void zobrist_fill() {
    uint64_t keys[5];
    unsigned i;

    keys[0] = 3514744284158856431;
    keys[1] = 31337;
    keys[2] = 320186092299510153;
    keys[3] = 8675309;
    keys[4] = UINT64_MAX;
    init_by_array64(keys, 5);
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
