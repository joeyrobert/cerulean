#include "evaluate.h"
#include "board.h"

unsigned piece_values[] = {0, 100, 315, 320, 500, 950, INFINITE};

int static_evaluation() {
    unsigned i, result;
    result = 0;

    for(i = 0; i < w_pieces.count; i++)
        result += piece_values[pieces[w_pieces.index[i]]];

    for(i = 0; i < b_pieces.count; i++)
        result -= piece_values[pieces[b_pieces.index[i]]];

    return turn*result;
}
