#include <stdio.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"

int main() {
    piece_list list;
    unsigned moves[256], count;
    board_new();
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    count = gen_moves(moves);


    return 0;
}
