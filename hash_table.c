#include <stdlib.h>
#include <string.h>
#include "hash_table.h"
#include "zobrist.h"

/* actual size = 2^size */
void hash_new(hash_table* table, unsigned size) {
    table->size = 2<<size;
    table->table = (hash_node*) malloc(sizeof(hash_node)*(table->size));
    memset(table->table, (uint64_t)NULL, sizeof(uint64_t)*(table->size));
}

hash_node* hash_find(hash_table* table, ZOBRIST key) {
    hash_node* node;
    node = &table->table[key % table->size];

    if(node->key == key)
        return node;
    return NULL;
}

void hash_add(hash_table* table, ZOBRIST key, unsigned depth, uint64_t sub_nodes) {
    hash_node* node;
    node = &table->table[key % table->size];
    node->key = key;
    node->depth = depth;
    node->sub_nodes = sub_nodes;
}
