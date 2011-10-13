#ifndef TREE_H 
#define TREE_H

typedef struct tree_node {
    unsigned move;
    int children_size;
    struct tree_node* next; /* down always links to start of sublist */
    struct tree_node* down;
} tree_node;

tree_node* tree_new(unsigned move);

#endif
