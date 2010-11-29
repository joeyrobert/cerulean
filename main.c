#include <stdio.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"
#include "engine.h"

int main() {
    unsigned long long result;
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board_draw();
    result = engine_perft(3); 
    board_draw();
    
    return 0;
}
