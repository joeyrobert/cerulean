#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "board.h"
#include "util.h"
#include "move.h"

uint64_t engine_perft(unsigned depth) {
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
            total += engine_perft(depth - 1);
            board_subtract();
        }
    }

    hash_add(table, zobrist, depth, total);
    return total;
}

void engine_divide(int depth) {
    char str[6];
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
            move_to_string(moves[i], str);
            perft = engine_perft(depth-1);
            printf("%-5s     %10llu\n", str, perft);
            total += perft;
            board_subtract();
        }
    }
    
    end = clock();
    timespan = (double)(end - start) / CLOCKS_PER_SEC;
    printf("\nTotal     %10llu\n", total);
    printf("Time      %9.3fs\n", timespan);
    printf("Moves/s %12.1f\n", total/timespan);
}

void engine_test() {
    FILE *file;
    char line[200], *pch, fen[200];
    int depth;
    uint64_t actual;
    long expected;
    unsigned total_tests, passed_tests;
    uint64_t expected_moves, actual_moves;    
    clock_t start, end;
    double timespan;

    file = fopen("suites/perftsuite.epd", "r");
    total_tests = 0; passed_tests = 0;
    expected_moves = 0; actual_moves = 0;
    
    start = clock();
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

    fclose(file);
    end = clock();
    timespan = (double)(end - start) / CLOCKS_PER_SEC;
    printf("# Passed       %10u\n", passed_tests);
    printf("# Total        %10u\n", total_tests);
    printf("Moves Actual   %10llu\n", actual_moves);
    printf("Moves Expected %10llu\n", expected_moves);
    printf("Time           %9.3fs\n", timespan);
    printf("Moves/s        %10.1f\n", actual_moves/timespan);
}
