#include <stdio.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"
#include "engine.h"

int main() {
    unsigned long long result;
    unsigned count, moves[256];
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    count = gen_moves(moves);
    board_add(moves[6]);
    count = gen_moves(moves);
    board_add(moves[0]);
    board_draw();
    engine_divide(1);
    return 0;
}
