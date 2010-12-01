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
    engine_test();
    //board_set_fen("8/Pk6/8/8/8/8/6Kp/8 w - - 0 1");
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[3]);
    //engine_divide(1);
    //board_draw();
    //board_draw();
    //engine_test();
    //engine_divide(7);
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[3]);
    //engine_divide(3);
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[9]);
    //engine_divide(2);
    //count = gen_moves(moves);
    //describe_moves(moves, count);
    //board_add(moves[18]);
    //engine_divide(1);
    gets(fuck);
    return 0;
}
