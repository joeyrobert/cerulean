#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "xboard.h"
#include "board.h"
#include "perft.h"
#include "test.h"
#include "util.h"
#include "zobrist.h"
#include "search.h"
#include "evaluate.h"

/* this is a very poor implementation of xboard */
void xboard_run() {
    char command[1000], move_string[6];
    unsigned value, move;
    uint64_t result;
    clock_t start, end;
    double timespan;
    setbuf(stdout, NULL);

    n_time = 0, n_otim = 0, out_of_opening = 0;    
    zobrist_fill();
    board_create_table();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    /* Generation tables */
    generate_PST();

    while(1) {
        if(fgets(command, 1000, stdin) == NULL) return;
        move = find_move(command);
        
        if(move != EMPTY) {
            board_add(move);            
			move = search_root();
			board_add(move);
			move_to_string(move, move_string);
			printf("move %s\n", move_string);
        } else if(!strncmp(command, "new", 3))
            board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        else if(!strncmp(command, "undo", 4))
            board_subtract();
        else if(!strncmp(command, "quit", 4) || !strncmp(command, "exit", 4))
            exit(0);
        else if(!strncmp(command, "white", 5))
            turn = WHITE;
        else if(!strncmp(command, "black", 5))
            turn = BLACK;
        else if(!strncmp(command, "time", 4))
            n_time = atoi(&command[5]);
        else if(!strncmp(command, "perfttest", 9))
            perft_test();
        else if(!strncmp(command, "searchtest", 10))
            search_test();
        else if(!strncmp(command, "eval", 4))
            static_evaluation(1);
        else if(!strncmp(command, "setboard", 8))
            board_set_fen(&command[9]);
        else if(!strncmp(command, "otim", 4))
            n_otim = atoi(&command[5]);
        else if(!strncmp(command, "display", 7) || !strncmp(command, "draw", 4))
            board_draw();
        else if(!strncmp(command, "go", 2)) {
            move = search_root();
            board_add(move);
            move_to_string(move, move_string);
            printf("move %s\n", move_string);
        } else if(!strncmp(command, "perft", 5)) {
            value = atoi(&command[6]);
            result = perft_perft(value);
            printf("%llu\n", (long long unsigned int)result);
        } else if(!strncmp(command, "divide", 6)) {
            value = atoi(&command[6]);
            perft_divide(value);
        } else if(!strncmp(command, "help", 4)) {
            printf("Commands\n");
            printf("--------\n");
            printf("display         Draws the board\n");
            printf("divide n        Divides the current board to a depth of n\n");
            printf("e2e4            Moves from the current position, and thinks\n");
            printf("go              Forces the engine to think\n");
            printf("undo            Subtracts the previous move\n");
            printf("new             Sets up the default board position\n");
            printf("setboard [FEN]  Sets the board to whatever you want\n");
            printf("perfttest       Runs the perft test suite\n");
            printf("searchtest      Runs the search test suite\n");
            printf("eval            Performs a static evaluation of the board and prints info\n");
            printf("white           Sets the active colour to WHITE\n");
            printf("black           Sets the active colour to BLACK\n");
            printf("time [INT]      Sets engine's time (in centiseconds)\n");
            printf("otim [INT]      Sets opponent's time (in centiseconds)\n");
            printf("exit            Exits the menu\n");
            printf("help            Gets you this magical menu\n\n");
        } else
            printf("Error (unknown command): %s", command);
    }
}
