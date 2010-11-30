#include <stdio.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"
#include "engine.h"
#include "util.h"

int main() {
    unsigned count, moves[256];
    char fuck[1];
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    engine_divide(4);
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[2]);
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[13]);
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[17]);
    //engine_divide(1);
    gets(fuck);
    return 0;
}
