#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "board.h"
#include "util.h"
#include "move.h"
#include "perft.h"

uint64_t perft_perft(int depth) {
    unsigned moves[256], count, i;
    hash_node* node;
    uint64_t total;
    if(depth == 0) return 1;

    node = hash_find(table, zobrist);
    if(node != NULL && node->depth == depth)
        return node->sub_nodes;
    
    count = gen_moves(moves);
    total = 0;

    for(i = 0; i < count; i++) {
        if(board_add(moves[i])) {
            total += perft_perft(depth - 1);
            board_subtract();
        }
    }

    hash_add_perft(table, zobrist, depth, total);
    return total;
}

void perft_divide(int depth) {
    char str[10];
    unsigned moves[256], count, i;
    double timespan;
    uint64_t total, perft;
    clock_t start, end;

    printf("Divide (depth %02i)\n", depth);
    printf("--------------------\n");

    start = clock();
    count = gen_moves(moves);
    total = 0, perft = 0;

    for(i = 0; i < count; i++) {
        if(board_add(moves[i])) {
            perft = perft_perft(depth-1);
            board_subtract();
            total += perft;

            move_to_string(moves[i], str);
            printf("%-5s     %10llu\n", str, (long long unsigned int)perft);
        }
    }
    
    end = clock();
    timespan = (double)(end - start) / CLOCKS_PER_SEC;
    printf("\nTotal     %10llu\n", (long long unsigned int)total);
    printf("Time      %9.3fs\n", timespan);
    printf("Moves/s %12.1f\n", total/timespan);
}
