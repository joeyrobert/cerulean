#include <stdio.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"

int main() {
    unsigned i;
    piece_list list;
    unsigned moves[256], count;
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board_draw();
    count = gen_moves(moves);

    for(i = 0; i < count; i++) {
        board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        board_add(moves[i]);
        board_draw();
    }
    
    return 0;
}
