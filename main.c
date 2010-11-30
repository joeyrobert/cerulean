#include <stdio.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"
#include "engine.h"
#include "util.h"

int main() {
    char fuck[50];
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    engine_divide(4);
    gets(fuck);
    return 0;
}
