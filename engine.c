#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "board.h"
#include "util.h"
#include "move.h"

uint64_t engine_perft(unsigned depth) {
    unsigned moves[256], count, i;
    uint64_t total;
    if(depth == 0) return 1;
    
    count = gen_moves(moves);
    total = 0;

    for(i = 0; i < count; i++) {
        if(board_add(moves[i])) {
            total += engine_perft(depth - 1);
            board_subtract();
        }
    }
    return total;
}

void engine_divide(int depth) {
    char str[5];
    unsigned moves[256], count, i;
    uint64_t total, perft;
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

void engine_test() {
    FILE *file;
    char line[200], *pch, fen[200];
    int depth;
    uint64_t actual;
    long expected;
    unsigned total_tests, passed_tests;
    uint64_t expected_moves, actual_moves;

    file = fopen("suites/perftsuite.epd", "r");
    total_tests = 0; passed_tests = 0;
    expected_moves = 0; actual_moves = 0;

    while(fgets(line, 200, file) != NULL) {
        pch = strtok(line, ";");
        strcpy (fen, pch);
        printf("%s\n", pch);
        pch = strtok (NULL, ";");

        while(pch != NULL) {
            board_set_fen(fen);
            depth = pch[1] - '0';
            expected = atol(&pch[3]);            
            printf("%i %li ", depth, expected);
            actual = engine_perft(depth);
            expected_moves += expected;
            actual_moves += actual;
            total_tests++;

            if(expected == actual) {
                printf(".\n");
                passed_tests++;
            } else
                printf("(received %llu)\n", actual);

            pch = strtok (NULL, ";");
        }
        printf("\n");
    }
    printf("\n%u/%u tests passed.\n", passed_tests, total_tests);
    printf("%llu moves run (%llu expected).\n", actual_moves, expected_moves);
}