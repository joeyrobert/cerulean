#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "piece_list.h"
#include "engine.h"
#include "util.h"
#include "hash_table.h"
#include "zobrist.h"

int main() {
    hash_table *table;
    table = (hash_table*) malloc(sizeof(hash_table));
    hash_new(table, 23);
    
    zobrist_fill();
    return 0;
}
