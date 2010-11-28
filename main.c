#include <stdio.h>
#include <string.h>
#include "board.h"

int main() {
    board_new();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    //unsigned long long perft = engine_perft(3);
    //printf("Perft results: %llu\n", perft);

    engine_perft(5);
    return 0;
}
