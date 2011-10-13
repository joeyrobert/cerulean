#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "board.h"
#include "util.h"
#include "move.h"
#include "perft.h"
#include "search.h"

void perft_test() {
    FILE *file;
    char line[200], *pch, fen[200];
    int depth;
    uint64_t actual;
    uint64_t expected;
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
        printf("\n\033[1m%s\033[0m\n", pch);
        pch = strtok (NULL, ";");

        while(pch != NULL) {
            board_set_fen(fen);
            depth = pch[1] - '0';
            expected = (uint64_t)atol(&pch[3]);            
            printf("%i %9li ", depth, expected);
            actual = perft_perft(depth);
            expected_moves += expected;
            actual_moves += actual;
            total_tests++;

            if(expected == actual) {
                printf("\033[32mPASS\033[0m");
                passed_tests++;
            } else
                printf("\033[31mFAIL (%llu)\033[0m", (long long unsigned int)actual);
            printf("\n");

            pch = strtok (NULL, ";");
        }
        printf("\n");
    }

    fclose(file);
    end = clock();
    timespan = (double)(end - start) / CLOCKS_PER_SEC;
    printf("# Passed       %10u\n", passed_tests);
    printf("# Total        %10u\n", total_tests);
    printf("Moves Actual   %10llu\n", (long long unsigned int)actual_moves);
    printf("Moves Expected %10llu\n", (long long unsigned int)expected_moves);
    printf("Time           %9.3fs\n", timespan);
    printf("Moves/s        %10.1f\n", actual_moves/timespan);
}

void search_test() {
    FILE *file;
    char line_tmp[200], line[200], *pch, fen[200], id[200];
    char expected_move_string[10], actual_move_string[10];
    unsigned total_tests, passed_tests, i, j;
    unsigned actual_move, expected_move;
    clock_t start, end;
    double timespan;

    char suites[][30] = {"suites/arasan12.epd",
                         "suites/bt2630.epd",
                         "suites/ecmgcp.epd",
                         "suites/eet.epd",
                         "suites/lapuce2.epd",
                         "suites/pet.epd",
                         "suites/sbd.epd",
                         "suites/wac.epd"};

    total_tests = 0; passed_tests = 0;
    start = clock();
    
    for(j = 0; j < 8; j++) {
      printf("\n\033[1m%s\033[0m\n", suites[j]);
      file = fopen(suites[j], "r");
      
      while(fgets(line_tmp, 200, file) != NULL) {        
          total_tests++;

          /* Get the best move */
          strcpy(line, line_tmp);
          pch = strtok(line, " ");
          for(i = 0; i < 5; i++)
              pch = strtok(NULL, " ");
          strcpy(expected_move_string, pch);
          expected_move_string[strlen(expected_move_string) - 1] = '\0';
          
          /* Get the fen board */
          strcpy(line, line_tmp);
          pch = strtok(line, ";");
          strcpy(fen, pch);

          /* Get the fen board */
          strcpy(line, line_tmp);
          pch = strtok(line, "\"");
          pch = strtok(NULL, "\"");
          strcpy(id, pch);

          board_set_fen(fen);
          actual_move = search_root();
          expected_move = find_short_algebraic_move(expected_move_string);
          
          printf("%-27s ", id);
          printf("%-8s ", expected_move_string);

          if(actual_move == expected_move) {
              printf("\033[32mPASS\033[0m");
              passed_tests++;
          } else {
              move_to_short_algebraic(actual_move, actual_move_string);
              printf("\033[31mFAIL (%s)\033[0m", actual_move_string);
          }

          printf("\n");
      }

      fclose(file);
    }

    end = clock();
    timespan = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n");
    printf("# Passed       %10u\n", passed_tests);
    printf("# Total        %10u\n", total_tests);
    printf("Time           %9.3fs\n", timespan);
}
