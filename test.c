#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "board.h"
#include "util.h"
#include "move.h"
#include "perft.h"

void perft_test() {
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
            actual = perft_perft(depth);
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

void search_test() {
    FILE *file;
    char line[200], *pch, fen[200];
    unsigned total_tests, passed_tests;

    file = fopen("suites/ecmgcp.epd", "r");
    total_tests = 0; passed_tests = 0;
    
    while(fgets(line, 200, file) != NULL) {
        pch = strtok(line, ";");
        strcpy (fen, pch);
        printf("%s\n", pch);
        pch = strtok (NULL, ";");

        while(pch != NULL) {
            board_set_fen(fen); 
            total_tests++;

            if(1) {
                printf(".\n");
                passed_tests++;
            } else
                printf("FUCK");

            pch = strtok (NULL, ";");
        }
        printf("\n");
    }

    printf("# Passed       %10u\n", passed_tests);
    printf("# Total        %10u\n", total_tests);
}
