#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "zobrist.h"

#define HASH_EXACT 1
#define HASH_ALPHA 2
#define HASH_BETA  3

typedef struct {
    unsigned depth;
    ZOBRIST key;
    uint64_t sub_nodes;
    unsigned move;
    unsigned type;      /* HASH_ALPHA, HASH_BETA or HASH_EXACT */
    int score;
} hash_node;

typedef struct {
    uint64_t size;
    hash_node *table;   /* this can grow */
} hash_table;

void hash_new(hash_table*, unsigned);
hash_node* hash_find(hash_table*, ZOBRIST);
void hash_add_perft(hash_table*, ZOBRIST, unsigned, uint64_t);
void hash_add_move(hash_table* table, ZOBRIST key, unsigned depth, int score, unsigned type);

#endif
