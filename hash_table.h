#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "zobrist.h"

typedef struct {
    unsigned depth;
    ZOBRIST key;
    uint64_t sub_nodes;
} hash_node;

typedef struct {
    uint64_t size;
    hash_node *table;      /* this can grow */
} hash_table;

void hash_new(hash_table*, unsigned);
hash_node* hash_find(hash_table*, ZOBRIST);
void hash_add(hash_table*, ZOBRIST, unsigned, uint64_t);

#endif