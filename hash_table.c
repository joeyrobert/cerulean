#include <stdlib.h>
#include <string.h>
#include "hash_table.h"
#include "zobrist.h"

/* actual size = 2^(size+1) */
void hash_new(hash_table* table, unsigned size) {
    table->size = (uint64_t)1 << size;
    table->table = (hash_node*) malloc(sizeof(hash_node) * table->size);
    memset(table->table, 0, sizeof(hash_node) * table->size);
}

hash_node* hash_find(hash_table* table, ZOBRIST key) {
    hash_node* node = &table->table[key & (table->size - 1)];
    if (node->key == key)
        return node;
    return NULL;
}

void hash_add_perft(hash_table* table, ZOBRIST key, int depth, uint64_t sub_nodes) {
    hash_node* node = &table->table[key & (table->size - 1)];
    node->key = key;
    node->depth = depth;
    node->sub_nodes = sub_nodes;
}

void hash_add_move(hash_table* table, ZOBRIST key, int depth, int score, unsigned type, unsigned best_move) {
    hash_node* node = &table->table[key & (table->size - 1)];
    /* Depth replacement: prefer deeper entries; always replace on key collision */
    if (node->key == key && depth < node->depth)
        return;
    node->key = key;
    node->depth = depth;
    node->score = score;
    node->type = type;
    node->move = best_move;
}
