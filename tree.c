#include <stdlib.h>
#include "tree.h"
#include "board.h"

tree_node* tree_new(unsigned move) {
    tree_node* node = (tree_node*)malloc(sizeof(tree_node*));
    node->move = move;
    node->children_size = EMPTY;
    node->down = NULL;
    node->next = NULL;
    return node;
}
