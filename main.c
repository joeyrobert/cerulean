#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"
#include "perft.h"
#include "util.h"
#include "hash_table.h"
#include "zobrist.h"
#include "xboard.h"
#include "cli_tools.h"

int main(int argc, char **argv) {
    int cli_result = cli_run(argc, argv);

    if (cli_result >= 0)
        return cli_result;

    xboard_run();
    return 0;
}
