#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "xboard.h"
#include "board.h"
#include "perft.h"
#include "util.h"
#include "zobrist.h"

/* this is a very poor implementation of xboard */
void xboard_run() {
    char command[1000];
    unsigned value, move;
    uint64_t result;
    setbuf(stdout, NULL);

    time = 0; otim = 0;    
    zobrist_fill();
    board_create_table();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    while(1) {
        fgets(command, 1000, stdin);
        move = find_move(command);
        
        if(move)
            board_add(move);
        else if(!strncmp(command, "new", 3))
            board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        else if(!strncmp(command, "quit", 4) || !strncmp(command, "exit", 4))
            exit(0);
        else if(!strncmp(command, "white", 5))
            turn = WHITE;
        else if(!strncmp(command, "black", 5))
            turn = BLACK;
        else if(!strncmp(command, "time", 4))
            time = atoi(&command[5]);
        else if(!strncmp(command, "testsuite", 9))
            perft_test();
        else if(!strncmp(command, "setboard", 8))
            board_set_fen(&command[9]);
        else if(!strncmp(command, "otim", 4))
            otim = atoi(&command[5]);
        else if(!strncmp(command, "display", 7) || !strncmp(command, "draw", 4))
            board_draw();
        else if(!strncmp(command, "perft", 5)) {
            value = atoi(&command[6]);
            result = perft_perft(value);
            printf("%llu\n", result);
        } else if(!strncmp(command, "divide", 6)) {
            value = atoi(&command[6]);
            perft_divide(value);
        } else
            printf("Unknown command: %s", command);
    }
}
