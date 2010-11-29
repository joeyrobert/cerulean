#include <stdio.h>
#include <string.h>
#include "board.h"
#include "util.h"


unsigned long long engine_perft(unsigned depth) {
    unsigned moves[256], count, i, before, after;
    unsigned long long total;
    if(depth == 0) return 1;
    
    count = gen_moves(moves);
    total = 0;    

    for(i = 0; i < count; i++) {
        before = board_debug();
        if(board_add(moves[i])) {
            total += engine_perft(depth - 1);
            board_subtract();
        }
        after = board_debug();

        if(after != before) {
            printf("FUCK");
        }
    }
    return total;
}

void engine_divide(int depth) {
    char str[5];
    unsigned moves[256], count, i;
    unsigned long long total, perft;
    printf("Divide at depth %02i\n", depth);
    printf("------------------\n");

    count = gen_moves(moves);
    total = 0, perft = 0;

    for(i = 0; i < count; i++) {
        if(board_add(moves[i])) {
            move_to_string(moves[i], str);
            perft = engine_perft(depth-1);
            printf("%s    %10llu\n", str, perft);
            total += perft;
            board_subtract();
        }
    }
    printf("Total   %10llu\n", total);
}
