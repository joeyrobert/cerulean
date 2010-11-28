#include <stdio.h>
#include <string.h>
#include "board.h"

unsigned long long engine_perft(int depth) {
    if(depth == 0) return 1;

    unsigned moves[256] = { 0 };
    int count = (int)gen_moves(moves);
    unsigned long long total = 0;
    char before[200] = "", after[200] = "";

    int i;
    for(i = 0; i < count; i++) {
        board_debug(before);

        if(board_add(moves[i])) {
            char str[5];
            move_to_string(moves[i], str);
            total += engine_perft(depth - 1);
            board_subtract();
        }

        board_debug(after);
        if(strcmp(before, after) != 0)
            printf("%u\n", moves[i]);
    }
    return total;
}

void engine_divide(int depth) {
    printf("Divide at depth %02i\n", depth);
    printf("------------------\n");

    unsigned moves[256] = { 0 };
    unsigned move_count = gen_moves(moves);
    unsigned long long total = 0, perft = 0;
    char before[200] = "", after[200] = "";

    int i;
    for(i = 0; i < move_count; i++) {
        board_debug(before);
        if(board_add(moves[i])) {
            char str[5]; move_to_string(moves[i], str);
            perft = engine_perft(depth-1);
            printf("%s    %10llu\n", str, perft);
            total += perft;
            board_subtract();
        }

        board_debug(after);
        if(strcmp(before, after) != 0)
            printf("%u\n", moves[i]);
    }
    printf("Total   %10llu\n", total);
}
